/*
 * Copyright 2018 WebAssembly Community Group participants
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

#ifndef wasm_ir_equivalent_sets_h
#define wasm_ir_equivalent_sets_h

#include <wasm.h>

namespace wasm {

// A map of each index to all those it is equivalent to, and some helpers.
struct EquivalentSets {
  // A set of indexes. This is ordered for deterministic iteration.
  using Set = std::set<Index>;

  std::unordered_map<Index, std::shared_ptr<Set>> indexSets;

  // Clears the state completely, removing all equivalences.
  void clear() { indexSets.clear(); }

  // Resets an index, removing any equivalences between it and others.
  void reset(Index index) {
    auto iter = indexSets.find(index);
    if (iter != indexSets.end()) {
      auto& set = iter->second;
      assert(!set->empty()); // can't be empty - we are equal to ourselves!
      if (set->size() > 1) {
        // We are not the last item, fix things up
        set->erase(index);
      }
      indexSets.erase(iter);
    }
  }

  // Adds a new equivalence between two indexes.
  // `justReset` is an index that was just reset, and has no
  // equivalences. `other` may have existing equivalences.
  void add(Index justReset, Index other) {
    auto iter = indexSets.find(other);
    if (iter != indexSets.end()) {
      auto& set = iter->second;
      set->insert(justReset);
      indexSets[justReset] = set;
    } else {
      auto set = std::make_shared<Set>();
      set->insert(justReset);
      set->insert(other);
      indexSets[justReset] = set;
      indexSets[other] = set;
    }
  }

  // Checks whether two indexes contain the same data.
  bool check(Index a, Index b) {
    if (a == b) {
      return true;
    }
    if (auto* set = getEquivalents(a)) {
      if (set->find(b) != set->end()) {
        return true;
      }
    }
    return false;
  }

  // Returns the equivalent set, or nullptr
  Set* getEquivalents(Index index) {
    auto iter = indexSets.find(index);
    if (iter != indexSets.end()) {
      return iter->second.get();
    }
    return nullptr;
  }
};

// Similar to EquivalentSets, but does not allow for iteration over the
// equivalent indexes to a particular index, i.e., getEquivalents() is not
// provided. Instead, this focuses on providing an *indexing* of the locals,
// where equivalent ones get the same index. This makes checking if any two
// given indexes are equal a fast operation.
struct EquivalentIndexing {
  // Mapping of input indexes to output/logical indexes. Two input indexes
  // with the same logical index are equivalent.
  std::unordered_map<Index, Index> map;

  // Clears the state completely, removing all equivalences.
  void clear() { map.clear(); }

  // Resets an index, removing any equivalences between it and others.
  void reset(Index index) { map.erase(index); }

  // For convenience, start logical indexes from 1, which simplifies the code
  // below. We never reuse indexes, which means this is not suitable for
  // extremely-long-running tasks.
  Index nextLogicalIndex = 1;

  // Adds a new equivalence between two indexes.
  // `justReset` is an index that was just reset, and has no
  // equivalences. `other` may have existing equivalences.
  void add(Index justReset, Index other) {
    assert(!map.contains(justReset));

    auto& otherLogicalIndex = map[other];
    if (otherLogicalIndex != 0) {
      // Use `other`'s existing index.
      map[justReset] = otherLogicalIndex;
    } else {
      // There was no prior index for `other`.
      assert(nextLogicalIndex != std::numeric_limits<Index>::max());
      map[justReset] = otherLogicalIndex = nextLogicalIndex++;
    }
  }

  std::optional<Index> getLogicalIndex(Index index) const {
    auto iter = map.find(index);
    if (iter != map.end()) {
      return iter->second;
    }
    return {};
  }

  // Checks whether two indexes are equivalent.
  bool check(Index a, Index b) const {
    if (a == b) {
      // An index is always equivalent to itself.
      return true;
    }

    if (auto aLogical = getLogicalIndex(a)) {
      if (auto bLogical = getLogicalIndex(b)) {
        // We return true if we know a logical index for both, and they match.
        return *aLogical == *bLogical;
      }
    }

    // Otherwise, we can't say they are equivalent.
    return false;
  }

  bool operator!=(const EquivalentIndexing& other) const {
    // TODO: the actual indexes don't matter, just the shape
    return map != other.map;
  }
};

} // namespace wasm

#endif // wasm_ir_equivalent_sets_h
