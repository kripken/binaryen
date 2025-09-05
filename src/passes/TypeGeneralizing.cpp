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
  // All the links we found in this function. Rarely are there duplicates
  // in this list (say when writing to the same global location from another
  // global location), and we do not try to deduplicate here, just store them in
  // a plain array for now, which is faster (later, when we merge all the info
  // from the functions, we need to deduplicate anyhow).
  std::vector<LocationLink> links;

  // All the roots of the graph, that is, concrete constraints on typing (i.e.,
  // absolute, as opposed to links that are relative between locations). For
  // example, ref.eq imposes a root constraint of eqref on both children.
  //
  // The vector here is of the location of the root and its type.
  std::vector<std::pair<Location, Type>> roots;
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
