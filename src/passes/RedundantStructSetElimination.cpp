/*
 * Copyright 2024 WebAssembly Community Group participants
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
// Eliminate redundant struct.sets by folding them into struct.news: Given a
// struct.new and a struct.set that occurs right after it, and that applies to
// the same data, try to apply the set during the new. This can be either with
// a nested tee:
//
//  (struct.set
//    (local.tee $x (struct.new X Y Z))
//    X'
//  )
// =>
//  (local.set $x (struct.new X' Y Z))
//
// or without, in sequence in a block:
//
//  (local.set $x (struct.new X Y Z))
//  (struct.set (local.get $x) X')
// =>
//  (local.set $x (struct.new X' Y Z))
//
// This cannot be a simple peephole optimization because of branching issues:
//
//  (local.set $x (struct.new X Y Z))
//  (struct.set (local.get $x) (call $throw))
//
// If the call throws then we executed the struct.new and local.set but *not*
// the struct.set, and so if that throw can reach a location in the function
// that uses the local $x then it can observe that fact. To handle that we track
// whether such branches can in fact reach such uses. Note that this is
// important as it is common in real-world code to have a call in such a
// position (and the call is, even with global effects, not easily seen as not
// throwing).
//

#include "cfg/cfg-traversal.h"
#include "pass.h"
#include "wasm.h"
#include "wasm-traversal.h"

namespace wasm {

namespace {

// We store relevant instructions in each basic block.
struct Info {
  std::vector<Expression**> items;
};

struct RedundantStructSetElimination
  : public WalkerPass<CFGWalker<RedundantStructSetElimination,
                                Visitor<RedundantStructSetElimination>,
                                Info>> {
  bool isFunctionParallel() override { return true; }

  std::unique_ptr<Pass> create() override {
    return std::make_unique<RedundantStructSetElimination>();
  }

  // Branches outside of the function can be ignored, as we only look at locals
  // which vanish when we leave.
  bool ignoreBranchesOutsideOfFunc = true;

  // CFG traversal work: We track the exact expressions we care about, as
  // mentioned above: struct.sets and blocks, on which we optimize, and
  // local.gets and sets, which we need to check for control flow issues.

  void note(Expression** currp) {
    if (currBasicBlock) {
      currBasicBlock->contents.items.push_back(currp);
    }
  } 
   
  static void doVisitBlock(RedundantStructSetElimination* self,
                              Expression** currp) {
    self->note(currp);
  }
  static void doVisitStructSet(RedundantStructSetElimination* self,
                              Expression** currp) {
    self->note(currp);
  }
  static void doVisitLocalGet(RedundantStructSetElimination* self,
                              Expression** currp) {
    self->note(currp);
  }
  static void doVisitLocalSet(RedundantStructSetElimination* self,
                              Expression** currp) {
    self->note(currp);
  }

  // main entry point

  void doWalkFunction(Function* func) {
    // Create the CFG by walking the IR.
    CFGWalker<RedundantStructSetElimination, Visitor<RedundantStructSetElimination>, Info>::
      doWalkFunction(func);

    // Find things to optimize.
    for (auto& block : basicBlocks) {
      auto& items = block->contents.items;
      for (auto** currp : items) {
        // If this is a struct.set with a nested tee and new (the first
        // situation in the top comment in this file), handle that, or if it is
        // a block, handle struct.sets after news (the second situation in the
        // top comment in this file).
        if (auto* set = (*currp)->dynCast<StructSet>()) {
          optimizeStructSet(set, currp);
        } else if (auto* block = (*currp)->dynCast<Block>()) {
          optimizeBlock(block);
        }
      }
    }
  }

  void optimizeStructSet(StructSet* set, Expression** currp) {
    if (auto* tee = set->ref->dynCast<LocalSet>()) {
      if (auto* new_ = tee->value->dynCast<StructNew>()) {
        if (optimizeSubsequentStructSet(new_, set, tee->index)) {
          // Success, so we do not need the struct.set any more, and the tee
          // can just be a set instead of us.
          tee->makeSet();
          *currp = tee;
        }
      }
    }
  }

  // Handle pairs like this among a block's children:
  //
  //  (local.set $x (struct.new X Y Z))
  //  (struct.set (local.get $x) X')
  // =>
  //  (local.set $x (struct.new X' Y Z))
  //
  // We also handle other struct.sets immediately after this one, but we only
  // handle the case where they are all in sequence and right after the
  // local.set (anything in the middle of this pattern will stop us from
  // optimizing later struct.sets, which might be improved later but would
  // require an analysis of effects TODO).
  void optimizeBlock(Block* block) {
    auto& list = block->list;
    for (Index i = 0; i < list.size(); i++) {
      auto* localSet = list[i]->dynCast<LocalSet>();
      if (!localSet) {
        continue;
      }
      auto* new_ = localSet->value->dynCast<StructNew>();
      if (!new_) {
        continue;
      }

      // This local.set of a struct.new looks good. Find struct.sets after it
      // to optimize.
      for (Index j = i + 1; j < list.size(); j++) {
        auto* structSet = list[j]->dynCast<StructSet>();
        if (!structSet) {
          // Any time the pattern no longer matches, stop optimizing possible
          // struct.sets for this struct.new.
          break;
        }
        auto* localGet = structSet->ref->dynCast<LocalGet>();
        if (!localGet || localGet->index != localSet->index) {
          break;
        }
        if (!optimizeSubsequentStructSet(new_, structSet, localGet->index)) {
          break;
        } else {
          // Success. Replace the set with a nop, and continue to
          // perhaps optimize more.
          ExpressionManipulator::nop(structSet);
        }
      }
    }
  }

  // Given a struct.new and a struct.set that occurs right after it, and that
  // applies to the same data, try to apply the set during the new. This can be
  // either with a nested tee:
  //
  //  (struct.set
  //    (local.tee $x (struct.new X Y Z))
  //    X'
  //  )
  // =>
  //  (local.set $x (struct.new X' Y Z))
  //
  // or without:
  //
  //  (local.set $x (struct.new X Y Z))
  //  (struct.set (local.get $x) X')
  // =>
  //  (local.set $x (struct.new X' Y Z))
  //
  // Returns true if we succeeded.
  bool optimizeSubsequentStructSet(StructNew* new_,
                                   StructSet* set,
                                   Index refLocalIndex) {
    // Leave unreachable code for DCE, to avoid updating types here.
    if (new_->type == Type::unreachable || set->type == Type::unreachable) {
      return false;
    }

    if (new_->isWithDefault()) {
      // Ignore a new_default for now. If the fields are defaultable then we
      // could add them, in principle, but that might increase code size.
      return false;
    }

    auto index = set->index;
    auto& operands = new_->operands;

    // Check for effects that prevent us moving the struct.set's value (X' in
    // the function comment) into its new position in the struct.new. First, it
    // must be ok to move it past the local.set (otherwise, it might read from
    // memory using that local, and depend on the struct.new having already
    // occurred; or, if it writes to that local, then it would cross another
    // write).
    auto setValueEffects = effects(set->value);
    if (setValueEffects.localsRead.count(refLocalIndex) ||
        setValueEffects.localsWritten.count(refLocalIndex)) {
      return false;
    }

    // We must move the set's value past indexes greater than it (Y and Z in
    // the example in the comment on this function).
    // TODO When this function is called repeatedly in a sequence this can
    //      become quadratic - perhaps we should memoize (though, struct sizes
    //      tend to not be ridiculously large).
    for (Index i = index + 1; i < operands.size(); i++) {
      auto operandEffects = effects(operands[i]);
      if (operandEffects.invalidates(setValueEffects)) {
        // TODO: we could use locals to reorder everything
        return false;
      }
    }

    Builder builder(*getModule());

    // See if we need to keep the old value.
    if (effects(operands[index]).hasUnremovableSideEffects()) {
      operands[index] =
        builder.makeSequence(builder.makeDrop(operands[index]), set->value);
    } else {
      operands[index] = set->value;
    }

    return true;
  }

  EffectAnalyzer effects(Expression* expr) {
    return EffectAnalyzer(getPassOptions(), *getModule(), expr);
  }
};

} // namespace

Pass* createRedundantStructSetEliminationPass() {
  return new RedundantStructSetElimination();
}

} // namespace wasm
