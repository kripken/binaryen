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

#include "ir/gc-type-utils.h"
#include "ir/locations.h"
#include "ir/module-utils.h"
#include "wasm.h"

namespace wasm::Locations {

Type getType(const Location& location, Module& wasm) {
  if (auto* loc = std::get_if<wasm::ExpressionLocation>(&location)) {
    return loc->expr->type;
  } else if (auto* loc = std::get_if<wasm::DataLocation>(&location)) {
    return GCTypeUtils::getField(loc->type, loc->index)->type;
  } else if (auto* loc = std::get_if<wasm::TagLocation>(&location)) {
    return wasm.getTag(loc->tag)->params()[loc->tupleIndex];
  } else if (auto* loc = std::get_if<wasm::ParamLocation>(&location)) {
    return wasm.getFunction(loc->func->name)->getLocalType(loc->index);
  } else if (auto* loc = std::get_if<wasm::LocalLocation>(&location)) {
    return wasm.getFunction(loc->func->name)->getLocalType(loc->index);
  } else if (auto* loc = std::get_if<wasm::ResultLocation>(&location)) {
    return wasm.getFunction(loc->func->name)->getResults()[loc->index];
  } else if (auto* loc = std::get_if<wasm::GlobalLocation>(&location)) {
    return wasm.getGlobal(loc->name)->type;
  } else if (auto* loc = std::get_if<wasm::SignatureParamLocation>(&location)) {
    return loc->type.getSignature().params[loc->index];
  } else if (auto* loc =
               std::get_if<wasm::SignatureResultLocation>(&location)) {
    return loc->type.getSignature().results[loc->index];
  } else {
    WASM_UNREACHABLE("TODO: all locations");
  }
}

} // namespace wasm::Locations

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
std::cout << "RESULTLOCATION1\n";
std::cout << "RESULTLOCATION2 " << loc->func << "\n";
    o << "  resultloc $" << loc->func->name << " : " << loc->index << '\n';
std::cout << "RESULTLOCATION3\n";
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
