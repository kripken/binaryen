/*
 * Copyright 2022 WebAssembly Community Group participants
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

#include "ir/function-utils.h"
#include "ir/local-graph.h"
#include "ir/localize.h"
#include "ir/possible-constant.h"
#include "ir/type-updating.h"
#include "passes/param-utils.h"
#include "support/sorted_vector.h"
#include "wasm.h"

namespace wasm::ParamUtils {

std::unordered_set<Index> getUsedParams(Function* func) {
  LocalGraph localGraph(func);

  std::unordered_set<Index> usedParams;

  for (auto& [get, sets] : localGraph.getSetses) {
    if (!func->isParam(get->index)) {
      continue;
    }

    for (auto* set : sets) {
      // A nullptr value indicates there is no LocalSet* that sets the value,
      // so it must be the parameter value.
      if (!set) {
        usedParams.insert(get->index);
      }
    }
  }

  return usedParams;
}

// Get the operands of a Call/CallRef.
static ExpressionList& getOperands(Expression* call) {
  if (auto* c = call->dynCast<Call>()) {
    return c->operands;
  } else if (auto* c = call->dynCast<CallRef>()) {
    return c->operands;
  } else {
    WASM_UNREACHABLE("bad call");
  }
};

void removeParameter(const std::vector<Function*>& funcs,
                     Index index,
                     const std::vector<CallOrigin>& calls,
                     Module* module,
                     PassRunner* runner) {
  assert(funcs.size() > 0);
  auto* first = funcs[0];
#ifndef NDEBUG
  for (auto* func : funcs) {
    assert(func->type == first->type);
  }
#endif

  // Remove the parameter from the function. We must add a new local
  // for uses of the parameter, but cannot make it use the same index
  // (in general).
  auto paramsType = first->getParams();
  std::vector<Type> params(paramsType.begin(), paramsType.end());
  auto type = params[index];
  params.erase(params.begin() + index);
  // TODO: parallelize some of these loops?
  for (auto* func : funcs) {
    func->setParams(Type(params));

    // It's cumbersome to adjust local names - TODO don't clear them?
    Builder::clearLocalNames(func);
  }
  std::vector<Index> newIndexes;
  for (auto* func : funcs) {
    newIndexes.push_back(Builder::addVar(func, type));
  }
  // Update local operations.
  struct LocalUpdater : public PostWalker<LocalUpdater> {
    Index removedIndex;
    Index newIndex;
    LocalUpdater(Function* func, Index removedIndex, Index newIndex)
      : removedIndex(removedIndex), newIndex(newIndex) {
      walk(func->body);
    }
    void visitLocalGet(LocalGet* curr) { updateIndex(curr->index); }
    void visitLocalSet(LocalSet* curr) { updateIndex(curr->index); }
    void updateIndex(Index& index) {
      if (index == removedIndex) {
        index = newIndex;
      } else if (index > removedIndex) {
        index--;
      }
    }
  };
  for (Index i = 0; i < funcs.size(); i++) {
    auto* func = funcs[i];
    if (!func->imported()) {
      LocalUpdater(funcs[i], index, newIndexes[i]);
      TypeUpdating::handleNonDefaultableLocals(func, *module);
    }
  }

  // Remove the arguments from the calls.
  for (auto& callOrigin : calls) {
    auto*& call = *callOrigin.call;
    // Localize the call's children so that we can remove the one we want.
    ChildLocalizer localizer(call, callOrigin.func, *module, runner->options);
    auto* operands = getOperands(call);
    operands->erase(operands->begin() + index);
    if (!localizer.sets.empty()) {
      // When we localized we found we need some sets before the call. Add
      // those now.
      auto* replacementBlock = localizer.getReplacement();
      call = replacementBlock;
    }
  }
}

void removeParameters(const std::vector<Function*>& funcs,
                      SortedVector indexes,
                      const std::vector<CallOrigin>& calls,
                      Module* module,
                      PassRunner* runner) {
  if (indexes.empty()) {
    return;
  }

  assert(funcs.size() > 0);
  auto* first = funcs[0];
#ifndef NDEBUG
  for (auto* func : funcs) {
    assert(func->type == first->type);
  }
#endif

  // Iterate downwards, as we may remove more than one, and going forwards would
  // alter the indexes after us.
  Index i = first->getNumParams() - 1;
  while (1) {
    if (indexes.has(i)) {
      removeParameter(funcs, i, calls, module, runner);
    }
    if (i == 0) {
      break;
    }
    i--;
  }
}

SortedVector applyConstantValues(const std::vector<Function*>& funcs,
                                 const std::vector<CallOrigin>& calls,
                                 Module* module) {
  assert(funcs.size() > 0);
  auto* first = funcs[0];
#ifndef NDEBUG
  for (auto* func : funcs) {
    assert(func->type == first->type);
  }
#endif

  SortedVector optimized;
  auto numParams = first->getNumParams();
  for (Index i = 0; i < numParams; i++) {
    PossibleConstantValues value;
    for (auto& callOrigin : calls) {
      auto*& call = *callOrigin.call;
      auto* operands = getOperands(call);
      value.note(operands[i], *module);
      if (!value.isConstant()) {
        break;
      }
    }
    if (!value.isConstant()) {
      continue;
    }

    // Optimize: write the constant value in the function bodies, making them
    // ignore the parameter's value.
    Builder builder(*module);
    for (auto* func : funcs) {
      func->body = builder.makeSequence(
        builder.makeLocalSet(i, value.makeExpression(*module)), func->body);
    }
    optimized.insert(i);
  }

  return optimized;
}

} // namespace wasm::ParamUtils
