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

Location getLocation(Expression* curr) {
  // TODO: tuples
  return ExpressionLocation{curr, 0};
}

// Collect subtyping constraints for one CollectedFuncInfo.
struct Collector
  : ControlFlowWalker<Collector, SubtypingDiscoverer<Collector>> {

  using Super = ControlFlowWalker<Collector, SubtypingDiscoverer<Collector>>;

  CollectedFuncInfo& info;
  PassOptions& passOptions;

  Collector(CollectedFuncInfo& info, PassOptions& passOptions) : info(info), passOptions(passOptions) {}

  bool isRelevant(Type type) {
    // Only reference types can be generalized.
    return type.isRef();
  }

  // Override the normal scanning to add a hook right after visiting. This will
  // allow us to see which children are constrainted during the visit, and hence which remain unconstrainted.
  static void scan(Collector* self, Expression** currp) {
    self->pushTask(Collector::doPostVisit, currp);                                 \

    Super::scan(self, currp);
  }

  SmallSet<Expression*, 3> justConstrained; // TODO move all this up to top

  static void doPostVisit(Collector* self, Expression** currp) {
    auto* curr = *currp;
    for (auto* child : ChildIterator(curr)) {
      if (self->isRelevant(child->type) && !self->justConstrained.count(child)) {
        if (DEBUG) std::cerr << "Unconstrained child " << *child << "\n";
        self->root(getLocation(child), child->type);
      }
    }

    // Add fallthrough constraints. SubtypingDiscoverer handles subtyping, not
    // equalities like these, so we must add them.
    if (self->isRelevant(curr->type)) {
      if (auto* fallthrough =
            Properties::getImmediateFallthrough(curr,
                                                self->passOptions,
                                                *self->getModule())) {
        // If the type differs then this is something like ref.as_non_null: not
        // a pure fallthrough but one that changes the type as it flows through.
        // We leave such things for specific handling below.
        if (fallthrough->type == curr->type) {
          self->link(getLocation(fallthrough), getLocation(curr));
        }
      }
    }

    self->justConstrained.clear();
  }

  // Note an expression to a given set, if it has a relevant type.
  void noteConstrainedExpr(Location loc) {
    if (auto* exprLoc = std::get_if<ExpressionLocation>(&loc)) {
      if (isRelevant(exprLoc->expr->type)) {
        justConstrained.insert(exprLoc->expr);
      }
    }
  }

  void root(Location loc, Type value) {
    info.roots.emplace_back(loc, value);

    // Roots are constrained by their types.
    noteConstrainedExpr(loc);
  }

  void link(Location sub, Location super) {
    info.links.emplace_back(sub, super);

    // |sub| is constrained to be a subtype of the super.
    noteConstrainedExpr(sub);
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

  // An absolute (root) constraint, e.g. from ref.eq.
  void noteSubtype(Expression* sub, Type super) {
    if (isRelevant(super)) {
      root(getLocation(sub), super);
    }
  }
  void noteNonFlowSubtype(Expression* sub, Type super) {
    noteSubtype(sub, super);
  }

  // A relative (link) constraint, e.g. between a block and its last child.
  void noteSubtype(Expression* sub, Expression* super) {
    if (isRelevant(sub->type) && isRelevant(super->type)) {
      link(getLocation(sub), getLocation(super));
    }
  }

  // Casts impose no requirements on us.
  void noteCast(HeapType, HeapType) {}
  void noteCast(Expression*, Type) {}
  void noteCast(Expression*, Expression*) {}

  // Override subtype-exprs where we want more precise representation of the
  // flow of constraints.

  void visitLocalGet(LocalGet* curr) {
    // local.get's type must match the local type.
    if (isRelevant(curr->type)) { // XXX remove all these and do getLocationType in root() link()
      enforceEquality(LocalLocation{getFunction(), curr->index},
                      getLocation(curr));
    }
  }

  void visitLocalSet(LocalSet* curr) {
    // Override the parent behavior of an absolute constraint of "value is a
    // subtype of ${current type of local}" with a link to the local, i.e., a
    // relative constraint.
    link(getLocation(curr->value), LocalLocation{getFunction(), curr->index});

    // As with local.get, link us to the local. Note that a tee can be more
    // refined than the local type, but this is necessary to ensure that we pick
    // up constraints from the parent (e.g. if the parent is a ref.eq, that
    // means the local must be ref.eq).
    if (isRelevant(curr->type)) { // Can be isTee() after move isRelevants up
      enforceEquality(LocalLocation{getFunction(), curr->index},
                      getLocation(curr)); // XXX can this be one-sided?
    }
  }

  void visitGlobalGet(GlobalGet* curr) {
    // global.get's type must match the global type.
    if (isRelevant(curr->type)) {
      enforceEquality(GlobalLocation{curr->name},
                      getLocation(curr));
    }
  }

  void visitGlobalSet(GlobalSet* curr) {
    // Similar to LocalSet.
    link(getLocation(curr->value), GlobalLocation{curr->name});
  }

  void visitGlobal(Global* global) {
    // Override the parent to specify GlobalLocation (and not just a type).
    if (global->init) {
      link(getLocation(global->init), GlobalLocation{global->name});
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
      root(getLocation(curr), curr->type);
    }
  }

  void visitFunction(Function* func) {
    Super::visitFunction(func);

    // Root the parameters. TODO optimize them
    for (Index i = 0; i < func->getNumParams(); ++i) {
      root(LocalLocation{func, i}, func->getLocalType(i));
    }
  }
};

struct TypeGeneralizing : public Pass {
  // Only alters types.
  bool requiresNonNullableLocalFixups() override { return false; }

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
    moduleCollector.walkModuleLevel(module);

    // Add roots for exports. TODO: not in closed world?
    for (auto& exp : module->exports) {
      auto name = exp->getInternalName();
      if (exp->kind == ExternalKind::Global) {
        moduleInfo.roots.emplace_back(GlobalLocation{*name}, module->getGlobal(*name)->type);
      }
    }

    // Add roots for imports. TODO: not in closed world?
    for (auto& global : module->globals) {
      if (global->imported()) {
        moduleInfo.roots.emplace_back(GlobalLocation{global->name}, global->type);
      }
    }

    // All our operations will be on the lattice of types.
    analysis::ValType lattice;

    // Merge the function information into a single large graph, deduplicating
    // as we go, and preparing the initial list of work (the roots).
    LocationLinkGraph links;
    std::unordered_set<Location> roots;
    UniqueDeferredQueue<Location> work;

    auto addRoot = [&](Location root, Element value) {
      if (DEBUG) std::cerr << "made root " << root << " to " << value << '\n';
      lattice.meet(locationValues[root], value);
      roots.insert(root);
      work.push(root);
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

    // Our updates are in the reverse of the normal direction of the flow of
    // information. E.g. when we see (global.set $g (value)) we do not send the
    // value to the GlobalLocation, but rather look back and say that the
    // value's sources must be refined enough to fit in the global's type.
    links.reverse();

    // Get the flow-efficient sorted graph.
    auto sortedGraph = links.getSortedGraph();
    if (DEBUG) {
      std::cerr << "\nGRAPH:\n";
      for (auto& [loc, targets] : sortedGraph) {
        std::cerr << loc << " sends to targets: [";
        bool first = true;
        for (auto t : targets) {
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
      for (auto target : sortedGraph[loc]) {
        if (lattice.meet(locationValues[target], value)) {
          if (DEBUG) std::cerr << "  met target " << target << " to " << value << '\n';
          // Flow onward.
          work.push(target);
        }
      }
    }

    if (DEBUG) std::cerr << "\nCOMPUTED locationValues:\n";
    for (auto& [loc, value] : locationValues) {
      if (DEBUG) std::cerr << loc << " => " << value << '\n';
    }

    // TODO: A "cast" variant of this pass, where after finding the maximal
    //       generalizations, we find places where the generalization helps
    //       remove a cast, and then undo all the ones that don't.

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
          type = type.with(type.getHeapType().getTop()).with(Nullable);
        } else {
          type = iter->second;
        }
        if (DEBUG) std::cerr << "Updated " << loc << " from " << old << " to " << type << '\n';
        // We should only ever generalize types, not refine them.
        assert(Type::isSubType(old, type));
      };

      void visitExpression(Expression* curr) {
        // Apply generalized types to all things we can. We must avoid updating
        // things with fixed types like constants (e.g. ref.func), and things we
        // do not yet optimize.
        // TODO: tuples
        switch (curr->_id) {
          // Control flow. Values fall through them, and we can just update
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
    // only works locally, at least, preserving global generalizations).
    ReFinalize().run(getPassRunner(), module); // utils.h
  }
};

} // anonymous namespace

Pass* createTypeGeneralizingPass() { return new TypeGeneralizing; }

} // namespace wasm
