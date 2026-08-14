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

#include "ir/equivalent_sets.h"
#include "gtest/gtest.h"

using namespace wasm;

TEST(EquivalentIndexingTest, Basics) {
  EquivalentIndexing eq;

  // Untracked indexes
  EXPECT_EQ(eq.getLogicalIndex(0), std::nullopt);
  EXPECT_EQ(eq.getLogicalIndex(1), std::nullopt);
  EXPECT_EQ(eq.getLogicalIndex(2), std::nullopt);

  // Self-equivalence holds for all indexes
  EXPECT_TRUE(eq.check(0, 0));
  EXPECT_TRUE(eq.check(1, 1));
  EXPECT_TRUE(eq.check(42, 42));

  // Distinct untracked indexes are not equivalent
  EXPECT_FALSE(eq.check(0, 1));
  EXPECT_FALSE(eq.check(1, 0));
  EXPECT_FALSE(eq.check(0, 2));

  // Add equivalence between 0 and 1
  eq.add(0, 1);
  EXPECT_TRUE(eq.check(0, 1));
  EXPECT_TRUE(eq.check(1, 0));
  EXPECT_TRUE(eq.check(0, 0));
  EXPECT_TRUE(eq.check(1, 1));
  EXPECT_FALSE(eq.check(0, 2));
  EXPECT_FALSE(eq.check(1, 2));

  auto log0 = eq.getLogicalIndex(0);
  auto log1 = eq.getLogicalIndex(1);
  ASSERT_TRUE(log0.has_value());
  ASSERT_TRUE(log1.has_value());
  EXPECT_EQ(*log0, *log1);

  // Add 2 to the existing equivalence class containing 0 (and 1)
  eq.add(2, 0);
  EXPECT_TRUE(eq.check(0, 2));
  EXPECT_TRUE(eq.check(2, 0));
  EXPECT_TRUE(eq.check(1, 2));
  EXPECT_TRUE(eq.check(2, 1));
  EXPECT_TRUE(eq.check(0, 1));

  auto log2 = eq.getLogicalIndex(2);
  ASSERT_TRUE(log2.has_value());
  EXPECT_EQ(*log2, *log0);
}

TEST(EquivalentIndexingTest, MultipleGroups) {
  EquivalentIndexing eq;

  // Group A: {0, 1, 2}
  eq.add(0, 1);
  eq.add(2, 0);

  // Group B: {10, 11, 12}
  eq.add(10, 11);
  eq.add(12, 11);

  // Group C: {20, 21}
  eq.add(20, 21);

  // Check intra-group equivalences
  const std::vector<Index> groupA = {0, 1, 2};
  for (auto a : groupA) {
    for (auto b : groupA) {
      EXPECT_TRUE(eq.check(a, b));
    }
  }

  const std::vector<Index> groupB = {10, 11, 12};
  for (auto a : groupB) {
    for (auto b : groupB) {
      EXPECT_TRUE(eq.check(a, b));
    }
  }

  const std::vector<Index> groupC = {20, 21};
  for (auto a : groupC) {
    for (auto b : groupC) {
      EXPECT_TRUE(eq.check(a, b));
    }
  }

  // Check inter-group non-equivalences
  for (auto a : groupA) {
    for (auto b : groupB) {
      EXPECT_FALSE(eq.check(a, b));
      EXPECT_FALSE(eq.check(b, a));
    }
    for (auto c : groupC) {
      EXPECT_FALSE(eq.check(a, c));
      EXPECT_FALSE(eq.check(c, a));
    }
  }
  for (auto b : groupB) {
    for (auto c : groupC) {
      EXPECT_FALSE(eq.check(b, c));
      EXPECT_FALSE(eq.check(c, b));
    }
  }

  // Logical indexes must differ between groups
  EXPECT_NE(*eq.getLogicalIndex(0), *eq.getLogicalIndex(10));
  EXPECT_NE(*eq.getLogicalIndex(0), *eq.getLogicalIndex(20));
  EXPECT_NE(*eq.getLogicalIndex(10), *eq.getLogicalIndex(20));
}

TEST(EquivalentIndexingTest, Reset) {
  EquivalentIndexing eq;

  // Group: {0, 1, 2}
  eq.add(0, 1);
  eq.add(2, 0);

  EXPECT_TRUE(eq.check(0, 1));
  EXPECT_TRUE(eq.check(1, 2));
  EXPECT_TRUE(eq.check(0, 2));

  // Reset an element in the middle
  eq.reset(1);
  EXPECT_EQ(eq.getLogicalIndex(1), std::nullopt);
  EXPECT_TRUE(eq.check(1, 1)); // Reflexivity remains
  EXPECT_FALSE(eq.check(0, 1));
  EXPECT_FALSE(eq.check(1, 0));
  EXPECT_FALSE(eq.check(2, 1));
  EXPECT_FALSE(eq.check(1, 2));
  // 0 and 2 should still be equivalent
  EXPECT_TRUE(eq.check(0, 2));
  EXPECT_TRUE(eq.check(2, 0));

  // Reset 0
  eq.reset(0);
  EXPECT_EQ(eq.getLogicalIndex(0), std::nullopt);
  EXPECT_FALSE(eq.check(0, 2));
  EXPECT_FALSE(eq.check(2, 0));
  // 2 is still tracked
  EXPECT_NE(eq.getLogicalIndex(2), std::nullopt);

  // Reset 2
  eq.reset(2);
  EXPECT_EQ(eq.getLogicalIndex(2), std::nullopt);

  // Resetting an untracked index is safe
  eq.reset(99);
  EXPECT_EQ(eq.getLogicalIndex(99), std::nullopt);
}

TEST(EquivalentIndexingTest, ReAddAfterReset) {
  EquivalentIndexing eq;

  // Group 1: {0, 1}, Group 2: {10, 11}
  eq.add(0, 1);
  eq.add(10, 11);

  EXPECT_TRUE(eq.check(0, 1));
  EXPECT_FALSE(eq.check(1, 10));

  // Reset 1 and add it to Group 2
  eq.reset(1);
  eq.add(1, 10);

  EXPECT_FALSE(eq.check(0, 1));
  EXPECT_TRUE(eq.check(1, 10));
  EXPECT_TRUE(eq.check(1, 11));
  EXPECT_EQ(*eq.getLogicalIndex(1), *eq.getLogicalIndex(10));
  EXPECT_EQ(*eq.getLogicalIndex(1), *eq.getLogicalIndex(11));
}

TEST(EquivalentIndexingTest, Clear) {
  EquivalentIndexing eq;

  eq.add(0, 1);
  eq.add(2, 0);
  eq.add(3, 4);

  EXPECT_TRUE(eq.check(0, 1));
  EXPECT_TRUE(eq.check(3, 4));

  eq.clear();

  EXPECT_EQ(eq.getLogicalIndex(0), std::nullopt);
  EXPECT_EQ(eq.getLogicalIndex(1), std::nullopt);
  EXPECT_EQ(eq.getLogicalIndex(2), std::nullopt);
  EXPECT_EQ(eq.getLogicalIndex(3), std::nullopt);
  EXPECT_EQ(eq.getLogicalIndex(4), std::nullopt);

  EXPECT_FALSE(eq.check(0, 1));
  EXPECT_FALSE(eq.check(0, 2));
  EXPECT_FALSE(eq.check(3, 4));
  EXPECT_TRUE(eq.check(0, 0));
  EXPECT_TRUE(eq.check(3, 3));

  // Can add new equivalences after clear
  eq.add(0, 3);
  EXPECT_TRUE(eq.check(0, 3));
  EXPECT_FALSE(eq.check(0, 1));
}

TEST(EquivalentIndexingTest, DifferentialWithEquivalentSets) {
  // Verify that EquivalentIndexing and EquivalentSets exhibit identical
  // check(a, b) behavior across arbitrary sequences of operations.
  EquivalentIndexing indexing;
  EquivalentSets sets;

  constexpr Index numLocals = 8;
  std::vector<bool> active(numLocals, false);

  auto checkAllPairs = [&]() {
    for (Index i = 0; i < numLocals; ++i) {
      for (Index j = 0; j < numLocals; ++j) {
        bool indexingRes = indexing.check(i, j);
        bool setsRes = sets.check(i, j);
        EXPECT_EQ(indexingRes, setsRes)
          << "Mismatch at check(" << i << ", " << j << ")";
      }
    }
  };

  // Simple pseudo-random LCG for deterministic test execution
  uint32_t state = 42;
  auto nextRand = [&]() -> uint32_t {
    state = state * 1664525u + 1013904223u;
    return state;
  };

  for (size_t iter = 0; iter < 200; ++iter) {
    uint32_t op = nextRand() % 10;
    if (op < 6) {
      // Add: pick an inactive (untracked) index as `justReset`, and an
      // arbitrary other index
      std::vector<Index> inactiveIndices;
      for (Index i = 0; i < numLocals; ++i) {
        if (!active[i]) {
          inactiveIndices.push_back(i);
        }
      }
      if (!inactiveIndices.empty()) {
        Index justReset = inactiveIndices[nextRand() % inactiveIndices.size()];
        Index other = nextRand() % numLocals;
        if (justReset != other) {
          indexing.add(justReset, other);
          sets.add(justReset, other);
          active[justReset] = true;
          active[other] = true;
        }
      }
    } else if (op < 9) {
      // Reset an index
      Index idx = nextRand() % numLocals;
      indexing.reset(idx);
      sets.reset(idx);
      active[idx] = false;
    } else {
      // Clear all
      indexing.clear();
      sets.clear();
      std::fill(active.begin(), active.end(), false);
    }

    checkAllPairs();
  }
}

TEST(EquivalentSetsTest, Basics) {
  EquivalentSets sets;

  EXPECT_TRUE(sets.check(0, 0));
  EXPECT_FALSE(sets.check(0, 1));
  EXPECT_EQ(sets.getEquivalents(0), nullptr);

  sets.add(0, 1);
  EXPECT_TRUE(sets.check(0, 1));
  EXPECT_TRUE(sets.check(1, 0));
  EXPECT_FALSE(sets.check(0, 2));

  auto* eq0 = sets.getEquivalents(0);
  ASSERT_NE(eq0, nullptr);
  EXPECT_EQ(*eq0, (EquivalentSets::Set{0, 1}));

  sets.add(2, 0);
  EXPECT_TRUE(sets.check(0, 2));
  EXPECT_TRUE(sets.check(1, 2));
  EXPECT_EQ(*sets.getEquivalents(0), (EquivalentSets::Set{0, 1, 2}));

  sets.reset(1);
  EXPECT_FALSE(sets.check(0, 1));
  EXPECT_TRUE(sets.check(0, 2));
  EXPECT_EQ(sets.getEquivalents(1), nullptr);
  EXPECT_EQ(*sets.getEquivalents(0), (EquivalentSets::Set{0, 2}));

  sets.clear();
  EXPECT_FALSE(sets.check(0, 2));
  EXPECT_EQ(sets.getEquivalents(0), nullptr);
}
