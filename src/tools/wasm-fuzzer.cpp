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

// In-process fuzzing. Run with e.g.
//
// bin/wasm-fuzzer -only_ascii=1 -max_len=8196 wats
//

#include <algorithm>

#include "parser/wat-parser.h"
#include "wasm-validator.h"
#include "wasm.h"

#include "tool-options.h"

using namespace wasm;

namespace {
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  Module wasm;
  wasm.features.setAll();

  // Null-terminate the input data so it is a valid string to parse.
  std::vector<char> bytes;
  bytes.resize(size + 1);
  std::copy(data, data + size, bytes.data());
  bytes[size] = 0;

  // Reject invalid inputs.
  auto parsed = WATParser::parseModule(wasm, bytes.data());
  if (parsed.getErr()) {
    return -1;
  }

  if (!WasmValidator().validate(wasm, WasmValidator::Globally | WasmValidator::Quiet)) {
    return -1;
  }

  // Optimize.
  auto options = PassOptions::getWithDefaultOptimizationOptions();
  options.optimizeLevel = 3;
  PassRunner runner(&wasm, options);
  runner.addDefaultOptimizationPasses();
  runner.run();
  if (!WasmValidator().validate(wasm, options)) {
    // A validation error is an error we want the fuzzer to catch, so halt.
    abort();
  }

  return 0;
}
