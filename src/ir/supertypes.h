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

#ifndef wasm_ir_supertypes_h
#define wasm_ir_supertypes_h

#include "support/small_vector.h"
#include "wasm.h"

//
// Utilities to get the list of all supertypes of a type or heap type.
//

namespace wasm {

namespace SuperTypes {

using SuperHeapVec = SmallVector<HeapType, 5>;
using SuperVec = SmallVector<Type, 5>;

SuperHeapVec getSuperTypes(HeapType type) {
  SuperHeapVec ret;
  while (1) {
    ret.push_back(type);
    if (auto super = type.getSuperType()) {
      type = *super;
    } else {
      // No non-basic supers left.
      if (type.isStruct()) {
        ret.push_back(HeapType::struct_);
        ret.push_back(HeapType::eq);
        ret.push_back(HeapType::any);
      } else if (type.isArray()) {
        ret.push_back(HeapType::array);
        ret.push_back(HeapType::eq);
        ret.push_back(HeapType::any);
      } else if (type.isSignature()) {
        ret.push_back(HeapType::func);
      } else if (type == HeapType::struct_ || type == HeapType::array) {
        ret.push_back(HeapType::eq);
        ret.push_back(HeapType::any);
      } else if (type == HeapType::eq) {
        ret.push_back(HeapType::any);
      }
      break;
    }
  }
  return ret;
}

SuperVec getSuperTypes(Type type) {
  SuperVec ret;
  for (auto heapType : getSuperTypes(type.getHeapType())) {
    ret.push_back(Type(heapType, Nullable));
    if (type.isNonNullable()) {
      ret.push_back(Type(heapType, NonNullable));
    }
  }
  return ret;
}

} // namespace SuperTypes

} // namespace wasm

#endif // wasm_ir_supertypes_h
