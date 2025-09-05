/*
 * Copyright 2025 WebAssembly Community Group participants
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

#include "ir/locations.h"
#include "wasm.h"

namespace wasm {

void LocationLinkGraph::fill(Module& wasm) {
  // Find all locations, so we know what additional links to add.
  std::unordered_set<Location> all;
  for (auto& [from, to] : *this) {
    all.insert(from);
    all.insert(to);
  }

  // Find the parent function of all expressions, as ExpressionLocations do not
  // store that themselves (t

  // Find locations that we can add boilerplate links to, and do so.
  for (auto& location : all) {
    auto* exprLoc = std::get_if<wasm::ExpressionLocation>(&location);
    if (!exprLoc) {
      continue;
    }
    auto* curr = exprLoc->expr;
    if (auto* get = curr->dynCast<GlobalGet>()) {
      // Reads from the corresponding global.
      emplace(GlobalLocation{get->name}, location);
    } else if (auto* set = curr->dynCast<GlobalGet>()) {
      // Write to the corresponding global.
      emplace(location, GlobalLocation{set->name});
    } /*else if (auto* get = curr->dynCast<LocalGet>()) {
      return LocalLocation{get->index};
    } else if (auto* get = curr->dynCast<StructGet>()) {
      return DataLocation{get->ref->type.getHeapType(), get->index};
    } else if (auto* get = curr->dynCast<ArrayGet>()) {
      return DataLocation{get->ref->type.getHeapType(),
                          DataLocation::ArrayIndex};
    } else if (auto* get = curr->dynCast<RefGetDesc>()) {
      return DataLocation{get->ref->type.getHeapType(),
                          DataLocation::DescriptorIndex};
    } */
  }
}

} // namespace wasm

namespace std {

// Debugging

std::ostream& operator<<(std::ostream& o, wasm::ModuleLocation moduleLocation) {
  auto [module, location] = moduleLocation;
  if (auto* loc = std::get_if<wasm::ExpressionLocation>(&location)) {
    o << "  exprloc \n" << *loc->expr << " : " << loc->tupleIndex << '\n';
  } else if (auto* loc = std::get_if<wasm::DataLocation>(&location)) {
    o << "  dataloc ";
    if (module->typeNames.count(loc->type)) {
      o << '$' << module->typeNames[loc->type].name;
    } else {
      o << loc->type << '\n';
    }
    o << " : " << loc->index << '\n';
  } else if (auto* loc = std::get_if<wasm::TagLocation>(&location)) {
    o << "  tagloc " << loc->tag << " : " << loc->tupleIndex << '\n';
  } else if (auto* loc = std::get_if<wasm::ParamLocation>(&location)) {
    o << "  paramloc " << loc->func->name << " : " << loc->index << '\n';
  } else if (auto* loc = std::get_if<wasm::LocalLocation>(&location)) {
    o << "  localloc " << loc->func->name << " : " << loc->index << '\n';
  } else if (auto* loc = std::get_if<wasm::ResultLocation>(&location)) {
    o << "  resultloc $" << loc->func->name << " : " << loc->index << '\n';
  } else if (auto* loc = std::get_if<wasm::GlobalLocation>(&location)) {
    o << "  globalloc " << loc->name << '\n';
  } else if (std::get_if<wasm::SignatureParamLocation>(&location)) {
    o << "  sigparamloc " << '\n';
  } else if (auto* loc =
               std::get_if<wasm::SignatureResultLocation>(&location)) {
    o << "  sigresultloc " << loc->type << " : " << loc->index << '\n';
  } else {
    o << "  (other)\n";
  }
  return o;
}

std::ostream& operator<<(std::ostream& o, wasm::Location location) {
  return o << wasm::ModuleLocation{nullptr, location};
}

} // namespace std

