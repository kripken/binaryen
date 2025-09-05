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
Location getSourceLocation(Expression* curr) {
  if (auto* get = curr->dynCast<GlobalGet>()) {
    return GlobalLocation{get->name};
  } if (auto* get = curr->dynCast<LocalGet>()) {
    return LocalLocation{get->name};
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

// Collect subtyping constraints for one CollectedFuncInfo.
struct InfoCollector : public SubtypingDiscoverer<InfoCollector> {
  CollectedFuncInfo& info;

  InfoCollector(CollectedFuncInfo& info) : info(info) {}

  // Constraints on type themselves do not interest us.
  void noteSubtype(Type, Type) {}
  void noteSubtype(HeapType, HeapType) {}
  void noteSubtype(Type, Expression) {}

  // An absolute (root) constraint.
  void noteSubtype(Expression* sub, Type super) {
    info.roots.emplace_back({getSourceLocation(super), type});
  }
  void noteNonFlowSubtype(Expression* sub, Type super) {
    noteSubtype(sub, super);
  }

  // A relative (link) constraint.
  void noteSubtype(Expression* sub, Expression* super) {
    info.roots.emplace_back({getLocation(super), getLocation(sub)});
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
  }
};

} // anonymous namespace

Pass* createTypeGeneralizingPass() { return new TypeGeneralizing; }

} // namespace wasm
