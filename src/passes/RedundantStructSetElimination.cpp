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
#include "support/unique_deferring_queue.h"
#include "wasm-traversal.h"
#include "wasm.h"

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
  // local operations, which we need to check for control flow issues.

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
    // We don't strictly need to track local.set, but it does let us stop
    // scanning a path for a local.get. XXX
    self->note(currp);
  }

  // main entry point

  void doWalkFunction(Function* func) {
    // Create the CFG by walking the IR.
    CFGWalker<RedundantStructSetElimination,
              Visitor<RedundantStructSetElimination>,
              Info>::doWalkFunction(func);

    // Find things to optimize.
    for (Index i = 0; i < basicBlocks.size(); i++) {
      auto* basicBlock = basicBlocks[i].get();
      auto& items = basicBlock->contents.items;
      for (Index j = 0; j < items.size(); j++) {
        auto** currp = items[j];
        // If this is a struct.set with a nested tee and new (the first
        // situation in the top comment in this file), handle that, or if it is
        // a block, handle struct.sets after news (the second situation in the
        // top comment in this file).
        if (auto* set = (*currp)->dynCast<StructSet>()) {
          optimizeStructSet(set, currp, basicBlock, j);
        } else if (auto* block = (*currp)->dynCast<Block>()) {
          optimizeBlock(block, basicBlock, j);
        }
      }
    }
  }

  // Optimize a tee'd new in a struct.set:
  //
  //  (struct.set (local.tee $x (struct.new X Y Z)) X')
  // =>
  //  (local.set $x (struct.new X' Y Z))
  //
  // We are provided the struct.set, the pointer to it (so we can replace it if
  // we optimize) and also the index of our basic block and our index inside
  // that basic block.
  void optimizeStructSet(StructSet* set,
                         Expression** currp,
                         BasicBlock* basicBlock,
                         Index indexInBasicBlock) {
    if (auto* tee = set->ref->dynCast<LocalSet>()) {
      if (auto* new_ = tee->value->dynCast<StructNew>()) {
        if (optimizeSubsequentStructSet(
              new_, set, tee, basicBlock, indexInBasicBlock)) {
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
  void
  optimizeBlock(Block* block, BasicBlock* basicBlock, Index indexInBasicBlock) {
    auto& list = block->list;
    for (Index i = 0; i < list.size(); i++) {
      // First, find a local.set of a struct.new.
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
        if (!optimizeSubsequentStructSet(
              new_, structSet, localSet, basicBlock, indexInBasicBlock)) {
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
  // We are also provided the index of the basic block that the set is in, and
  // its index inside that basic block (needed for CFG computation).
  //
  // Returns true if we succeeded.
  bool optimizeSubsequentStructSet(StructNew* new_,
                                   StructSet* set, // XXX rename
                                   LocalSet* localSet,
                                   BasicBlock* structSetBasicBlock,
                                   Index structSetIndexInBasicBlock) {
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
    if (setValueEffects.localsRead.count(localSet->index) ||
        setValueEffects.localsWritten.count(localSet->index)) {
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

    // Finally, consider CFG issues, which is a more expensive check.
    if (hasEscapingRefBeforeStructSet(
          set, localSet, structSetBasicBlock, structSetIndexInBasicBlock)) {
      return false;
    }

    // Looks good! We can optimize here.

    // See if we need to keep the old value. TODO use existing helper here
    if (effects(operands[index]).hasUnremovableSideEffects()) {
      Builder builder(*getModule());
      operands[index] =
        builder.makeSequence(builder.makeDrop(operands[index]), set->value);
    } else {
      operands[index] = set->value;
    }

    return true;
  }

  // We need to avoid a situation where the reference "escapes" before the
  // struct.set. In that case, the reference might be used without the
  // struct.set being applied, and we are trying to apply it unconditionally.
  // For example:
  //
  //  (try
  //    (do
  //      (local.set $x
  //        (struct.new X)
  //      )
  //      (struct.set
  //        (local.get $x)
  //        (call $throw)
  //      )
  //    )
  //    (catch)
  //  )
  //  ;; This read will get X, the value before the struct.set, if we threw.
  //  (struct.get
  //    (local.get $x)
  //  )
  //
  // To detect this, we consider the basic blocks of the local.set and the
  // struct.set. The only code that executes in between those, aside from the
  // struct.set's value, is a local.get which does not branch. So any blocks
  // we observe are due to the struct.set's value, and we can follow those to
  // see if they lead to local.gets. If so, the situation is dangerous. Note
  // that the situation is analogous with a local.tee as well:
  //
  //  (struct.set
  //    (local.tee $x (struct.new X Y Z))
  //    ..value..
  //  )
  //
  // Once more, between the local.set/tee and the struct.set all that can
  // branch (in fact, all that can execute) is the struct.set's value.
  //
  // We're given structSetBasicBlock and structSetIndexInBasicBlock, the
  // basic block the struct.set is in, and its index inside that block. Using
  // that we can find the basic block of the local.set, by going backwards until
  // we find it. Along the way we collect basic blocks to scan forward from.
  bool hasEscapingRefBeforeStructSet(StructSet* structSet,
                                     LocalSet* localSet,
                                     BasicBlock* structSetBasicBlock,
                                     Index structSetIndexInBasicBlock) {
    // First, find the basic blocks between the struct.set and local.set.
    auto& items = structSetBasicBlock->contents.items;
    for (Index i = 0; i < structSetIndexInBasicBlock; i++) {
      if (*items[i] == localSet) {
        // The local.set is in the same block as the struct.set, so escaping is
        // not possible.
        return false;
      }
    }

    // We will note the local.set's block as we work, as we'll need it later.
    BasicBlock* localSetBlock = nullptr;

    // There are blocks in between the struct.set and local.set. Find them by
    // flowing backwards from the struct.set, and stop at the local.set. We'll
    // add the in-between blocks to a queue for the forward flow later.
    UniqueNonrepeatingDeferredQueue<BasicBlock*> inBetween;

    // A queue for the backwards flow.
    UniqueNonrepeatingDeferredQueue<BasicBlock*> queue;
    for (auto* prev : structSetBasicBlock->in) {
      queue.push(prev);
    }
    while (!queue.empty()) {
      auto* block = queue.pop();

      // Looping is impossible in the IR structures we are considering.
      assert(block != structSetBasicBlock);

      // This is a pred of the struct.set's block, which we'll need to scan
      // later for forward escaping.
      inBetween.push(block);

      // Look through the block. If we reached the local.set, stop and do not
      // proceed to our preds.
      auto stop = false;
      for (auto** item : block->contents.items) {
        if (*item == localSet) {
          localSetBlock = block;
          stop = true;
          break;
        }
      }
      if (!stop) {
        for (auto* prev : block->in) {
          queue.push(prev);
        }
      }
    }

    // Flow forward from those in-between blocks to find any dangerous uses of
    // the reference.
    std::unordered_set<BasicBlock*> scanned;
    scanned.insert(structSetBasicBlock);
    while (!inBetween.empty()) {
      auto* block = inBetween.pop();
      auto& items = block->contents.items;

      // Process the relevant contents of this block. If this is the block that
      // has the local.set then we only want to look from right after it, as
      // anything before is not relevant for us.
      Index start = 0;
      if (block == localSetBlock) {
        while (*items[start] != localSet) {
          start++;
        }
        start++;
      }

      // On paths where the local is overwritten we can stop scanning.
      auto overwritten = false;
      for (Index i = start; i < items.size(); i++) {
        auto* item = *items[i];
        if (auto* get = item->dynCast<LocalGet>()) {
          // Check if this is a dangerous get: another get of the same index.
          // Note that we must ignore the struct.set's reference, as in the non-
          // tee form we have
          //
          //      (local.set $x
          //        (struct.new X)
          //      )
          //      (struct.set
          //        (local.get $x)
          //        (call $throw)
          //      )
          //
          // We know that particular local.get is safe.
          if (get->index == localSet->index && get != structSet->ref) {
            return true;
          }
        } else if (auto* set = item->dynCast<LocalSet>()) {
          // We do not need to scan the original local.set, and should not.
          assert(set != localSet);
          if (set->index == localSet->index) {
            overwritten = true;
            break;
          }
        }
      }
      if (!overwritten) {
        // We must look onwards.
        for (auto* succ : block->out) {
          inBetween.push(succ);
        }
      }
    }

    // No escaping, no problem.
    return false;
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
