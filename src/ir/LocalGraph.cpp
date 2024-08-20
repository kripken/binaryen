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

#include <iterator>

#include "cfg/cfg-traversal.h"
#include "ir/find_all.h"
#include "ir/local-graph.h"
#include "support/unique_deferring_queue.h"
#include "wasm-builder.h"

namespace wasm {

namespace {

// Fill in a LocalGraph by processing the AST, taking advantage of the
// structured control flow for speed. Another approach would be to construct a
// CFG and do a flow, which would be slightly simpler conceptually, but the
// difference in speed is quite large and this is a core utility that many
// passes utilize.
//
// In particular, doing it this way allows us to do almost all the work in a
// single forward pass, since in post-order each block is visited after its
// predecessors. The only exceptions are backedges, which in the AST are
// conveniently identified as reaching Loops. We handle those using phis, just
// like SSA form: any read of a local from a loop entry is turned into a read of
// a phi (for that local + loop). At the end of the flow, after we have seen all
// backedges as well, we can "expand out" those phis (into the combination of
// values arriving from the block before the Loop entry, and the backedges).
//
// For efficiency we also use phis for forward merges as well (e.g. at the end
// of Ifs). The benefit is that it allows our core data structure to represent
// the overall local state as a map of indexes to single values. The single
// value can be a LocalSet* if there is just one, or a phi if there is more than
// one, and the phi then refers to the multiple possible values. This avoids
// copying multiple values all the time.

// We represent phis as LocalSets. We do not need the value there, but we do
// need the index, and also it is convenient to be able to store pointers to
// phis in the same place as sets.
using Phi = LocalSet;

// Maps local indexes to the the LocalSet that writes to them. That is, when
// we reach a LocalGet, it will read from that set. Note that the LocalSet*
// here can be to a phi, which is just a special LocalSet.
//
// We do not store nullptr values here, to save space. That is, when a get
// would read from the parameter of default value at the function entry, then
// LocalGraph represents that as a nullptr (since there is no explicit
// LocalSet), and we do not store such nullptrs here. That avoids us needing
// to fill in nullptrs for all locals, which can waste memory in functions
// with many locals that are used sparsely. Similarly, when |loop| is not null
// then an empty entry here means that we would read the "implicit" value,
// which in a loop is the loop phi. We only construct an actual phi when we
// see such a get, which once more allows us to avoid filling in values for
// all locals eagerly.
// TODO: small map? ordered/unordered?
using IndexSets = std::unordered_map<Index, LocalSet*>;

// The function-level state, such as information about phis.
struct FunctionState;

// The core state that we track in each basic block as we do the forward pass.
struct LocalState {
  // The (top-most, i.e., closest to us) loop we are enclosed in, if there is
  // one, and nullptr if not.
  Loop* loop = nullptr;

  // A shared reference to a map of index sets. A shared reference is useful to
  // reduce memory usage here, because we will have a LocalState for each active
  // basic block. That is, we can "deactivate" blocks (free their memory) once
  // we are totally done with them, but sometimes we need to allocate them ahead
  // of time, say when we reach a branch, then we must track all the things that
  // are send to that branch target. For example, consider an If: the state
  // before the If is passed into the body, and the result of the body is
  // combined with the state before the If to get the new state after it. If
  // there are no actual LocalSets in the If body then we can just keep
  // referring to the same LocalState in this entire process, avoiding any new
  // allocation at all. We do that by copy-on-write: we only allocate a new
  // IndexSets when we modify it.
  //
  // As another example, consider a br_table that branches to 1,000 blocks. We
  // will initially only pass a shared reference to them all, avoiding
  // allocation. If each of those blocks has a set then we will end up
  // allocating, but at least we will only do so when we actually reach that
  // block - by which point, other blocks before it will have been deallocated.
  // That is, lazily allocating can reduce the maximum memory usage here, which
  // can be important given that we may have thousands of locals and thousands
  // of basic blocks.
  std::shared_ptr<IndexSets> indexSets;

  // Apply a given LocalSet to the state, which tramples all LocalSets before
  // it. All later reads from that set's index will read from it.
  void applySet(LocalSet* set) {
    ensureSoleOwnership();
    (*indexSets)[set->index] = set;
  }

  // Given a LocalGet, return the LocalSet for it. The FunctionState is used to
  // handle phis.
  LocalSet* getSet(LocalGet* get, FunctionState& funcState);

  // Given another LocalState, merge its contents into ours. This is done each
  // time we find a branch to a target, for example: each branch brings more
  // possible sets, which we must all merge in.
  void mergeIn(const LocalState& other, FunctionState& funcState);

private:
  // Ensures |indexSets| exists and that we are the sole owner/referrer, so that
  // it is valid for us to add LocalSets to it (which is the only modification
  // we ever do).
  void ensureSoleOwnership() {
    if (!indexSets) {
      // This is the first set here. Allocate a new IndexSets.
      indexSets = std::make_shared<IndexSets>();
    } else if (indexSets.use_count() > 1) {
      // This has multiple users, so before we write we must make a private
      // copy.
      indexSets = std::make_shared<IndexSets>(*indexSets);
    }

    // We now are the sole owner of this data, and can write to it.
    assert(indexSets.use_count() == 1);
  }
};

// The function-level state we track here.
struct FunctionState {
private:
  using PhiSetses = std::unordered_map<Phi*, LocalGraph::Sets>;

  // Map of merge phis to the two sets that they merge.
  PhiSetses mergePhis;

  struct LoopInfo {
    // Map of indexes to phis for that index in this loop.
    std::unordered_map<Index, Phi*> phis;

    // All incoming data, one indexSets for each branch to the loop top.
    std::vector<std::shared_ptr<IndexSets>> indexSetses;
  };

  std::unordered_map<Loop*, LoopInfo> loopInfos;

public:
  // Given two sets, return a phi that combines the two. This is used in control
  // flow merges, like after an If.
  Phi* makeMergePhi(LocalSet* a, LocalSet* b) {
    // We only merge sets of the same index.
    assert(a->index == b->index);
    auto* phi = makePhi(a->index);
    // TODO: consider appending more sets to a given phi, and not always making
    //       a new merge of 2?
    auto& phiSets = mergePhis[phi];
    phiSets.insert(a);
    phiSets.insert(b);
    return phi;
  }

  // Gets a loop phi for a loop + index combination.
  Phi* getLoopPhi(Loop* loop, Index index) {
    auto& loopInfo = loopInfos[loop];
    auto iter = loopInfo.phis.find(index);
    if (iter != loopInfo.phis.end()) {
      return iter->second;
    }

    // Allocate a new phi here, as this is the first use.
    auto* phi = makePhi(index);
    loopInfo.phis[index] = phi;
    return phi;
  }

  // Adds a link to a loop entry. This is called both to link the basic block
  // before the loop, as well as backedges to it. We store all the arriving data
  // for later, when it is used to compute phis.
  void linkLoop(Loop* loop, std::shared_ptr<IndexSets> indexSets) {
    loopInfos[loop].indexSetses.push_back(indexSets);
  }

  // Given a map of gets to sets, expand the phis: some of the sets are phis,
  // and we must replace them with the sets that we know they refer to.
  void expandPhis(LocalGraph::GetSetses& getSetses) {
    // First, gather all the phis to a single map from each phi to the sets it
    // can reach. We can simply move over the merge phis, but have work to
    // convert the loop phis.
    auto allPhis = std::move(mergePhis);
    for (auto& [_, loopInfo] : loopInfos) {
      // Each phi can reach all values in the indexSetses that reach this loop
      // (of the proper index).
      for (auto& [_, phi] : loopInfo.phis) {
        auto& loopPhi = allPhis[phi];
        for (auto& indexSets : loopInfo.indexSetses) {
          if (!indexSets) {
            // This is a read of the function entry value (if this were of a
            // loop value, then a read of a missing element in an indexSet would
            // lead to us creating a phi and filling in the element).
            loopPhi.insert(nullptr);
          }
          auto iter = indexSets->find(phi->index);
          if (iter == indexSets->end()) {
            // See above.
            loopPhi.insert(nullptr);
          } else {
            loopPhi.insert(iter->second);
          }
        }
      }
    }

    auto isPhi = [&](LocalSet* set) { return allPhis.count(set) > 0; };

    // Phis may refer to other phis, and may form loops, so we do a flow
    // operation to find the set of normal LocalSet*s (i.e., that are not phis)
    // for each phi.
    // TODO: It may be worth computing strongly-connected components here and
    //       then doing a topological sort, to avoid repeated work.
    for (auto& [phi, sets] : allPhis) {
      UniqueNonrepeatingDeferredQueue<Phi*> subPhis;
      for (auto* set : sets) {
        if (isPhi(set)) {
          // This is another phi, whose items we will need to add.
          subPhis.push(set);
        }
      }

      if (subPhis.empty()) {
        // No phis referred to, so |sets| is perfect as it is, and we can skip
        // all the below work.
        continue;
      }

      // |sets| contains phis and may also contain non-phi sets as well. We need
      // to expand the phis while keeping the sets. First, remove the phis,
      // which we've added to |subPhis| already.
      LocalGraph::Sets copy(std::move(sets));
      for (auto* set : copy) {
        if (!isPhi(set)) {
          sets.insert(set);
        }
      }

      // Continue to do work while any remains. Note that subPhis is a non-
      // repeating queue, so we don't need to handle cycles here.
      while (!subPhis.empty()) {
        auto* subPhi = subPhis.pop();
        assert(isPhi(subPhi));
        for (auto* set : allPhis[subPhi]) {
          if (isPhi(set)) {
            subPhis.push(set);
          } else {
            sets.insert(set);
          }
        }
      }
    }

    // Phis are now expanded, and we can process getSetses.
    for (auto& [_, sets] : getSetses) {
      std::vector<Phi*> phis;
      for (auto* set : sets) {
        if (isPhi(set)) {
          phis.push_back(set);
        }
      }

      if (phis.empty()) {
        continue;
      }

      // Remove phis from sets.
      std::cout << sets.size() << '\n';
      LocalGraph::Sets copy(std::move(sets));
      std::cout << copy.size() << " : " << sets.size() << '\n';
      assert(sets.empty());
      for (auto* set : copy) {
        if (!isPhi(set)) {
          sets.insert(set);
        }
      }

      // Add values from phis.
      for (auto* phi : phis) {
        for (auto* phiSet : allPhis[phi]) {
          assert(!isPhi(phiSet));
          sets.insert(phiSet);
        }
      }

#if !NDEBUG
      // No phis should remain in the output.
      for (auto* set : sets) {
        assert(!isPhi(set));
      }
#endif
    }
  }

private:
  Phi* makePhi(Index index) {
    auto phi = std::make_unique<LocalSet>();
    phi->index = index;
    // The value does not matter, but must refer to something. We utilize a
    // singleton nop for that purpose (this is intentionally invalid in two
    // ways: it does not have a concrete value, and it will be used in multiple
    // places; that way if this ends up in actual code we will error).
    phi->value = &nop;
    auto* ret = phi.get();
    allocatedSets.push_back(std::move(phi));
    return ret;
  }

  Nop nop;

  std::vector<std::unique_ptr<LocalSet>> allocatedSets;
};

// LocalState implementations (written out here, as they also depend on the
// definition of FunctionState).
LocalSet* LocalState::getSet(LocalGet* get, FunctionState& funcState) {
  if (indexSets) {
    auto iter = indexSets->find(get->index);
    if (iter != indexSets->end()) {
      return iter->second;
    }
  }

  // No entry was found: either we have no indexSets at all, or we have one but
  // it lacks that index, so this LocalGet is reading a default value.
  if (!loop) {
    // The default value outside of a loop is the value from the function entry,
    // which is represented as nullptr.
    return nullptr;
  }

  // Inside a loop, we are reading a phi.
  // TODO: We could in theory stash the loop phi in indexSets, to save this call
  //       later, at the cost of using more memory.
  return funcState.getLoopPhi(loop, get->index);
}

void LocalState::mergeIn(const LocalState& other, FunctionState& funcState) {
  if (indexSets == other.indexSets) {
    // We have the same pointer as |other|, so there is no work to do. This is
    // the common case mentioned before of an If arm with no sets, for
    // example.
    return;
  }

  if (!indexSets) {
    // We have nothing, so just refer to the same data as the other.
    indexSets = other.indexSets;
    return;
  }

  if (!other.indexSets) {
    // The other has nothing to give us.
    return;
  }

  // We only allocate if we actually find a change to write: two different
  // indexSets may contain the same data (internalization could save work here,
  // in theory).
  auto ensuredSoleOwnership = false;
  auto ensure = [&]() {
    if (!ensuredSoleOwnership) {
      ensureSoleOwnership();
      ensuredSoleOwnership = true;
    }
  };

  for (auto& [index, set] : *other.indexSets) {
    auto iter = indexSets->find(index);
    if (iter == indexSets->end()) {
      // We had nothing for this index: just copy.
      ensure();
      (*indexSets)[index] = set;
      return;
    }

    if (iter->second == set) {
      // We had the same value: skip.
      continue;
    }

    // We have different values, so this is a merge that creates a new phi.
    ensure();
    iter->second = funcState.makeMergePhi(iter->second, set);
  }
}

//
// We use CFGWalker here, which has all the logic to figure out where control
// flow joins and splits exist. It normally uses that to build a CFG, which we
// do not need (in theory we could remove the |out|, |in| vectors of edges on
// the BasicBlock struct, but the template magic to do so might not be
// worthwhile).
struct LocalGraphComputer
  : public CFGWalker<LocalGraphComputer, Visitor<LocalGraphComputer>, LocalState> {
  // The data we fill in (see LocalGraph class).
  LocalGraph::GetSetses& getSetses;
  LocalGraph::Locations& locations;

  // The function state we track (phis etc.).
  FunctionState funcState;

  LocalGraphComputer(LocalGraph::GetSetses& getSetses,
                     LocalGraph::Locations& locations)
    : getSetses(getSetses), locations(locations) {}

  // We track all loop entries, mapping them to their loops, as backedges to
  // loops need special handling.
  std::unordered_map<BasicBlock*, Loop*> loopEntries;

  // Loops must set up the state so that phis are used.
  void visitLoop(Loop* curr) {
    if (currBasicBlock) {
      loopEntries[currBasicBlock] = curr;

      currBasicBlock->contents.loop = curr;

      // CFGWalker automatically linked the basic block before us to us, so we
      // now contain the data arriving from outside the loop. Erase that so that
      // we are ready to apply phis as needed, after stashing the information
      // for later.
      auto& indexSets = currBasicBlock->contents.indexSets;
      funcState.linkLoop(curr, indexSets);
      currBasicBlock->contents.indexSets.reset();
    }
  }

  // LocalGet/Set call the proper hooks on LocalState, and append locations.
  void visitLocalGet(LocalGet* curr) {
    if (currBasicBlock) {
      getSetses[curr].insert(currBasicBlock->contents.getSet(curr, funcState));
      locations[curr] = getCurrentPointer();
    }
  }
  void visitLocalSet(LocalSet* curr) {
    if (currBasicBlock) {
      currBasicBlock->contents.applySet(curr);
      locations[curr] = getCurrentPointer();
    }
  }

  // Linking of basic blocks leads to merging of data.
  void doLink(BasicBlock* from, BasicBlock* to) {
    assert(from && to);
    auto iter = loopEntries.find(to);
    if (iter != loopEntries.end()) {
      // Loops are linked in a special way at the function level.
      funcState.linkLoop(iter->second, from->contents.indexSets);
    } else {
      // Other merges are simple.
      to->contents.mergeIn(from->contents, funcState);
    }
  }

  // Finally, when the function is fully processed we can finish up by expanding
  // phis: we have already filled |getSetses|, and the only thing that remains
  // to do is replace any phis with the actual LocalSets that we now know they
  // refer to.
  void visitFunction(Function* curr) { funcState.expandPhis(getSetses); }
};

} // anonymous namespace

// LocalGraph implementation

LocalGraph::LocalGraph(Function* func, Module* module) : func(func) {
  LocalGraphComputer computer(getSetses, locations);
  computer.walkFunction(func);

#ifdef LOCAL_GRAPH_DEBUG
  std::cout << "LocalGraph::dump\n";
  for (auto& [get, sets] : getSetses) {
    std::cout << "GET\n" << get << " is influenced by\n";
    for (auto* set : sets) {
      std::cout << set << '\n';
    }
  }
  std::cout << "total locations: " << locations.size() << '\n';
#endif
}

bool LocalGraph::equivalent(LocalGet* a, LocalGet* b) {
  auto& aSets = getSetses[a];
  auto& bSets = getSetses[b];
  // The simple case of one set dominating two gets easily proves that they must
  // have the same value. (Note that we can infer dominance from the fact that
  // there is a single set: if the set did not dominate one of the gets then
  // there would definitely be another set for that get, the zero initialization
  // at the function entry, if nothing else.)
  if (aSets.size() != 1 || bSets.size() != 1) {
    // TODO: use a LinearExecutionWalker to find trivially equal gets in basic
    //       blocks. that plus the above should handle 80% of cases.
    // TODO: handle chains, merges and other situations
    return false;
  }
  auto* aSet = *aSets.begin();
  auto* bSet = *bSets.begin();
  if (aSet != bSet) {
    return false;
  }
  if (!aSet) {
    // They are both nullptr, indicating the implicit value for a parameter
    // or the zero for a local.
    if (func->isParam(a->index)) {
      // For parameters to be equivalent they must have the exact same
      // index.
      return a->index == b->index;
    } else {
      // As locals, they are both of value zero, but must have the right
      // type as well.
      return func->getLocalType(a->index) == func->getLocalType(b->index);
    }
  } else {
    // They are both the same actual set.
    return true;
  }
}

void LocalGraph::computeSetInfluences() {
  for (auto& [curr, _] : locations) {
    if (auto* get = curr->dynCast<LocalGet>()) {
      for (auto* set : getSetses[get]) {
        setInfluences[set].insert(get);
      }
    }
  }
}

void LocalGraph::computeGetInfluences() {
  for (auto& [curr, _] : locations) {
    if (auto* set = curr->dynCast<LocalSet>()) {
      FindAll<LocalGet> findAll(set->value);
      for (auto* get : findAll.list) {
        getInfluences[get].insert(set);
      }
    }
  }
}

void LocalGraph::computeSSAIndexes() {
  std::unordered_map<Index, std::set<LocalSet*>> indexSets;
  for (auto& [get, sets] : getSetses) {
    for (auto* set : sets) {
      indexSets[get->index].insert(set);
    }
  }
  for (auto& [curr, _] : locations) {
    if (auto* set = curr->dynCast<LocalSet>()) {
      auto& sets = indexSets[set->index];
      if (sets.size() == 1 && *sets.begin() != curr) {
        // While it has just one set, it is not the right one (us),
        // so mark it invalid.
        sets.clear();
      }
    }
  }
  for (auto& [index, sets] : indexSets) {
    if (sets.size() == 1) {
      SSAIndexes.insert(index);
    }
  }
}

bool LocalGraph::isSSA(Index x) { return SSAIndexes.count(x); }

} // namespace wasm
