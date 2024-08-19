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
//
// The following data structure represents the core state that we track as we do
// the forward pass.
struct LocalState {
  // The (top-most, i.e., closest to us) loop we are enclosed in, if there is
  // one, and nullptr if not.
  Loop* loop = nullptr;

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

  // Apply a given LocalSet to the state. All later reads from that set's index
  // will read from it.
  void applySet(LocalSet* set) {
    if (!indexSets) {
      // This is the first set here. Allocate a new IndexSets.
      indexSets = std::make_shared<IndexSets>();
    } else if (indexSets.use_count() > 1) {
      // This has multiple users, so before we write we must make a private
      // copy.
  }

};

// The function-level state we track here.
struct FunctionState {
  phis, loops
};

//
// We use CFGWalker here, which has all the logic to figure out where control
// flow joins and splits exist. It normally uses that to build a 
struct LocalGraphComputer : public CFGWalker<LocalGraphComputer> {
  // The data we fill in (see LocalGraph class).
  LocalGraph::GetSetses& getSetses;
  LocalGraph::Locations& locations;

  // We must handle
  static void scan(SubType* self, Expression** currp) {
    auto* curr = *currp;

    switch (curr->_id) {
      case Expression::Id::BlockId:
      case Expression::Id::IfId:
      case Expression::Id::LoopId:
      case Expression::Id::TryId:
      case Expression::Id::TryTableId: {
        self->pushTask(SubType::doPostVisitControlFlow, currp);
        break;
      }
      default: {
      }
    }

    PostWalker<SubType, VisitorType>::scan(self, currp);

    switch (curr->_id) {
      case Expression::Id::BlockId:
      case Expression::Id::IfId:
      case Expression::Id::LoopId:
      case Expression::Id::TryId:
      case Expression::Id::TryTableId: {
        self->pushTask(SubType::doPreVisitControlFlow, currp);
        break;
      }
      default: {
      }
    }
  }

};

// Information about a basic block.
struct Info {
  // actions occurring in this block: local.gets and local.sets
  std::vector<Expression*> actions;
  // for each index, the last local.set for it
  std::unordered_map<Index, LocalSet*> lastSets;

  void dump(Function* func) {
    std::cout << "    info: " << actions.size() << " actions\n";
  }
};

// flow helper class. flows the gets to their sets

struct Flower : public CFGWalker<Flower, Visitor<Flower>, Info> {

  Flower(LocalGraph::GetSetses& getSetses,
         LocalGraph::Locations& locations,
         Function* func,
         Module* module)
    : getSetses(getSetses), locations(locations) {
    setFunction(func);
    setModule(module);
    // create the CFG by walking the IR
    CFGWalker<Flower, Visitor<Flower>, Info>::doWalkFunction(func);
    // flow gets across blocks
    flow(func);
  }

  BasicBlock* makeBasicBlock() { return new BasicBlock(); }

  // Branches outside of the function can be ignored, as we only look at locals
  // which vanish when we leave.
  bool ignoreBranchesOutsideOfFunc = true;

  // cfg traversal work

  static void doVisitLocalGet(Flower* self, Expression** currp) {
    auto* curr = (*currp)->cast<LocalGet>();
    // if in unreachable code, skip
    if (!self->currBasicBlock) {
      return;
    }
    self->currBasicBlock->contents.actions.emplace_back(curr);
    self->locations[curr] = currp;
  }

  static void doVisitLocalSet(Flower* self, Expression** currp) {
    auto* curr = (*currp)->cast<LocalSet>();
    // if in unreachable code, skip
    if (!self->currBasicBlock) {
      return;
    }
    self->currBasicBlock->contents.actions.emplace_back(curr);
    self->currBasicBlock->contents.lastSets[curr->index] = curr;
    self->locations[curr] = currp;
  }

  void flow(Function* func) {
    // This block struct is optimized for this flow process (Minimal
    // information, iteration index).
    struct FlowBlock {
      // Last Traversed Iteration: This value helps us to find if this block has
      // been seen while traversing blocks. We compare this value to the current
      // iteration index in order to determine if we already process this block
      // in the current iteration. This speeds up the processing compared to
      // unordered_set or other struct usage. (No need to reset internal values,
      // lookup into container, ...)
      size_t lastTraversedIteration;
      std::vector<Expression*> actions;
      std::vector<FlowBlock*> in;
      // Sor each index, the last local.set for it
      // The unordered_map from BasicBlock.Info is converted into a vector
      // This speeds up search as there are usually few sets in a block, so just
      // scanning them linearly is efficient, avoiding hash computations (while
      // in Info, it's convenient to have a map so we can assign them easily,
      // where the last one seen overwrites the previous; and, we do that O(1)).
      std::vector<std::pair<Index, LocalSet*>> lastSets;
    };

    auto numLocals = func->getNumLocals();
    std::vector<FlowBlock*> work;

    // Convert input blocks (basicBlocks) into more efficient flow blocks to
    // improve memory access.
    std::vector<FlowBlock> flowBlocks;
    flowBlocks.resize(basicBlocks.size());

    // Init mapping between basicblocks and flowBlocks
    std::unordered_map<BasicBlock*, FlowBlock*> basicToFlowMap;
    for (Index i = 0; i < basicBlocks.size(); ++i) {
      auto* block = basicBlocks[i].get();
      basicToFlowMap[block] = &flowBlocks[i];
    }

    // We note which local indexes have local.sets, as that can help us
    // optimize later (if there are none at all).
    std::vector<bool> hasSet(numLocals, false);

    const size_t NULL_ITERATION = -1;

    FlowBlock* entryFlowBlock = nullptr;
    for (Index i = 0; i < flowBlocks.size(); ++i) {
      auto& block = basicBlocks[i];
      auto& flowBlock = flowBlocks[i];
      // Get the equivalent block to entry in the flow list
      if (block.get() == entry) {
        entryFlowBlock = &flowBlock;
      }
      flowBlock.lastTraversedIteration = NULL_ITERATION;
      flowBlock.actions.swap(block->contents.actions);
      // Map in block to flow blocks
      auto& in = block->in;
      flowBlock.in.resize(in.size());
      std::transform(in.begin(),
                     in.end(),
                     flowBlock.in.begin(),
                     [&](BasicBlock* block) { return basicToFlowMap[block]; });
      // Convert unordered_map to vector.
      flowBlock.lastSets.reserve(block->contents.lastSets.size());
      for (auto set : block->contents.lastSets) {
        flowBlock.lastSets.emplace_back(set);
        hasSet[set.first] = true;
      }
    }
    assert(entryFlowBlock != nullptr);

    size_t currentIteration = 0;
    for (auto& block : flowBlocks) {
#ifdef LOCAL_GRAPH_DEBUG
      std::cout << "basic block " << &block << " :\n";
      for (auto& action : block.actions) {
        std::cout << "  action: " << *action << '\n';
      }
      for (auto& val : block.lastSets) {
        std::cout << "  last set " << val.second << '\n';
      }
#endif

      // Track all gets in this block, by index.
      std::vector<std::vector<LocalGet*>> allGets(numLocals);

      // go through the block, finding each get and adding it to its index,
      // and seeing how sets affect that
      auto& actions = block.actions;

      // move towards the front, handling things as we go
      for (int i = int(actions.size()) - 1; i >= 0; i--) {
        auto* action = actions[i];
        if (auto* get = action->dynCast<LocalGet>()) {
          allGets[get->index].push_back(get);
        } else {
          // This set is the only set for all those gets.
          auto* set = action->cast<LocalSet>();
          auto& gets = allGets[set->index];
          for (auto* get : gets) {
            getSetses[get].insert(set);
          }
          gets.clear();
        }
      }
      // If anything is left, we must flow it back through other blocks. we
      // can do that for all gets as a whole, they will get the same results.
      for (Index index = 0; index < numLocals; index++) {
        auto& gets = allGets[index];
        if (gets.empty()) {
          continue;
        }
        if (!hasSet[index]) {
          // This local index has no sets, so we know all gets will end up
          // reaching the entry block. Do that here as an optimization to avoid
          // flowing through the (potentially very many) blocks in the function.
          //
          // Note that we may be in unreachable code, and if so, we might add
          // the entry values when they are not actually relevant. That is, we
          // are not precise in the case of unreachable code. This can be
          // confusing when debugging, but it does not have any downside for
          // optimization (since unreachable code should be removed anyhow).
          for (auto* get : gets) {
            getSetses[get].insert(nullptr);
          }
          continue;
        }
        work.push_back(&block);
        // Note that we may need to revisit the later parts of this initial
        // block, if we are in a loop, so don't mark it as seen.
        while (!work.empty()) {
          auto* curr = work.back();
          work.pop_back();
          // We have gone through this block; now we must handle flowing to
          // the inputs.
          if (curr->in.empty()) {
            if (curr == entryFlowBlock) {
              // These receive a param or zero init value.
              for (auto* get : gets) {
                getSetses[get].insert(nullptr);
              }
            }
          } else {
            for (auto* pred : curr->in) {
              if (pred->lastTraversedIteration == currentIteration) {
                // We've already seen pred in this iteration.
                continue;
              }
              pred->lastTraversedIteration = currentIteration;
              auto lastSet =
                std::find_if(pred->lastSets.begin(),
                             pred->lastSets.end(),
                             [&](std::pair<Index, LocalSet*>& value) {
                               return value.first == index;
                             });
              if (lastSet != pred->lastSets.end()) {
                // There is a set here, apply it, and stop the flow.
                for (auto* get : gets) {
                  getSetses[get].insert(lastSet->second);
                }
              } else {
                // Keep on flowing.
                work.push_back(pred);
              }
            }
          }
        }
        currentIteration++;
      }
    }
  }
};

} // anonymous namespace

// LocalGraph implementation

LocalGraph::LocalGraph(Function* func, Module* module) : func(func) {
  Flower flower(getSetses, locations, func, module);

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
