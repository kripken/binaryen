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

#include "passes/param-utils.h"
#include "cfg/liveness-traversal.h"
#include "ir/eh-utils.h"
#include "ir/function-utils.h"
#include "ir/localize.h"
#include "ir/possible-constant.h"
#include "ir/type-updating.h"
#include "pass.h"
#include "support/sorted_vector.h"
#include "wasm-traversal.h"
#include "wasm.h"

namespace wasm::ParamUtils {

std::unordered_set<Index> getUsedParams(Function* func, Module* module) {
  // To find which params are used, compute liveness at the entry.
  // TODO: We could write bespoke code here rather than reuse LivenessWalker, as
  //       we only need liveness at the entry. The code below computes it for
  //       the param indexes in the entire function. However, there are usually
  //       very few params (compared to locals, which we ignore here), so this
  //       may be fast enough, and is very simple.
  struct ParamLiveness
    : public LivenessWalker<ParamLiveness, Visitor<ParamLiveness>> {
    using Super = LivenessWalker<ParamLiveness, Visitor<ParamLiveness>>;

    // Branches outside of the function can be ignored, as we only look at
    // locals, which vanish when we leave.
    bool ignoreBranchesOutsideOfFunc = true;

    // Ignore unreachable code and non-params.
    static void doVisitLocalGet(ParamLiveness* self, Expression** currp) {
      auto* get = (*currp)->cast<LocalGet>();
      if (self->currBasicBlock && self->getFunction()->isParam(get->index)) {
        Super::doVisitLocalGet(self, currp);
      }
    }
    static void doVisitLocalSet(ParamLiveness* self, Expression** currp) {
      auto* set = (*currp)->cast<LocalSet>();
      if (self->currBasicBlock && self->getFunction()->isParam(set->index)) {
        Super::doVisitLocalSet(self, currp);
      }
    }
  } walker;
  walker.setModule(module);
  walker.walkFunction(func);

  if (!walker.entry) {
    // Empty function: nothing is used.
    return {};
  }

  // We now have a sorted vector of the live params at the entry. Convert that
  // to a set.
  auto& sortedLiveness = walker.entry->contents.start;
  std::unordered_set<Index> usedParams;
  for (auto live : sortedLiveness) {
    usedParams.insert(live);
  }
  return usedParams;
}

void localizeCallsTo(const std::unordered_set<Name>& callTargets,
                     Module& wasm,
                     PassRunner* runner,
                     std::function<void(Function*)> onChange) {
  struct LocalizerPass : public WalkerPass<PostWalker<LocalizerPass>> {
    bool isFunctionParallel() override { return true; }
    // May add non-nullable locals, but fixups are never needed as they are
    // immediately used in the code right after.
    bool requiresNonNullableLocalFixups() override { return false; }

    std::unique_ptr<Pass> create() override {
      return std::make_unique<LocalizerPass>(callTargets, onChange);
    }

    const std::unordered_set<Name>& callTargets;
    std::function<void(Function*)> onChange;

    LocalizerPass(const std::unordered_set<Name>& callTargets,
                  std::function<void(Function*)> onChange)
      : callTargets(callTargets), onChange(onChange) {}

    void visitCall(Call* curr) {
      if (!callTargets.count(curr->target)) {
        return;
      }

      ChildLocalizer localizer(
        curr, getFunction(), *getModule(), getPassOptions());
      auto* replacement = localizer.getReplacement();
      if (replacement != curr) {
        replaceCurrent(replacement);
        optimized = true;
        onChange(getFunction());
      }
    }

    bool optimized = false;

    void visitFunction(Function* curr) {
      if (optimized) {
        // Localization can add blocks, which might move pops.
        EHUtils::handleBlockNestedPops(curr, *getModule());
      }
    }
  };

  LocalizerPass(callTargets, onChange).run(runner, &wasm);
}

void localizeCallsTo(const std::unordered_set<HeapType>& callTargets,
                     Module& wasm,
                     PassRunner* runner) {
  struct LocalizerPass : public WalkerPass<PostWalker<LocalizerPass>> {
    bool isFunctionParallel() override { return true; }
    // See above.
    bool requiresNonNullableLocalFixups() override { return false; }

    std::unique_ptr<Pass> create() override {
      return std::make_unique<LocalizerPass>(callTargets);
    }

    const std::unordered_set<HeapType>& callTargets;

    LocalizerPass(const std::unordered_set<HeapType>& callTargets)
      : callTargets(callTargets) {}

    void visitCall(Call* curr) {
      handleCall(curr,
                 getModule()->getFunction(curr->target)->type.getHeapType());
    }

    void visitCallRef(CallRef* curr) {
      auto type = curr->target->type;
      if (type.isRef()) {
        handleCall(curr, type.getHeapType());
      }
    }

    void handleCall(Expression* call, HeapType type) {
      if (!callTargets.count(type)) {
        return;
      }

      ChildLocalizer localizer(
        call, getFunction(), *getModule(), getPassOptions());
      auto* replacement = localizer.getReplacement();
      if (replacement != call) {
        replaceCurrent(replacement);
        optimized = true;
      }
    }

    bool optimized = false;

    void visitFunction(Function* curr) {
      if (optimized) {
        EHUtils::handleBlockNestedPops(curr, *getModule());
      }
    }
  };

  LocalizerPass(callTargets).run(runner, &wasm);
}

} // namespace wasm::ParamUtils
