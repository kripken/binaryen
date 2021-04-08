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

#include <cfg/cfg-traversal.h>
#include <ir/local-graph.h>
#include <wasm-builder.h>

namespace wasm {

namespace {

// Information about a basic block.
struct Info {
  // Actions occurring in this block: uses and defs.
  std::vector<Expression*> actions;
  // For each lane, the last def for it.
  std::unordered_map<Index, Expression*> lastDefs;
};

// Flow helper class: flows the uses to their defs.

struct Flower
  : public CFGWalker<Flower, UnifiedExpressionVisitor<Flower>, Info> {
  UseDefAnalysis& analysis;

  std::map<Expression*, Expression**> locations;

  // The number of lanes. The wildcard lane is implemented as an extra lane at
  // the end.
  Index numLanes;

  Flower(UseDefAnalysis& analysis, Function* func) : analysis(analysis) {
    numLanes = analysis.getNumLanes();
    this->setFunction(func);
    // create the CFG by walking the IR
    CFGWalker<Flower, UnifiedExpressionVisitor<Flower>, Info>::doWalkFunction(
      func);
    // flow uses across blocks
    flow(func);
  }

  BasicBlock* makeBasicBlock() { return new BasicBlock(); }

  Index getLane(Expression* curr) {
    auto lane = analysis.getLane(curr);
    // The wildcard lane is implemented as the very last lane at the end.
    if (lane == UseDefAnalysis::WildcardLane) {
      return numLanes;
    }
    return lane;
  }

  // cfg traversal work

  void visitExpression(Expression* curr) {
    // if in unreachable code, skip
    if (!currBasicBlock) {
      return;
    }
    if (analysis.isUse(curr) || analysis.isDef(curr)) {
      currBasicBlock->contents.actions.emplace_back(curr);
      locations[curr] = getCurrentPointer();
      if (analysis.isDef(curr)) {
        currBasicBlock->contents.lastDefs[getLane(curr)] = curr;
      }
    }
  }

  void flow(Function* func) {
    // This block struct is optimized for this flow process (Minimal
    // information, iteration index).
    struct FlowBlock {
      // Last Traversed Iteration: This value helps us to find if this block
      // has been seen while traversing blocks. We compare this value to the
      // current iteration index in order to determine if we already process
      // this block in the current iteration. This speeds up the processing
      // compared to unordered_set or other struct usage. (No need to reset
      // internal values, lookup into container, ...)
      size_t lastTraversedIteration;
      std::vector<Expression*> actions;
      std::vector<FlowBlock*> in;
      // Sor each index, the last def for it.
      // The unordered_map from BasicBlock.Info is converted into a vector
      // This speeds up search as there are usually few defs in a block, so
      // just scanning them linearly is efficient, avoiding hash computations
      // (while in Info, it's convenient to have a map so we can assign them
      // easily, where the last one seen overwrites the previous; and, we do
      // that O(1)).
      std::vector<std::pair<Index, Expression*>> lastDefs;
    };

    std::vector<std::vector<Expression*>> allUses;
    allUses.resize(numLanes + 1);
    std::vector<FlowBlock*> work;

    // Convert input blocks (basicBlocks) into more efficient flow blocks to
    // improve memory access.
    std::vector<FlowBlock> flowBlocks;
    flowBlocks.resize(basicBlocks.size());

    // Init mapping between basicblocks and flowBlocks
    std::unordered_map<BasicBlock*, FlowBlock*> basicToFlowMap;
    for (Index i = 0; i < basicBlocks.size(); ++i) {
      basicToFlowMap[basicBlocks[i].get()] = &flowBlocks[i];
    }

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
      flowBlock.lastDefs.reserve(block->contents.lastDefs.size());
      for (auto def : block->contents.lastDefs) {
        flowBlock.lastDefs.emplace_back(std::make_pair(def.first, def.second));
      }
    }
    assert(entryFlowBlock != nullptr);

    size_t currentIteration = 0;
    for (auto& block : flowBlocks) {
#ifdef LOCAL_GRAPH_DEBUG
      std::cout << "basic block " << block.get() << " :\n";
      for (auto& action : block->contents.actions) {
        std::cout << "  action: " << *action << '\n';
      }
      for (auto* lastDef : block->contents.lastDefs) {
        std::cout << "  last def " << lastDef << '\n';
      }
#endif
      // go through the block, finding each get and adding it to its index,
      // and seeing how defs affect that
      auto& actions = block.actions;
      // move towards the front, handling things as we go
      for (int i = int(actions.size()) - 1; i >= 0; i--) {
        auto* action = actions[i];
        if (analysis.isUse(action)) {
          allUses[getLane(action)].push_back(action);
        } else {
          assert(analysis.isDef(action));

          // This def is the only def for all those uses.
          auto& uses = allUses[getLane(action)];
          for (auto* use : uses) {
            analysis.noteUseDef(use, action);
          }
          uses.clear();
        }
      }
      // If anything is left, we must flow it back through other blocks. we
      // can do that for all uses as a whole, they will use the same results.
      for (Index lane = 0; lane < numLanes + 1; lane++) {
        auto& uses = allUses[lane];
        if (uses.empty()) {
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
              for (auto* use : uses) {
                analysis.noteUseDef(use, nullptr);
              }
            }
          } else {
            for (auto* pred : curr->in) {
              if (pred->lastTraversedIteration == currentIteration) {
                // We've already seen pred in this iteration.
                continue;
              }
              pred->lastTraversedIteration = currentIteration;
              auto lastDef =
                std::find_if(pred->lastDefs.begin(),
                             pred->lastDefs.end(),
                             [&](std::pair<Index, Expression*>& value) {
                               return value.first == lane;
                             });
              if (lastDef != pred->lastDefs.end()) {
                // There is a def here, apply it, and stop the flow.
                for (auto* use : uses) {
                  analysis.noteUseDef(use, lastDef->second);
                }
              } else {
                // Keep on flowing.
                work.push_back(pred);
              }
            }
          }
        }
        uses.clear();
        currentIteration++;
      }
    }
  }
};

} // anonymous namespace

// UseDefAnalysis implementation

void UseDefAnalysis::analyze(Function* func) {
  Flower flower(*this, func);

  locations = std::move(flower.locations);

#ifdef LOCAL_GRAPH_DEBUG
  std::cout << "UseDefAnalysis::dump\n";
  for (auto& pair : useDefs) {
    auto* use = pair.first;
    auto& defs = pair.second;
    std::cout << "USE\n" << use << " is influenced by\n";
    for (auto* def : defs) {
      std::cout << def << '\n';
    }
  }
  std::cout << "total locations: " << locations.size() << '\n';
#endif
}

// LocalGraph implementation

LocalGraph::LocalGraph(Function* func) : func(func) {
  analyze(func);
}

bool LocalGraph::isUse(Expression* curr) { return curr->is<LocalGet>(); }

bool LocalGraph::isDef(Expression* curr) { return curr->is<LocalSet>(); }

Index LocalGraph::getLane(Expression* curr) {
  if (auto* get = curr->dynCast<LocalGet>()) {
    return get->index;
  } else if (auto* set = curr->dynCast<LocalSet>()) {
    return set->index;
  }
  WASM_UNREACHABLE("bad use-def expr");
}

Index LocalGraph::getNumLanes() { return func->getNumLocals(); }

void LocalGraph::noteUseDef(Expression* use, Expression* def) {
  useDefs[use->cast<LocalGet>()].insert(def ? def->cast<LocalSet>()
                                         : nullptr);
}

void LocalGraph::computeInfluences() {
  for (auto& pair : locations) {
    auto* curr = pair.first;
    if (auto* def = curr->dynCast<LocalSet>()) {
      FindAll<LocalGet> findAll(def->value);
      for (auto* use : findAll.list) {
        useInfluences[use].insert(def);
      }
    } else {
      auto* use = curr->cast<LocalGet>();
      for (auto* def : useDefs[use]) {
        defInfluences[def].insert(use);
      }
    }
  }
}

void LocalGraph::computeSSAIndexes() {
  std::unordered_map<Index, std::set<LocalSet*>> indexSets;
  for (auto& pair : useDefs) {
    auto* get = pair.first;
    auto& sets = pair.second;
    for (auto* set : sets) {
      indexSets[get->index].insert(set);
    }
  }
  for (auto& pair : locations) {
    auto* curr = pair.first;
    if (auto* set = curr->dynCast<LocalSet>()) {
      auto& sets = indexSets[set->index];
      if (sets.size() == 1 && *sets.begin() != curr) {
        // While it has just one set, it is not the right one (us),
        // so mark it invalid.
        sets.clear();
      }
    }
  }
  for (auto& pair : indexSets) {
    auto index = pair.first;
    auto& sets = pair.second;
    if (sets.size() == 1) {
      SSAIndexes.insert(index);
    }
  }
}

bool LocalGraph::isSSA(Index x) { return SSAIndexes.count(x); }

} // namespace wasm
