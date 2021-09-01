/*
 * Copyright 2021 WebAssembly Community Group participants
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

#include "ir/intrinsics.h"
#include "wasm-builder.h"

namespace wasm {

static Name BinaryenIntrinsics("binaryen-intrinsics"),
            ConsumerUsed("consumer.used");

bool Intrinsics::isConsumerUsed(Expression* curr) {
std::cout << *curr << '\n';
  if (auto* call = curr->dynCast<Call>()) {
std::cout << "  a1\n";//
    // The target function may not exist if the module is still being
    // constructed.
    if (auto* func = module.getFunctionOrNull(call->target)) {
std::cout << "  a2\n" << (func->module == BinaryenIntrinsics) << " : " << (func->base == ConsumerUsed)  << " : " <<
             (func->getParams() == Type::none)  << " : " << (func->getResults() == Type::i32) << '\n';
      return func->module == BinaryenIntrinsics && func->base == ConsumerUsed &&
             func->getParams() == Type::none && func->getResults() == Type::i32;
    }
  }
std::cout << "  a3\n";//
  return false;
}

} // namespace wasm
