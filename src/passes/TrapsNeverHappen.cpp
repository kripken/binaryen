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

//
// This pass applies the assumptions of traps-never-happen mode to remove
// unneeded code. TNH is already used in various passes in relevant places; what
// this pass does is additional optimizations that only make sense in TNH mode.
//
// The main optimization done here is to remove unneeded code. In TNH mode, a
// trap is assumed not to happen, so if a basic block looks like this:
//
// {
//   code1
//   code2
//   trap
// }
//
// then since the trap is assumed to never happen, we should never even enter
// this basic block, and can remove code1 and code2, even if they have side
// effects.
//

#include "ir/linear-execution.h"
#include "ir/utils.h"
#include "pass.h"
#include "wasm-builder.h"
#include "wasm-traversal.h"
#include "wasm.h"

namespace wasm {

namespace {

struct Scanner
  : public LinearExecutionWalker<Scanner, UnifiedExpressionVisitor<Scanner>> {

  // The expressions in the current linear execution span (basic block).
  std::vector<Expression*> inBlock;

  // All the expressions we have seen that we can remove.
  std::unordered_set<Expression*> removable;

  static void doNoteNonLinear(Scanner* self, Expression** currp) {
    // Control flow is splitting or merging; clear the current list of items in
    // the current block.
    //
    // Note that we do *not* do this if the cause of the nonlinear control flow
    // is an unreachable - that is the very thing we want to optimize here. We
    // optimize under the assumption that that nonlinearity never occurs.
    if (!(*currp)->is<Unreachable>()) {
      self->inBlock.clear();
    }
std::cout << "nonlinear\n";
  }

  void visitExpression(Expression* curr) {
    if (curr->is<Unreachable>()) {
std::cout << "trap!\n";
      // This traps, so everything in this basic block can be removed.
      //
      // Note that we don't need to bother with things after the trap - dce will
      // handle those anyhow.
      for (auto* item : inBlock) {
        removable.insert(item);
std::cout << "  mark removable\n";
      }
      inBlock.clear();
      return;
    }

    inBlock.push_back(curr);
std::cout << "push inNBlock\n";
  }
};

struct Remover
  : public PostWalker<Remover, UnifiedExpressionVisitor<Remover>> {

  std::unordered_set<Expression*>& removable;

  Remover(std::unordered_set<Expression*>& removable) : removable(removable) {}

  void visitExpression(Expression* curr) {
    if (removable.count(curr)) {
      replaceCurrent(Builder(*getModule()).makeUnreachable());
    }
  }
};

} // anonymous namespace

struct TrapsNeverHappen : public WalkerPass<PostWalker<TrapsNeverHappen>> {
  bool isFunctionParallel() override { return true; }

  // FIXME DWARF updating does not handle local changes yet.
  bool invalidatesDWARF() override { return true; }

  Pass* create() override { return new TrapsNeverHappen(); }

  void doWalkFunction(Function* func) {
    if (!getPassOptions().trapsNeverHappen) {
      Fatal() << "Cannot run traps-never-happen opts pass without -tnh";
    }

    while (1) {
std::cout << "iter1\n";
      Scanner scanner;
      scanner.walkFunctionInModule(func, getModule());
      if (scanner.removable.empty()) {
        return;
      }
std::cout << "iter2\n";

      // We have things to remove. Remove them, then apply DCE to propagate that
      // further, and then loop around.
      Remover remover(scanner.removable);
      remover.walkFunctionInModule(func, getModule());

      ReFinalize().walkFunctionInModule(func, getModule());

      PassRunner runner(getModule(), getPassRunner()->options);
      runner.setIsNested(true);
      runner.add("dce");
      runner.runOnFunction(func);
    }
  }
};

Pass* createTrapsNeverHappenPass() { return new TrapsNeverHappen(); }

} // namespace wasm
