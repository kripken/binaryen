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

namespace wasm {

void LocationLinkGraph::fill(Module& wasm, const std::unordered_set<Location>& roots) {
  // XXX unneeded?

  // Find all locations, so we know what additional links to add.
  std::unordered_set<Location> all;
  for (auto& [from, to] : *this) {
    all.insert(from);
    all.insert(to);
  }

  for (auto root : roots) {
    all.insert(root);
  }

  // Find the parent function of all expressions, as ExpressionLocations do not
  // store that themselves (to save on size).
  using Exprs = std::unordered_set<Expression*>;
  ModuleUtils::ParallelFunctionAnalysis<Exprs> analysis(
    wasm, [&](Function* func, Exprs& exprs) {
      if (func->imported()) {
        return;
      }

      struct Finder
        : public PostWalker<Finder, UnifiedExpressionVisitor<Finder>> {
        Exprs& exprs;
        Finder(Exprs& exprs) : exprs(exprs) {}
        void visitExpression(Expression* curr) { exprs.insert(curr); }
      } finder(exprs);
      finder.walk(func->body);
    });

  // Build a mapping of expressions to their parent functions.
  std::unordered_map<Expression*, Function*> exprFuncs;
  for (auto& [func, exprs] : analysis.map) {
    for (auto* expr : exprs) {
      exprFuncs[expr] = func;
    }
  }

std::cout << "loop2\n";
  // Find locations that we can add boilerplate links to, and do so.
  for (auto& location : all) {
std::cout << "loop3 " << location << '\n';
    auto* exprLoc = std::get_if<wasm::ExpressionLocation>(&location);
    if (!exprLoc) {
      continue;
    }
    auto* curr = exprLoc->expr;
    if (auto* get = curr->dynCast<GlobalGet>()) {
      // Reads from the corresponding global.
      emplace(GlobalLocation{get->name}, location);
    } /* else if (auto* get = curr->dynCast<LocalGet>()) {
       // Reads from the corresponding local.
       emplace(LocalLocation{exprFuncs[curr], get->index}, location);
     } else if (auto* set = curr->dynCast<LocalSet>()) {
       // Write to the corresponding local.
       emplace(location, LocalLocation{exprFuncs[curr], set->index});
     }*/
    /* TODO else if (auto* get = curr->dynCast<StructGet>()) {
      return DataLocation{get->ref->type.getHeapType(), get->index};
    } else if (auto* get = curr->dynCast<ArrayGet>()) {
      return DataLocation{get->ref->type.getHeapType(),
                          DataLocation::ArrayIndex};
    } else if (auto* get = curr->dynCast<RefGetDesc>()) {
      return DataLocation{get->ref->type.getHeapType(),
                          DataLocation::DescriptorIndex};
    } */
  }

  // TODO: collect calls for [Signature]Param/ResultLocation
  //         - collect them, then see if their params are in our initial set etc
  // TODO: struct operations for DataLoc, build ConeReadLocations
  // TODO: taglocation stuffs
}

void LocationLinkGraph::reverse() {
  auto original = std::move(*this);
  for (auto& link : original) {
    insert({link.to, link.from});
  }
}

LocationLinkGraph::SortedGraph LocationLinkGraph::getSortedGraph() { // XXX unneeded
  SortedGraph ret;
  for (auto& [from, to] : *this) {
    ret[from].push_back(to);
  }
  return ret;
}

Type getLocationType(const Location& location, Module& wasm) {
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
