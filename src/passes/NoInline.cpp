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
// Mark functions as no-inline based on name wildcards.
//

#include "pass.h"
#include "support/string.h"
#include "wasm.h"

namespace wasm {

namespace {

enum NoInlineMode {
  Full = 0,
  Partial = 1,
  Both = 2
};

struct NoInline : public Pass {
  NoInlineMode mode;

  NoInline(NoInlineMode mode) : mode(mode) {}

  void run(Module* module) override {
    std::string pattern = getPassOptions().getArgument(
      "no-inline",
      "Usage usage:  wasm-opt --no-inline=WILDCARD");

    for (auto& func : module->functions) {
      if (!String::wildcardMatch(pattern, func->name.toString())) {
        continue;
      }

      if (mode == Full || mode == Both) {
        func->noFullInline = true;
      }
      if (mode == Partial || mode == Both) {
        func->noPartialInline = true;
      }
    }
  }
};

} // anonymous namespace

Pass* createNoInlinePass() { return new NoInline(NoInlineMode::Both); }
Pass* createNoFullInlinePass() { return new NoInline(NoInlineMode::Full); }
Pass* createNoPartialInlinePass() { return new NoInline(NoInlineMode::Partial); }

} // namespace wasm
