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
    info.links.emplace_back(getLocation(sub), getLocation(super));
  }

  // TODO
  void noteCast(HeapType, HeapType) {}
  void noteCast(Expression*, Type) {}
  void noteCast(Expression*, Expression*) {}
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

    // Collect information from each function.
    ModuleUtils::ParallelFunctionAnalysis<CollectedFuncInfo> analysis(
      *module, [&](Function* func, CollectedFuncInfo& info) {
        Collector(info).walkFunctionInModule(func, module);
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

    // All our operations will be on the lattice of types.
    analysis::ValType lattice;

    // Merge the function information into a single large graph, deduplicating
    // as we go, and preparing the initial list of work (the roots).
    LocationLinkGraph links;
    std::unordered_set<Location> roots;
    UniqueDeferredQueue<Location> work;

    for (auto& [func, info] : analysis.map) {
      for (auto& link : info.links) {
        links.insert(link);
      }
      for (auto& [root, value] : info.roots) {
        lattice.meet(locationValues[root], value);
        roots.insert(root);
        work.push(root);
      }
    }

    // We no longer need the function-level info.
    analysis.map.clear();

    // Prepare the full graph to flow on.
    links.fill(*module, roots);

    // Our updates are in the reverse of the normal direction of the flow of
    // information. E.g. when we see (global.set $g (value)) we do not send the
    // value to the GlobalLocation, but rather look back and say that the
    // value's sources must be refined enough to fit in the global's type.
    links.reverse();

    // Get the flow-efficient sorted graph.
    std::cerr << "graph:\n";
    auto sortedGraph = links.getSortedGraph();
    for (auto& [loc, targets] : sortedGraph) {
      std::cerr << loc << " sends to targets: [\n";
      for (auto t : targets) {
        std::cerr << "  " << t << '\n';
      }
      std::cerr << "]\n";
    }

    // Flow while changes happen.
    while (!work.empty()) {
      auto loc = work.pop();
      std::cerr << "working on " << loc << '\n';
      auto value = locationValues[loc];
      for (auto target : sortedGraph[loc]) {
        if (lattice.meet(locationValues[target], value)) {
          // Flow onward.
          work.push(target);
        }
      }
    }

    // Apply |locationValues| to the module.
    std::cerr << "map:\n";
    for (auto& [loc, value] : locationValues) {
      std::cerr << loc << " => " << value << '\n';
    }

    update(module);
  }

  // Apply |locationValues| to the IR.
  void update(Module* module) {
    struct Updater : public WalkerPass<PostWalker<Updater>> {
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
        auto iter = parent.locationValues.find(loc);
        type = iter == parent.locationValues.end()
                 ? Type(type.getHeapType().getTop(), Nullable)
                 : iter->second;
        std::cerr << "Updated " << loc << " to " << type << '\n';
      };

      void visitGlobalGet(GlobalGet* curr) {
        updateType(GlobalLocation{curr->name}, curr->type);
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
