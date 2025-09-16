/*
 * Copyright 2023 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Generalize the types of program locations as much as possible, both to
// eliminate unnecessarily refined types from the type section and also to
// weaken casts that cast to unnecessarily refined types. If the casts are
// weakened enough, they will be able to be removed by OptimizeInstructions.
//
// While this pass has the benefits mentioned above, unrefining types can still
// be harmful, e.g. if the VM might have benefited from more precise types after
// it does runtime inlining. For that reason it may be useful to run this pass
// near the end of the optimization pipeline, and optimize after it: doing so
// may allow removal of some unnecessary casts, and the follow optimizations
// will restore some amount of refinement (without adding casts back).
//
// To do this, we generate a global graph of all type constraints in the module,
// then flow the constraints to a fixed point to see what the minimal types are
// that do not break validation or change program behavior.
//
// In more detail:
//
//  * We build the basic graph using SubtypingDiscoverer which finds proper
//    subtyping constraints, like the value of a local.set being a subtype of
//    the local's type.
//  * We then add equality constraints, things that must be identical in the IR,
//    like a global.get being the exact type of the global.
//  * We do not define all such equality constraints. We *must* provide relevant
//    equality constraints for things we intend to optimize (to optimize
//    globals safely, we must ensure equality of global.gets to the global's
//    type, as mentioned before), but rather than depend on having all possible
//    equality constraints present, we use the following simple rule: When a
//    thing is unconstrained, we keep its type fixed. That is, if nothing tells
//    us that a particular location in the module must be a subtype of
//    something else, then we we do not know enough to safely optimize it, and
//    we (safely) leave its type unmodified.
//    TODO Each time we do so, we are leaving a possible optimization
//         opportunity on the table, so this can be incrementally improved. For
//         the work required to optimize a new type of thing, see the logic
//         below for Globals:
//         * Visit them in Collector.
//         * Handle imported and exported versions of them.
//         * Add equality constraints.
//         * Update them in update().
//
// The "casts" variant of this pass only generalizes up to the point that
// actually allows removal of a cast. This attempts a better tradeoff, where we
// only generalize when we accomplish something we know is useful for runtime
// (removing a cast), and hope that by doing so we do not add even more
// downsides (which are possible, in theory, with VM inlining that could have
// benefited from more refined types to remove casts then).
//
// TODO: In closed world we could not treat externally-visible types as roots,
//       e.g., we could generalize an exported (ref $foo) to an anyref.
//

#include "analysis/lattice.h"
#include "analysis/lattices/valtype.h"
#include "ir/iteration.h"
#include "ir/locations.h"
#include "ir/lubs.h"
#include "ir/properties.h"
#include "ir/subtype-exprs.h"
#include "ir/utils.h"
#include "pass.h"
#include "support/small_set.h"
#include "support/unique_deferring_queue.h"
#include "wasm-traversal.h"
#include "wasm.h"

namespace wasm {

namespace {

int DEBUG = 0;

// We will perform our computation over the ValType lattice, that is, over
// types, using the element from there.
using Element = analysis::ValType::Element;

// The data we gather from each function, as we process them in parallel. Later
// this will be merged into a single big graph.
struct CollectedFuncInfo {
  // We collect two types of information: 1. Relative constraints, where we find
  // links between points in the graph, like a global.get that feeds into a
  // local.set: the global must be refined enough to fit in the local.
  //
  // Rarely are there duplicates in this list, so we leave deduplication for
  // later when we merge all function infos anyhow.
  //
  // Links are in the order of subtype-exprs calls, that is, {sub, super} pairs,
  // where sub must be a subtype of super.
  std::vector<LocationLink> links;

  // 2. The second constraint we gather are absolute ones, "roots". E.g., ref.eq
  // imposes a root constraint of eqref on both children.
  //
  // The vector here is of the location of the root and the constraint there (as
  // above, we do not deduplicate here).
  std::vector<std::pair<Location, Element>> roots;
};

// Only reference types can be generalized; nothing else is relevant for this
// pass.
bool isRelevant(Type type) {
  return type.isRef();
}

// Collect subtyping constraints for one CollectedFuncInfo.
struct Collector
  : ControlFlowWalker<Collector, SubtypingDiscoverer<Collector>> {

  using Super = ControlFlowWalker<Collector, SubtypingDiscoverer<Collector>>;

  CollectedFuncInfo& info;
  PassOptions& passOptions;

  Collector(CollectedFuncInfo& info, PassOptions& passOptions) : info(info), passOptions(passOptions) {}

  // The set of expressions we added a constraint to in the last visit(). We
  // will use this to find expressions that are unconstrained. TODO text?
  SmallSet<Expression*, 3> justConstrained;

  // Note an expression to a given set, if it has a relevant type.
  void noteConstrainedExpr(Location loc) {
    if (auto* exprLoc = std::get_if<ExpressionLocation>(&loc)) {
      justConstrained.insert(exprLoc->expr);
    }
  }

  void root(Location loc, Type value) {
    // We can ignore irrelevant types, either in the type we are told to apply,
    // or the current type (if the current type is unreachable, this is dead
    // code).
    if (!isRelevant(value) || !isRelevant(Locations::getType(loc, *getModule()))) {
      return;
    }

    info.roots.emplace_back(loc, value);

    // Roots are constrained by their types.
    noteConstrainedExpr(loc);
  }

  void link(Location sub, Location super) {
    if (!isRelevant(Locations::getType(sub, *getModule())) || !isRelevant(Locations::getType(super, *getModule()))) {
      return;
    }

    info.links.emplace_back(sub, super);

    // |sub| is constrained to be a subtype of the super.
    noteConstrainedExpr(sub);
  }

  // Override the normal scanning to add a hook right after visiting. This will
  // allow us to see which children are constrainted during the visit, and hence which remain unconstrainted.
  static void scan(Collector* self, Expression** currp) {
    self->pushTask(Collector::doPostVisit, currp);                                 \

    Super::scan(self, currp);
  }

  static void doPostVisit(Collector* self, Expression** currp) {
    auto* curr = *currp;
    for (auto* child : ChildIterator(curr)) {
      if (isRelevant(child->type) && !self->justConstrained.count(child)) {
        if (DEBUG) std::cerr << "Unconstrained child " << *child << "\n";
        self->root(Locations::get(child), child->type);
      }
    }

    // Add fallthrough constraints. SubtypingDiscoverer handles subtyping, not
    // equalities like these, so we must add them.
    //
    // When computing the fallthrough we allow tees and br_ifs - everything is
    // relevant - and we can ignore effects, as we just care about the flow of
    // types, not values.
    if (auto* fallthrough =
          Properties::getImmediateFallthrough(curr,
                                              self->passOptions,
                                              *self->getModule(),
                                              Properties::FallthroughBehavior::AllowTeeBrIf,
                                              Properties::FallthroughEffectBehavior::Ignore)) {
      // If the type differs then this is something like ref.as_non_null: not
      // a pure fallthrough but one that changes the type as it flows through.
      // We leave such things for specific handling below.
      if (fallthrough != curr && fallthrough->type == curr->type) {
        self->link(Locations::get(fallthrough), Locations::get(curr));
      }
    }

    self->justConstrained.clear();
  }

  // Forces two locations to be equal in value, like a local.get and the type
  // of the local. These are not subtyping constraints but requirements of the
  // IR, and they are therefore not noted as constraints: a local.get may be
  // forced to be equal to the local's type, but if its parent does not
  // constrain it, it is unconstrained.
  void enforceEquality(Location a, Location b) {
    info.links.emplace_back(a, b);
    info.links.emplace_back(b, a);
  }

  // SubtypingDiscoverer hooks. These implement all true subtyping constraints
  // in the wasm, for example, that the value written to a global must be a
  // subtype of the global. We override some of these below where we want to be
  // more flexible, but SubtypingDiscoverer forms the sound foundation of
  // enforcing all constraints.

  // Constraints on type themselves do not interest us.
  void noteSubtype(Type, Type) {}
  void noteSubtype(HeapType, HeapType) {}
  void noteSubtype(Type, Expression*) {}
  void noteSubtype(Type, const Location&) {}

  // An absolute (root) constraint, e.g. from ref.eq.
  void noteSubtype(Expression* sub, Type super) { // XXX can we remove all Expr* ones?
    if (isRelevant(super)) {
      root(Locations::get(sub), super);
    }
  }
  void noteNonFlowSubtype(Expression* sub, Type super) {
    noteSubtype(sub, super);
  }

  // A relative (link) constraint, e.g. between a block and its last child.
  void noteSubtype(Expression* sub, Expression* super) {
    link(Locations::get(sub), Locations::get(super));
  }

  void noteSubtype(const Location& sub, Type super) {
    root(sub, super);
  }
  void noteSubtype(const Location& sub, const Location& super) {
    link(sub, super);
  }
  void noteNonFlowSubtype(const Location& sub, Type super) {
    root(sub, super);
  }

  // See below for how we optimize ref.cast specifically. TODO other casts too.
  void noteCast(HeapType, HeapType) {}
  void noteCast(Expression*, Type) {}
  void noteCast(Expression*, Expression*) {}
  void noteCast(const Location&, Type) {}
  void noteCast(const Location&, const Location&) {}

  // Override subtype-exprs where we want more precise representation of the
  // flow of constraints.

  // TODO: maybe make struct-utils use Locations, and add a helper on top that
  //       un-locates to the old API for people that want simplicity? or use
  //       locations in current users? Or, use locations there, but also add the
  //       high level hooks to translate to non-location stuff, so it gets
  //       immediately folded away in struct-utils.
  void visitLocalGet(LocalGet* curr) {
    // local.get's type must match the local type.
    if (isRelevant(curr->type)) { // XXX remove all these and do Locations::getType in root() link()
      enforceEquality(LocalLocation{getFunction(), curr->index},
                      Locations::get(curr));
    }
  }

  void visitLocalSet(LocalSet* curr) {
    // Override the parent behavior of an absolute constraint of "value is a
    // subtype of ${current type of local}" with a link to the local, i.e., a
    // relative constraint.
    link(Locations::get(curr->value), LocalLocation{getFunction(), curr->index});

    // As with local.get, link us to the local. Note that a tee can be more
    // refined than the local type, but this is necessary to ensure that we pick
    // up constraints from the parent (e.g. if the parent is a ref.eq, that
    // means the local must be ref.eq). XXX do we need this? can it be in the parent SubtypingDiscoverer?
    if (isRelevant(curr->type)) { // Can be isTee() after move isRelevants up
      enforceEquality(LocalLocation{getFunction(), curr->index},
                      Locations::get(curr)); // XXX can this be one-sided?
    }
  }

  void visitGlobalGet(GlobalGet* curr) {
    // global.get's type must match the global type.
    if (isRelevant(curr->type)) {
      enforceEquality(GlobalLocation{curr->name},
                      Locations::get(curr));
    }
  }

  void noteEmptyConstraint(Expression* value) {
    // Not this as constrained, so that we do not think it is unconstrained.
    if (isRelevant(value->type)) {
      justConstrained.insert(value);
    }
  }

  void visitDrop(Drop* curr) {
    // Drop imposes no constraints on the input. It is the only expression
    // that truly does so, and therefore requires special treatment: Since we
    // consider things that have no constraints as unchangeable (see above), we
    // must make a note to allow optimization here, by applying an empty
    // constraint.
    if (curr->value->type.isRef()) {
      noteEmptyConstraint(curr->value);
    }
  }

  void visitRefIsNull(RefIsNull* curr) {
    // The input must be a reference, but nothing more, so SubtypingDiscoverer
    // does nothing for it. We treat it as a drop.
    noteEmptyConstraint(curr->value);
  }

  void visitRefCast(RefCast* curr) {
    if (passOptions.trapsNeverHappen) {
      // The type we cast to is not a constraint, when traps cannot happen.
      noteEmptyConstraint(curr->ref);
    } else {
      // Otherwise, we cannot generalize a cast as it might no longer fail.
      root(Locations::get(curr), curr->type);
      // If this is an exact cast, we must keep it "trivial" if custom
      // descriptors are not enabled (see visitRefCast/Test in wasm-validator).
      // For now, just force the input to remain as before. TODO
      root(Locations::get(curr->ref), curr->ref->type);
    }
  }

  void visitFunction(Function* func) {
    Super::visitFunction(func);

    // Root the parameters and results. TODO optimize them
    for (Index i = 0; i < func->getNumParams(); ++i) {
      root(LocalLocation{func, i}, func->getLocalType(i));
    }
    for (Index i = 0; i < func->getResults().size(); ++i) {
      root(ResultLocation{func, i}, func->getResults()[i]);
    }
  }
};

struct TypeGeneralizing : public Pass {
  // Only alters types.
  bool requiresNonNullableLocalFixups() override { return false; }

  // Whether we are the variant of the pass that focuses on removing casts.
  bool casts;

  TypeGeneralizing(bool casts) : casts(casts) {}

  // The computed element for each location.
  std::unordered_map<Location, Element> locationValues;

  void run(Module* module) override {
    if (!module->features.hasGC()) {
      return;
    }

    if (getenv("DEBUG")) DEBUG = 1;

    auto* runner = getPassRunner();

    // Collect information from each function.
    ModuleUtils::ParallelFunctionAnalysis<CollectedFuncInfo> analysis(
      *module, [&](Function* func, CollectedFuncInfo& info) {
        if (!func->imported()) {
          Collector collector(info, runner->options);
          collector.walkFunctionInModule(func, module);
        }
      });

    // Also walk the global module code (for simplicity, also add it to the
    // function map, using a "function" key of nullptr).
    auto& moduleInfo = analysis.map[nullptr];
    Collector moduleCollector(moduleInfo, runner->options);
    moduleCollector.walkModuleCode(module);
    moduleCollector.walkModuleLevel(module);

    // Add roots for exports. TODO: not in closed world?
    moduleCollector.setModule(module);
    for (auto& exp : module->exports) {
      auto name = exp->getInternalName();
      if (exp->kind == ExternalKind::Global) {
        moduleCollector.root(GlobalLocation{*name}, module->getGlobal(*name)->type);
      }
    }

    // Add roots for imports. TODO: not in closed world?
    for (auto& global : module->globals) {
      if (global->imported()) {
        moduleCollector.root(GlobalLocation{global->name}, global->type);
      }
    }

    // All our operations will be on the lattice of types.
    analysis::ValType lattice;

    // Merge the function information into a single large graph, deduplicating
    // as we go, and preparing the initial list of work (the roots).
    std::unordered_set<LocationLink> links;
    UniqueDeferredQueue<Location> work;

    auto addRoot = [&](Location root, Element value) { // TODO inline
      if (DEBUG) std::cerr << "made root " << root << " to " << value << '\n';
      assert(isRelevant(value));
      lattice.meet(locationValues[root], value);
      if (isRelevant(locationValues[root])) {
        work.push(root);
      }
    };

    for (auto& [func, info] : analysis.map) {
      for (auto& link : info.links) {
        links.insert(link);
      }
      for (auto& [root, value] : info.roots) {
        addRoot(root, value);
      }
    }

    // We no longer need the function-level info.
    analysis.map.clear();

    // Build the data structures for flowing of information, a graph of
    // information about the links for a location.
    struct LocationLinks {
      // To avoid ambiguity in notation, we will talk about links going in the
      // "forward" and "back" directions, which follow the flow of values:
      // values flow forward from children to parents.
      //
      // Most optimizations flow in the "forward" direction (following value
      // flow), and that is what SubtypingDiscoverer built up for us. However,
      // our main work here is in the opposite direction: a parent imposes a
      // constraint on the child that will send it a value, forcing the child to
      // be refined enough to fit.
      SmallVector<Location, 2> forward, back;
    };
    std::unordered_map<Location, LocationLinks> graph;
    for (auto& [from, to] : links) {
      // Convert the [from, to] in the sense of SubtypingDiscoverer - which go
      // in the "forward" direction - to our graph.
      graph[from].forward.push_back(to);
      graph[to].back.push_back(from);
    }

    if (DEBUG) {
      std::cerr << "\nGRAPH:\n";
      for (auto& [loc, links] : graph) {
        std::cerr << loc << " sends to back: [";
        bool first = true;
        for (auto t : links.back) {
          if (first) {
            first = false;
          } else {
            std::cerr << ',';
          }
          std::cerr << "\n  " << t;
        }
        std::cerr << "] and sends to forward: [";
        first = true;
        for (auto t : links.forward) {
          if (first) {
            first = false;
          } else {
            std::cerr << ',';
          }
          std::cerr << "\n  " << t;
        }
        std::cerr << "]\n\n";
      }
    }

    // Flow while changes happen.
    while (!work.empty()) {
      auto loc = work.pop();
      if (DEBUG) std::cerr << "working on " << loc << '\n';
      auto value = locationValues[loc];
      if (!isRelevant(value)) {
        continue;
      }
      for (auto target : graph[loc].back) {
        if (lattice.meet(locationValues[target], value)) {
          if (isRelevant(locationValues[target])) {
            if (DEBUG) std::cerr << "  met target " << target << " to " << value << '\n';
            // Flow onward.
            work.push(target);
          }
        }
      }
    }

    if (DEBUG) std::cerr << "\nCOMPUTED locationValues:\n";
    for (auto& [loc, value] : locationValues) {
      if (DEBUG) std::cerr << loc << " => " << value << '\n';
    }

    if (casts) {
      // Thus far we have found the *maximal* generalization we can accomplish,
      // that is, how far we can go with generalizing when we try as hard as
      // possible. In the "casts" variant, we now find places where
      // generalization can help remove a cast, and then we only generalize just
      // enough for that.

      // Stash the maximally general findings before we compute the new ones.
      auto maximalLocationValues = std::move(locationValues);

      // Find casts that we might remove, and the types we want to generalize
      // them to.
      std::vector<std::pair<Location, Element>> casts;

      for (auto& [loc, value] : maximalLocationValues) {
        if (auto* exprLoc = std::get_if<ExpressionLocation>(&loc)) {
          if (auto* cast = exprLoc->expr->dynCast<RefCast>()) {
            // A cast becomes removable (in trapsNeverHappen, at least) when it
            // can be generalized to something that makes its input always
            // succeed:
            //
            //   (ref.cast (ref $bar) (call $get_foo))
            //
            // If the call returns (ref null $foo), and we can generalize this
            // cast to (ref null $foo), then it is unneeded.
            if (Type::isSubType(cast->ref->type, value)) {
              // We only need to generalize as much as the cast's value (the
              // actual maximal generalization may be larger).
              casts.emplace_back(loc, cast->ref->type);
            }
          }
        }
      }

      if (casts.empty()) {
        return;
      }

      // Starting from the casts we found, generalize all the other things we
      // need, so that they are valid after generalization. This is a flow in
      // the opposite direction of before, where children force parents to be
      // less refined than before.
      UniqueDeferredQueue<Location> work;
      for (auto& [loc, value] : casts) {
        work.push(loc);
        locationValues[loc] = value;
        if (DEBUG) std::cerr << "cast " << loc << " set to " << value << '\n';
      }
      while (!work.empty()) {
        auto loc = work.pop();
        auto value = locationValues[loc];
        if (DEBUG) std::cerr << "casts working on " << loc << " with " << value << '\n';
        if (!isRelevant(value)) {
          continue;
        }
        for (auto target : graph[loc].forward) {
          auto& targetValue = locationValues[target];
          if (targetValue == Type::none) {
            // This is the first time we flow to here, get the initial value.
            targetValue = Locations::getType(target, *module);
          }
          if (lattice.join(targetValue, value)) {
            if (isRelevant(targetValue)) {
              if (DEBUG) std::cerr << "  joined target " << target << " to " << value << '\n';
              // Flow onward.
              work.push(target);
            }
          }
        }
      }

      if (DEBUG) std::cerr << "\nCOMPUTED CAST locationValues:\n";
      for (auto& [loc, value] : locationValues) {
        if (DEBUG) std::cerr << loc << " => " << value << '\n';
      }
    }

    // Apply |locationValues| to the module.
    update(module);
  }

  // Apply |locationValues| to the IR.
  void update(Module* module) {
    struct Updater : public WalkerPass<PostWalker<Updater, UnifiedExpressionVisitor<Updater>>> {
      bool isFunctionParallel() override { return true; }

      TypeGeneralizing& parent;

      Updater(TypeGeneralizing& parent) : parent(parent) {}

      std::unique_ptr<Pass> create() override {
        return std::make_unique<Updater>(parent);
      }

      // Given a location and its type in the IR, update the type.
      void updateType(Location loc, Type& type) {
        if (!type.isRef()) {
          return;
        }

        // If there is no info at all, that means no constraints, and we can use
        // the top type.
        [[maybe_unused]] auto old = type;
        auto iter = parent.locationValues.find(loc);
        if (iter == parent.locationValues.end()) {
          if (parent.casts) {
            // In the casts mode we do not do unnecessary generalizations: only
            // the things we found to help with casts, are applied.
            return;
          }
          type = type.with(type.getHeapType().getTop()).with(Nullable);
        } else {
          // Do not apply irrelevant types like unreachable.
          auto newType = iter->second;
          if (!isRelevant(newType)) {
            return;
          }
          // We should only ever generalize types, not refine them. Unreachable
          // code can make us try to alter a type wrongly.
          if (!Type::isSubType(old, newType)) {
            return;
          }
          type = newType;
        }
        if (DEBUG) std::cerr << "Updated " << loc << " from " << old << " to " << type << '\n';
      };

      void visitExpression(Expression* curr) {
        // Apply generalized types to all things we can. We must avoid updating
        // things with fixed types like constants (e.g. ref.func), and things we
        // do not yet optimize.
        // TODO: tuples
        switch (curr->_id) {
          // Things that flow/fall through values, which we can just update to
          // their new types.
          case Expression::BlockId:
          case Expression::IfId:
          case Expression::LoopId:
          case Expression::SelectId:
          case Expression::TryId:
          case Expression::TryTableId:
          case Expression::LocalSetId:
          case Expression::BreakId:
          // Things we optimize.
          case Expression::LocalGetId:
          case Expression::GlobalGetId:
          case Expression::RefCastId:
            updateType(ExpressionLocation{curr, 0}, curr->type);
            break;
          default:
            break;
        }
      }

      void visitFunction(Function* func) {
        // Apply types to the vars (but not params).
        auto base = func->getVarIndexBase();
        for (Index i = 0; i < func->getNumVars(); ++i) {
          updateType(LocalLocation{func, base + i}, func->vars[i]);
        }
      }
    };

    Updater updater(*this);
    PassRunner runner(module);
    updater.run(&runner, module);
    updater.walkModuleCode(module);

    for (auto& global : module->globals) {
      updater.updateType(GlobalLocation{global->name}, global->type);
    }

    // ReFinalize to propagate changes and make things consistent. This can end
    // up refining types, but is necessary for correctness in general (and it
    // only works locally, at least, which preserves global generalizations).
    ReFinalize().run(getPassRunner(), module); // utils.h
  }
};

} // anonymous namespace

Pass* createTypeGeneralizingPass() { return new TypeGeneralizing(false); }

Pass* createTypeGeneralizingCastsPass() { return new TypeGeneralizing(true); }

} // namespace wasm
