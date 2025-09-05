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

#include "analysis/lattice.h"
#include "analysis/lattices/valtype.h"
#include "ir/locations.h"
#include "ir/lubs.h"
#include "ir/subtype-exprs.h"
#include "pass.h"
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
  // where sub must be a subtype of super.
  std::vector<LocationLink> links;

  // 2. The second constraint we gather are absolute ones, "roots". E.g., ref.eq
  // imposes a root constraint of eqref on both children.
  //
  // The vector here is of the location of the root and the constraint there (as
  // above, we do not deduplicate here).
  std::vector<std::pair<Location, Element>> roots;
};

// Get the source location for an expression, that is, the location from where
// its values arrive. For example, a global.get's source is the corresponding
// GlobalLocation.
// TODO: move to locations.h, if there are other users?
#if 0
Location getSourceLocation(Expression* curr) { // TODO; get func
  if (auto* get = curr->dynCast<GlobalGet>()) {
    return GlobalLocation{get->name};
  } if (auto* get = curr->dynCast<LocalGet>()) {
    return LocalLocation{get->index};
  } if (auto* get = curr->dynCast<StructGet>()) {
    return DataLocation{get->ref->type.getHeapType(), get->index};
  } if (auto* get = curr->dynCast<ArrayGet>()) {
    return DataLocation{get->ref->type.getHeapType(),
                        DataLocation::ArrayIndex};
  } if (auto* get = curr->dynCast<RefGetDesc>()) {
    return DataLocation{get->ref->type.getHeapType(),
                        DataLocation::DescriptorIndex};
  }
  Fatal() << "bad source " << *curr;
}
#endif

Location getLocation(Expression* curr) {
  // TODO: tuples
  return ExpressionLocation{curr, 0};
}

// Collect subtyping constraints for one CollectedFuncInfo.
struct Collector : ControlFlowWalker<Collector, SubtypingDiscoverer<Collector>> {
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

  void run(Module* module) override {
    if (!module->features.hasGC()) {
      return;
    }

    if (!getPassOptions().closedWorld) {
      Fatal() << "TypeRefining requires --closed-world";
    }

    // Collect information from each function.
    ModuleUtils::ParallelFunctionAnalysis<CollectedFuncInfo> analysis(
      *module, [&](Function* func, CollectedFuncInfo& info) {
        Collector(info).walkFunctionInModule(func, module);
      });

    // Also walk the global module code (for simplicity, also add it to the
    // function map, using a "function" key of nullptr).
    auto& globalInfo = analysis.map[nullptr];
    Collector(globalInfo).walkModuleCode(module);

    // Merge the function information into a single large graph, deduplicating
    // as we go
    std::unordered_set<LocationLink> links;
    InsertOrderedMap<Location, Element> roots; // XXX insertordered?

    for (auto& [func, info] : analysis.map) {
      for (auto& link : info.links) {
        links.insert(link);
      }
      for (auto& [root, value] : info.roots) {
        roots[root] = value;
      }
    }

    // We no longer need the function-level info.
    analysis.map.clear();
  }
};

} // anonymous namespace

Pass* createTypeGeneralizingPass() { return new TypeGeneralizing; }

} // namespace wasm
