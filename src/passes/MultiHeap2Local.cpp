/*
 * Copyright 2026 WebAssembly Community Group participants
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
// Simple multi-allocation escape analysis, aiming to cleanly separate multiple
// allocations written to one or more local variables.
//
// This pass identifies a simple case of refs not escaping, like Heap2Local, but
// allows more than a single allocation (struct.new), which is better, and
// only handles a far simpler-to-identify situation, which is worse. To see the
// motivation, consider overlapping allocations like this:
//
//    x = new Foo(42);
//    if (condition) x = new Foo(1337);
//    else x.value = -1;
//    bar(x.value, x.value);
//
// Heap2Local cannot optimize this due to the overlapping live ranges (it sees
// some x.value operations are not exclusive to any single allocation). But this
// pass can optimize into this:
//
//    x_value = (new Foo(42)).value;
//    if (condition) x_value = (new Foo(1337)).value;
//    else x_value = -1;
//    bar(x_value, x_value);
//
// Note how we now operate on x_value, a new local with data. Basically, each
// struct.get/struct.set is turned into an operation on that new local. This is
// safe despite the multiple allocations because the local x is always used
// immediately, as data: rather than store a reference, we can just focus on the
// data. That is, each struct.new just writes a bunch of fields, and it all
// could have been unboxed. After verifying that nothing observes x as a
// reference (no ref.eq), and it does not escape, we can optimize this.
// Heap2Local will then see 'untangled' allocations, each one immediately
// consumed, and remove them all.
//
// This also extends to groups of locals connected by copies (e.g., `y = x`).
// Copies between locals do not cause escaping, so we partition struct locals
// into disjoint groups connected by copies and analyze/optimize each group
// together: each local gets its own field locals, and a copy `y = x` becomes
// copies of those field locals (`y_value = x_value`). Because copying fields by
// value differs from reference semantics when a struct is mutated while aliased
// by another live local, for multi-local groups that contain struct.sets to
// read fields we verify via backward liveness that no potentially-aliasing peer
// local is live at any such struct.set.
//
// Note that Heap2Local could safely optimize in this example, even before our
// changes: while the `x.value` operations are not exclusive to allocations, the
// live ranges of the allocations join exactly there - they do not overlap in a
// dangerous way. But here is an example MultiHeap2Local can optimize that
// Heap2Local cannot:
//
//   (local.set $a
//     (struct.new $struct
//       (..value..)
//     )
//   )
//   (struct.get $ref 0
//     (select
//       (local.get $a)
//       (local.tee $a
//         (struct.new_default $struct)
//       )
//       (..condition..)
//     )
//   )
//
// The struct.get at the end may read ..value.. from set at the top, or it may
// read the default value from the tee at the bottom. But here we cannot
// optimize while using a single local `a_value` because of the live range
// overlap: both allocations are alive as the select executes, and it picks
// between them, so we must store both of them somewhere in the interim.
// Detecting such overlaps is not trivial as it involves locals as well as the
// wasm value stack, which is why this pass is useful, as it handles common and
// obviously-optimizable cases.
//

#include "analysis/cfg.h"
#include "analysis/lattices/powerset.h"
#include "analysis/monotone-analyzer.h"
#include "analysis/visitor-transfer-function.h"
#include "ir/bits.h"
#include "ir/find_all.h"
#include "ir/local-graph.h"
#include "ir/properties.h"
#include "pass.h"
#include "support/disjoint_sets.h"
#include "support/small_set.h"
#include "wasm-builder.h"
#include "wasm-traversal.h"
#include "wasm.h"

namespace wasm {

namespace {

struct MultiHeap2Local : public WalkerPass<PostWalker<MultiHeap2Local>> {
  bool isFunctionParallel() override { return true; }

  std::unique_ptr<Pass> create() override {
    return std::make_unique<MultiHeap2Local>();
  }

  // Information about a local index, used to decide whether we can optimize.
  struct LocalInfo {
    // The number of gets for the local, tracked so that we can see if all the
    // local.gets are consumed by struct.get/sets or copies to struct locals.
    Index numGets = 0;

    // The number of sets (including tees) for the local that write a
    // struct.new. We only optimize a group when we see 2 or more allocations
    // across the group (a single allocation is trivially optimized by
    // Heap2Local, so we would be doing pointless work).
    Index numAllocs = 0;

    // The number of tees for the local, tracked for the same reason as gets:
    // they flow out the reference, so we must see them consumed by struct
    // gets/sets or copies to struct locals.
    Index numTees = 0;

    // All struct.get/struct.set operations that read a get/tee of this local.
    std::vector<Expression*> structOperations;

    // The number of gets/tees of this local that are consumed by a copy
    // (local.set/local.tee) into a struct local.
    Index numCopyUses = 0;

    // All local.gets of this local that are used as the source of a copy into a
    // struct local, tracked so we can check if they may read the initial value
    // from function entry.
    std::vector<LocalGet*> copyGets;

    // Struct locals that this local is copied into (directed edge: this -> dst).
    SmallSet<Index, 2> copyTargets;

    // Certain conditions prevent us from optimizing.
    bool unoptimizable = false;
  };

  // For each local index, its info.
  std::vector<LocalInfo> localInfos;

  // Disjoint sets grouping struct locals connected by copies.
  DisjointSets localGroups;

  // Checks if a local index is relevant for us: if it refers to a struct.
  bool isStructLocal(Index index) {
    auto type = getFunction()->getLocalType(index);
    return type.isRef() && type.getHeapType().isStruct();
  }

  void visitLocalGet(LocalGet* curr) {
    if (isStructLocal(curr->index)) {
      localInfos[curr->index].numGets++;
    }
  }

  void visitLocalSet(LocalSet* curr) {
    if (!isStructLocal(curr->index)) {
      return;
    }

    auto& info = localInfos[curr->index];
    if (curr->isTee()) {
      info.numTees++;
    }

    // Avoid unreachable sets (e.g., a tee of an unreachable struct.new).
    if (curr->type == Type::unreachable) {
      info.unoptimizable = true;
      return;
    }

    // All sets must write a struct.new or copy from another struct local,
    // otherwise we cannot analyze this local.
    if (curr->value->is<StructNew>()) {
      info.numAllocs++;
    } else if (auto* get = curr->value->dynCast<LocalGet>()) {
      if (isStructLocal(get->index)) {
        localInfos[get->index].numCopyUses++;
        localInfos[get->index].copyGets.push_back(get);
        localInfos[get->index].copyTargets.insert(curr->index);
        localGroups.getUnion(get->index, curr->index);
      } else {
        info.unoptimizable = true;
      }
    } else if (auto* tee = curr->value->dynCast<LocalSet>()) {
      assert(tee->isTee());
      if (isStructLocal(tee->index)) {
        localInfos[tee->index].numCopyUses++;
        localInfos[tee->index].copyTargets.insert(curr->index);
        localGroups.getUnion(tee->index, curr->index);
      } else {
        info.unoptimizable = true;
      }
    } else {
      info.unoptimizable = true;
    }
  }

  void visitStructGet(StructGet* curr) { handleStructOperation(curr); }
  void visitStructSet(StructSet* curr) { handleStructOperation(curr); }

  template<typename T> void handleStructOperation(T* curr) {
    // Avoid unreachable code.
    if (curr->type == Type::unreachable) {
      return;
    }

    // See if the reference is one of the patterns we recognize.
    // TODO fallthrough, but careful of br_if which can escape the value
    if (auto* get = curr->ref->template dynCast<LocalGet>()) {
      if (!isStructLocal(get->index)) {
        return;
      }
      localInfos[get->index].structOperations.push_back(curr);

      // Also handle the value for a struct.set: a struct.set has two inputs,
      // and if the value modifies the local, then two allocations can be
      // live at once:
      //
      //   (struct.set
      //     (local.get $x)
      //     (block
      //       (local.set $x ..)
      //       ..
      //
      // Here $x is put on the value stack, but then we write something new into
      // $x before the struct.set, which means the struct.set is writing to the
      // old allocation - one that no struct.get will later read. When we see
      // such live range overlaps, give up.
      if (auto* structSet = curr->template dynCast<StructSet>()) {
        for (auto* localSet : FindAll<LocalSet>(structSet->value).list) {
          if (localSet->index == get->index) {
            localInfos[get->index].unoptimizable = true;
            break;
          }
        }
      }
    } else if (auto* tee = curr->ref->template dynCast<LocalSet>()) {
      if (!isStructLocal(tee->index)) {
        return;
      }
      // TODO: Handle a tee for struct.set. This is a little complex to handle
      // below, and not really needed, since
      //
      //   (struct.set $A k
      //     (local.tee
      //       (struct.new $A ..)
      //     )
      //     (value)
      //   )
      //
      // can be folded into a single struct.new (that is, we are writing all the
      // fields, then writing an additional one, and other passes make us do
      // them all at once anyhow).
      if (curr->template is<StructSet>()) {
        localInfos[tee->index].unoptimizable = true;
      } else {
        // Otherwise, this is a valid struct operation.
        localInfos[tee->index].structOperations.push_back(curr);
      }
    }
    // TODO: handle more things flowing into struct.get, like an If
  }

  void doWalkFunction(Function* func) {
    auto numLocals = func->getNumLocals();
    localInfos.resize(numLocals);
    localGroups.reserve(numLocals);
    for (Index i = 0; i < numLocals; ++i) {
      localGroups.addSet();
    }

    WalkerPass<PostWalker<MultiHeap2Local>>::doWalkFunction(func);

    pickLocalsToOptimize(func);

    if (!optimizedLocals.empty()) {
      optimize(func);
    }
  }

  // For each optimized local, maps each used field index to the new local
  // index that stores that field. Note that the inner map is sorted, both for
  // deterministic ordering when we create new locals, and also because the
  // number of used fields tends to be small (so hashing is overkill). Every
  // local in an optimized group has an entry here (possibly empty if it has no
  // used fields of its own).
  std::unordered_map<Index, std::map<Index, Index>> optimizedLocals;

  void pickLocalsToOptimize(Function* func) {
    auto* module = getModule();
    Builder builder(*module);
    LazyLocalGraph localGraph(func, module);

    // Partition struct locals into disjoint groups in deterministic order.
    std::vector<std::vector<Index>> groups;
    std::unordered_map<size_t, size_t> rootToGroup;
    for (Index i = 0; i < localInfos.size(); ++i) {
      if (!isStructLocal(i)) {
        continue;
      }
      auto root = localGroups.getRoot(i);
      auto [it, inserted] = rootToGroup.emplace(root, groups.size());
      if (inserted) {
        groups.emplace_back();
      }
      groups[it->second].push_back(i);
    }

    struct CandidateGroup {
      std::vector<Index> members;
      std::unordered_map<Index, std::set<Index>> usedFields;
    };
    std::vector<CandidateGroup> candidates;

    // For multi-local candidate groups that mutate a read field via struct.set,
    // collect the struct.sets and potentially-aliasing peer locals to check via
    // backward liveness.
    struct StructSetCheck {
      size_t groupId;
      std::vector<size_t> peerBits;
    };
    std::unordered_map<Index, size_t> localToBit;
    std::unordered_map<StructSet*, StructSetCheck> structSetChecks;

    for (auto& members : groups) {
      Index totalAllocs = 0;
      bool hasStructOp = false;
      bool ok = true;

      for (auto localIndex : members) {
        auto& info = localInfos[localIndex];
        if (info.unoptimizable) {
          ok = false;
          break;
        }
        // Check if every local.get/local.tee is consumed by a struct.get/set or
        // a copy to another struct local in this group, which is the case if
        // the counts match.
        if (info.numGets + info.numTees !=
            info.structOperations.size() + info.numCopyUses) {
          ok = false;
          break;
        }
        totalAllocs += info.numAllocs;
        if (!info.structOperations.empty()) {
          hasStructOp = true;
        }
      }
      if (!ok) {
        continue;
      }

      if (totalAllocs < 2) {
        // We only care about the case of 2 or more allocations in the group:
        // otherwise, Heap2Local can handle things perfectly for a single
        // allocation.
        continue;
      }

      if (!hasStructOp) {
        // No struct.get/struct.set operations on any local in this group, so
        // nothing to untangle.
        continue;
      }

      // Gather direct used fields for each local in the group, and track which
      // fields are read by any struct.get in the group.
      std::unordered_map<Index, std::set<Index>> groupUsedFields;
      std::unordered_set<Index> readFields;
      for (auto localIndex : members) {
        auto& fields = groupUsedFields[localIndex];
        for (auto* op : localInfos[localIndex].structOperations) {
          auto fieldIndex = getFieldIndex(op);
          fields.insert(fieldIndex);
          if (op->is<StructGet>()) {
            readFields.insert(fieldIndex);
          }
        }
      }

      // Propagate used fields backwards along copy edges: if src is copied into
      // dst, then any field used on dst must also be populated on src (which is
      // always valid because Wasm typing guarantees type(src) <: type(dst)).
      if (members.size() > 1) {
        bool changed = true;
        while (changed) {
          changed = false;
          for (auto src : members) {
            auto& srcFields = groupUsedFields[src];
            for (auto dst : localInfos[src].copyTargets) {
              for (auto fieldIndex : groupUsedFields[dst]) {
                if (srcFields.insert(fieldIndex).second) {
                  changed = true;
                }
              }
            }
          }
        }
      }

      // Finally, if the initial value of any local from the function entry is
      // used, do not optimize. For a var, that means the default of zero, which
      // will trap when accessed, while for params it means an unknown value. We
      // therefore must always check for params, but for vars, if we know the
      // default cannot be accessed, we can skip this (which is the case for
      // a non-nullable local, and if trapsNeverHappen as a trap on the null
      // default can be ignored - except for copies of non-defaultable fields,
      // where reading the uninitialized field local would introduce a new
      // ref.as_non_null trap at the copy site).
      for (auto localIndex : members) {
        auto& info = localInfos[localIndex];
        auto localType = func->getLocalType(localIndex);
        auto isParam = func->isParam(localIndex);
        auto canReadDefault =
          localType.isNullableRef() && !getPassOptions().trapsNeverHappen;

        if (isParam || canReadDefault) {
          for (auto* op : info.structOperations) {
            Expression* ref;
            if (auto* get = op->dynCast<StructGet>()) {
              ref = get->ref;
            } else if (auto* set = op->dynCast<StructSet>()) {
              ref = set->ref;
            } else {
              WASM_UNREACHABLE("bad op");
            }
            if (auto* get = ref->dynCast<LocalGet>()) {
              if (localGraph.getSets(get).contains(nullptr)) {
                ok = false;
                break;
              }
            }
          }
          if (!ok) {
            break;
          }
        }

        if (!info.copyGets.empty()) {
          bool hasNonDefaultableField = false;
          if (localType.isNullableRef() && getPassOptions().trapsNeverHappen) {
            auto& fields = localType.getHeapType().getStruct().fields;
            for (auto fieldIndex : groupUsedFields[localIndex]) {
              if (!fields[fieldIndex].type.isDefaultable()) {
                hasNonDefaultableField = true;
                break;
              }
            }
          }
          if (isParam || canReadDefault || hasNonDefaultableField) {
            for (auto* get : info.copyGets) {
              if (localGraph.getSets(get).contains(nullptr)) {
                ok = false;
                break;
              }
            }
            if (!ok) {
              break;
            }
          }
        }
      }
      if (!ok) {
        continue;
      }

      size_t groupId = candidates.size();

      // For multi-local groups, a struct.set on a local updates only that
      // local's field locals, not any other local's. This is valid only if no
      // other local in the group that could alias the same allocation is live
      // after the struct.set (for fields that are actually read in the group).
      if (members.size() > 1 && !readFields.empty()) {
        std::vector<std::pair<Index, StructSet*>> mutatingSets;
        for (auto localIndex : members) {
          for (auto* op : localInfos[localIndex].structOperations) {
            if (auto* structSet = op->dynCast<StructSet>()) {
              if (readFields.contains(structSet->index)) {
                mutatingSets.emplace_back(localIndex, structSet);
              }
            }
          }
        }

        if (!mutatingSets.empty()) {
          SmallSet<Index, 4> mutatingLocals;
          for (auto [localIndex, structSet] : mutatingSets) {
            mutatingLocals.insert(localIndex);
          }
          // Two locals in the group can alias the same allocation only if both
          // are reachable via directed copy edges from some local that receives
          // a struct.new.
          std::unordered_map<Index, SmallSet<Index, 2>> mayAliasPeers;
          for (auto source : members) {
            if (localInfos[source].numAllocs == 0) {
              continue;
            }
            std::vector<Index> worklist = {source};
            SmallSet<Index, 4> reachable;
            reachable.insert(source);
            while (!worklist.empty()) {
              auto curr = worklist.back();
              worklist.pop_back();
              for (auto next : localInfos[curr].copyTargets) {
                if (!reachable.contains(next)) {
                  reachable.insert(next);
                  worklist.push_back(next);
                }
              }
            }
            for (auto u : reachable) {
              if (!mutatingLocals.contains(u)) {
                continue;
              }
              for (auto v : reachable) {
                if (u != v) {
                  mayAliasPeers[u].insert(v);
                }
              }
            }
          }

          for (auto [localIndex, structSet] : mutatingSets) {
            auto it = mayAliasPeers.find(localIndex);
            if (it == mayAliasPeers.end()) {
              continue;
            }
            StructSetCheck check;
            check.groupId = groupId;
            for (auto peer : it->second) {
              auto [bitIt, inserted] =
                localToBit.emplace(peer, localToBit.size());
              check.peerBits.push_back(bitIt->second);
            }
            structSetChecks[structSet] = std::move(check);
          }
        }
      }

      candidates.push_back({std::move(members), std::move(groupUsedFields)});
    }

    // If any multi-local candidate group has a struct.set to a read field with
    // potentially-aliasing peers, run backward liveness over just those peers
    // to reject groups where an aliasing peer is live after a struct.set.
    std::vector<bool> groupFailed(candidates.size(), false);
    if (!structSetChecks.empty()) {
      auto cfg = analysis::CFG::fromFunction(func, module);
      analysis::FiniteIntPowersetLattice lattice(localToBit.size());

      struct StructSetAliasLiveness
        : public analysis::VisitorTransferFunc<
            StructSetAliasLiveness,
            analysis::FiniteIntPowersetLattice,
            analysis::AnalysisDirection::Backward> {
        const std::unordered_map<Index, size_t>& localToBit;
        const std::unordered_map<StructSet*, StructSetCheck>& checks;
        std::vector<bool>& groupFailed;

        StructSetAliasLiveness(
          const std::unordered_map<Index, size_t>& localToBit,
          const std::unordered_map<StructSet*, StructSetCheck>& checks,
          std::vector<bool>& groupFailed)
          : localToBit(localToBit), checks(checks), groupFailed(groupFailed) {}

        void visitLocalSet(LocalSet* curr) {
          if (auto it = localToBit.find(curr->index); it != localToBit.end()) {
            currState->set(it->second, false);
          }
        }

        void visitLocalGet(LocalGet* curr) {
          if (auto it = localToBit.find(curr->index); it != localToBit.end()) {
            currState->set(it->second, true);
          }
        }

        void visitStructSet(StructSet* curr) {
          if (!collectingResults) {
            return;
          }
          if (auto it = checks.find(curr); it != checks.end()) {
            const auto& check = it->second;
            if (groupFailed[check.groupId]) {
              return;
            }
            for (auto peerBit : check.peerBits) {
              if (currState->get(peerBit)) {
                groupFailed[check.groupId] = true;
                break;
              }
            }
          }
        }
      } liveness(localToBit, structSetChecks, groupFailed);

      analysis::MonotoneCFGAnalyzer analyzer(lattice, liveness, cfg);
      analyzer.evaluateAndCollectResults();
    }

    // We can optimize all surviving candidate groups!
    for (size_t groupId = 0; groupId < candidates.size(); ++groupId) {
      if (groupFailed[groupId]) {
        continue;
      }
      auto& candidate = candidates[groupId];
      for (auto localIndex : candidate.members) {
        auto localType = func->getLocalType(localIndex);
        auto& fields = localType.getHeapType().getStruct().fields;
        // Always insert an entry for every local in an optimized group, even if
        // it has no used fields of its own, so that copies into it are still
        // rewritten and do not keep old references alive.
        auto& fieldMap = optimizedLocals[localIndex];
        for (auto fieldIndex : candidate.usedFields[localIndex]) {
          fieldMap[fieldIndex] = builder.addVar(func, fields[fieldIndex].type);
        }
      }
    }
  }

  void optimize(Function* func) {
    struct Optimizer : PostWalker<Optimizer> {
      MultiHeap2Local& parent;

      Optimizer(MultiHeap2Local& parent) : parent(parent) {}

      void visitLocalSet(LocalSet* curr) {
        if (!curr->isTee() && parent.optimizedLocals.contains(curr->index)) {
          // local.set now stores/copies the used fields (tee is handled by its
          // consumer: another local.set/tee or a struct.get).
          replaceCurrent(makeSetOrCopy(curr));
        }
      }

      void visitStructGet(StructGet* curr) {
        if (auto* get = curr->ref->dynCast<LocalGet>()) {
          if (parent.optimizedLocals.contains(get->index)) {
            // struct.get of a local.get reads the field from a new local.
            replaceCurrent(
              getFieldFromLocal(get->index, curr->index, curr->signed_));
          }
        } else if (auto* tee = curr->ref->dynCast<LocalSet>()) {
          if (parent.optimizedLocals.contains(tee->index)) {
            // struct.get of a local.tee first writes/copies the fields, then
            // reads.
            replaceCurrent(makeSetOrCopy(tee, curr));
          }
        }
      }

      void visitStructSet(StructSet* curr) {
        if (auto* get = curr->ref->dynCast<LocalGet>()) {
          if (parent.optimizedLocals.contains(get->index)) {
            // struct.set of a local.get writes the field into a new local.
            replaceCurrent(
              setFieldToLocal(get->index, curr->index, curr->value));
          }
        }
      }

      Expression*
      getFieldFromLocal(Index localIndex, Index fieldIndex, bool signed_) {
        auto& field = getFunction()
                        ->getLocalType(localIndex)
                        .getHeapType()
                        .getStruct()
                        .fields[fieldIndex];
        // Read from the appropriate new local, handling sign extension as
        // needed.
        auto newLocal = parent.optimizedLocals[localIndex][fieldIndex];
        Builder builder(*getModule());
        Expression* ret =
          builder.makeLocalGet(newLocal, getFunction()->getLocalType(newLocal));
        return Bits::makePackedFieldGet(ret, field, signed_, *getModule());
      }

      Expression*
      setFieldToLocal(Index localIndex, Index fieldIndex, Expression* value) {
        // Write to the appropriate new local.
        auto newLocal = parent.optimizedLocals[localIndex][fieldIndex];
        Builder builder(*getModule());
        return builder.makeLocalSet(newLocal, value);
      }

      Expression* makeSetOrCopy(LocalSet* set, StructGet* teeParent = nullptr) {
        // Unwind any chain of local.tees to find the root value (either a
        // StructNew or a LocalGet of a local in the same group). `chain[0]` is
        // the outermost set (`set`) and `chain.back()` is the innermost set
        // directly receiving `root`.
        std::vector<LocalSet*> chain;
        Expression* root = set;
        while (auto* currSet = root->dynCast<LocalSet>()) {
          chain.push_back(currSet);
          root = currSet->value;
        }

        Builder builder(*getModule());
        std::vector<Expression*> list;

        auto emitCopy = [&](Index srcIndex, Index dstIndex) {
          if (srcIndex == dstIndex) {
            return;
          }
          const auto& srcFields = parent.optimizedLocals.at(srcIndex);
          const auto& dstFields = parent.optimizedLocals.at(dstIndex);
          for (auto [fieldIndex, dstNewLocal] : dstFields) {
            auto srcNewLocal = srcFields.at(fieldIndex);
            list.push_back(builder.makeLocalSet(
              dstNewLocal,
              builder.makeLocalGet(srcNewLocal,
                                   getFunction()->getLocalType(srcNewLocal))));
          }
        };

        auto innermostIndex = chain.back()->index;
        if (root->is<StructNew>()) {
          auto localType = getFunction()->getLocalType(innermostIndex);
          auto& fields = localType.getHeapType().getStruct().fields;

          // Store the reference, then store struct.gets of all used fields in
          // the new locals.
          list.push_back(builder.makeLocalSet(innermostIndex, root));
          for (auto [fieldIndex, newLocal] :
               parent.optimizedLocals.at(innermostIndex)) {
            auto& field = fields[fieldIndex];
            // Note that the sign does not matter here: we just store the bits,
            // and will handle the sign when reading back out (like in
            // Heap2Local).
            auto* sget = builder.makeStructGet(
              fieldIndex,
              builder.makeLocalGet(innermostIndex, localType),
              MemoryOrder::Unordered,
              field.type,
              false);
            list.push_back(builder.makeLocalSet(newLocal, sget));
          }
        } else if (auto* get = root->dynCast<LocalGet>()) {
          emitCopy(get->index, innermostIndex);
        } else {
          WASM_UNREACHABLE("unexpected root in MultiHeap2Local");
        }

        // Propagate field copies up the tee chain, if any.
        for (size_t i = chain.size() - 1; i > 0; --i) {
          emitCopy(chain[i]->index, chain[i - 1]->index);
        }

        // Finally, if this is a tee, then it was consumed by a struct.get, and
        // we can read that field and return it.
        if (teeParent) {
          list.push_back(getFieldFromLocal(
            set->index, teeParent->index, teeParent->signed_));
        }

        if (list.empty()) {
          return builder.makeNop();
        }
        if (list.size() == 1) {
          return list[0];
        }
        return builder.makeBlock(list);
      }
    } optimizer(*this);
    optimizer.setModule(getModule());
    optimizer.walkFunction(func);
  }

  // Given a struct.get or struct.set, return the field index.
  Index getFieldIndex(Expression* curr) {
    if (auto* get = curr->dynCast<StructGet>()) {
      return get->index;
    } else if (auto* set = curr->dynCast<StructSet>()) {
      return set->index;
    } else {
      WASM_UNREACHABLE("bad op");
    }
  }
};

} // anonymous namespace

Pass* createMultiHeap2LocalPass() { return new MultiHeap2Local(); }

} // namespace wasm
