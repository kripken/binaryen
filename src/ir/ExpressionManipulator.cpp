/*
 * Copyright 2017 WebAssembly Community Group participants
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

#include "ir/load-utils.h"
#include "ir/utils.h"

namespace wasm {

namespace ExpressionManipulator {

Expression*
flexibleCopy(Expression* original, Module& wasm, CustomCopier custom) {
  struct CopyTask {
    // The thing to copy.
    Expression* source;
    // The location of the pointer to write the copy to.
    Expression** destPointer;
  };
  std::vector<CopyTask> tasks;
  // A child pointer may be nullptr, in which case we don't need to copy.
  auto handleChild = [&](Expression* source, Expression** destPointer) {
    if (source == nullptr) {
      *destPointer = nullptr;
    } else {
      tasks.push_back({ source, destPointer });
    }
  };
  Expression* ret;
  tasks.push_back({ original, &ret });
  while (!tasks.empty()) {
    auto task = tasks.back();
    tasks.pop_back();
    // If the custom copier handled this one, we have nothing to do.
    auto* copy = custom(task.source);
    if (copy) {
      *task.destPointer = copy;
      continue;
    }
    // Copy it ourselves.
    switch (task.source->_id) {
#include "expression-copying.generated.h"
      default: {
        WASM_UNREACHABLE("invalid copy id");
      }
    }
  }
  return ret;
}

// Splice an item into the middle of a block's list
void spliceIntoBlock(Block* block, Index index, Expression* add) {
  auto& list = block->list;
  if (index == list.size()) {
    list.push_back(add); // simple append
  } else {
    // we need to make room
    list.push_back(nullptr);
    for (Index i = list.size() - 1; i > index; i--) {
      list[i] = list[i - 1];
    }
    list[index] = add;
  }
  block->finalize(block->type);
}

} // namespace ExpressionManipulator

} // namespace wasm
