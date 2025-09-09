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
// To do this, we generate a global graph of all type constraints in the module,
// then flow the constraints to a fixed point to see what the minimal types are
// that do not break validation or change program behavior.
//
// TODO: In closed world we could not treat externally-visible types as roots,
//       e.g., we could generalize an exported (ref $foo) to an anyref.
//

#include "analysis/lattice.h"
#include "analysis/lattices/valtype.h"
#include "ir/locations.h"
#include "ir/lubs.h"
#include "ir/subtype-exprs.h"
#include "pass.h"
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
  // where sub must be a subtype of super. XXX this may not be right with fill()
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
  CollectedFuncInfo& info;

  Collector(CollectedFuncInfo& info) : info(info) {}

  void link(Location sub, Location super) {
    // TODO: don't bother linking non-ref-typed things. At least exprs are easy
    info.links.emplace_back(sub, super);
  }

  void loop(Location sub, Location super) {
    link(sub, super);
    link(super, sub);
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
    info.roots.emplace_back(getLocation(sub), super);
  }
  void noteNonFlowSubtype(Expression* sub, Type super) {
    noteSubtype(sub, super);
  }

  // A relative (link) constraint, e.g. between a block and its last child.
  void noteSubtype(Expression* sub, Expression* super) {
    link(getLocation(sub), getLocation(super));
  }

  // TODO
  void noteCast(HeapType, HeapType) {}
  void noteCast(Expression*, Type) {}
  void noteCast(Expression*, Expression*) {}

  // Override subtype-exprs where we want more precise representation of the
  // flow of constraints.

  void visitGlobalGet(GlobalGet* curr) {
    // Reads from the corresponding global, and must match the global's type
    // exactly, so loop them.
    // TODO: should all loops be in a general "add equality connections"
    //       helper, which adds on top of the subtype-exprs stuff?
    loop(GlobalLocation{curr->name}, ExpressionLocation{curr, 0});
  }

  void visitGlobalSet(GlobalSet* curr) {
    // Override the parent behavior of an absolute constraint of "value is a
    // subtype of ${current type of global}" with a link to the global, i.e., a
    // relative constraint.
    link(getLocation(curr->value), GlobalLocation{curr->name});
  }

  void visitDrop(Drop* curr) {
    // Drop imposes no constraints on the input. It is the only expression
    // that truly does so, and therefore requires special treatment, as below,
    // we look for expressions that have no constraints and fix their types (see
    // below for why). We do not want drop to do this, so we make it impose the
    // "empty" constraint of the top type for the child.
    auto* value = curr->value;
    if (value->type.isRef()) {
      noteSubtype(value,
                  Type(value->type.getHeapType().getTop(), Nullable));
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

    // Collect information from each function.
    ModuleUtils::ParallelFunctionAnalysis<CollectedFuncInfo> analysis(
      *module, [&](Function* func, CollectedFuncInfo& info) {
        if (!func->imported()) {
          Collector(info).walkFunctionInModule(func, module);
        }
      });

    // Also walk the global module code (for simplicity, also add it to the
    // function map, using a "function" key of nullptr).
    auto& moduleInfo = analysis.map[nullptr];
    Collector(moduleInfo).walkModuleCode(module);

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

    // Finalize the graph. If an expression has no constraints on it whatsoever,
    // then we force its type to remain fixed by making it a root. Normally
    // every expression has some constraint, derived by the parent of the
    // expression, like the parent ref.eq of a child forces it to be of type eq.
    // When subtyping-exprs has no constraint at all, that means it is unaware
    // of subtyping requirements, but there are other ones, such as a
    // struct.get's reference child. TODO copy explanation for that from
    // subtype-exprs, or give a better example.
    //
    // Each time we do this, we are missing a potential optimization opportunity
    // that could be added. This starts us out in a safe and valid place. For
    // example, we could optimize BrOn by adding a visitor above and precisely
    // stating what requirements are placed on the input reference.
    std::unordered_set<Expression*> isTarget;
    auto noteExprTarget = [&](const Location& loc) {
      if (auto* exprLoc = std::get_if<ExpressionLocation>(&loc)) { // TODO not just exprloc?
        if (exprLoc->expr->type.isRef()) {
          isTarget.insert(exprLoc->expr);
        }
      }
    };
    for (auto& root : roots) {
      noteExprTarget(root);
    }
    for (auto& link : links) {
      noteExprTarget(link.to);
    }
    // After finding all the expressions that are targets, mark the others as
    // roots. We only need to consider the |from| of links, as all other things
    // are definitely targets.
    for (auto& link : links) {
      if (auto* exprLoc = std::get_if<ExpressionLocation>(&link.from)) {
        if (exprLoc->expr->type.isRef() && !isTarget.count(exprLoc->expr) &&
            !locationValues.count(*exprLoc)) {
          // Force its type to its original one.
          addRoot(*exprLoc, exprLoc->expr->type);
        }
      }
    }

    // Get the flow-efficient sorted graph.
    if (DEBUG) std::cerr << "graph:\n";
    auto sortedGraph = links.getSortedGraph();
    for (auto& [loc, targets] : sortedGraph) {
      if (DEBUG) std::cerr << loc << " sends to targets: [\n";
      for (auto t : targets) {
        if (DEBUG) std::cerr << "  " << t << '\n';
      }
      if (DEBUG) std::cerr << "]\n";
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

    // Apply |locationValues| to the module.
    if (DEBUG) std::cerr << "computed locationValues:\n";
    for (auto& [loc, value] : locationValues) {
      if (DEBUG) std::cerr << loc << " => " << value << '\n';
    }

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
        auto old = type;
        auto iter = parent.locationValues.find(loc);
        type = iter == parent.locationValues.end()
                 ? Type(type.getHeapType().getTop(), Nullable)
                 : iter->second;
        if (DEBUG) std::cerr << "Updated " << loc << " to " << type << '\n';
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
          // Things we optimize, and need updating.
          case Expression::GlobalGetId:
            updateType(ExpressionLocation{curr, 0}, curr->type);
            //curr->finalize();?
            break;
          default:
            break;
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
  }
};

} // anonymous namespace

Pass* createTypeGeneralizingPass() { return new TypeGeneralizing; }

} // namespace wasm
