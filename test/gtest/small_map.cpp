#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "support/small_map.h"
#include "support/utilities.h"
#include "gtest/gtest.h"

using namespace wasm;

template<typename MapType> void testBasics() {
  MapType map;
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.size(), 0u);
  EXPECT_EQ(map.count(1), 0u);
  EXPECT_FALSE(map.contains(1));

  // Insert via operator[]
  map[1] = 100;
  EXPECT_FALSE(map.empty());
  EXPECT_EQ(map.size(), 1u);
  EXPECT_EQ(map.count(1), 1u);
  EXPECT_TRUE(map.contains(1));
  EXPECT_EQ(map[1], 100);
  EXPECT_EQ(map.at(1), 100);

  // Update existing key
  map[1] = 101;
  EXPECT_EQ(map.size(), 1u);
  EXPECT_EQ(map[1], 101);

  // Insert via insert()
  auto [it, inserted] = map.insert({2, 200});
  EXPECT_TRUE(inserted);
  EXPECT_EQ(it->first, 2);
  EXPECT_EQ(it->second, 200);
  EXPECT_EQ(map.size(), 2u);

  // Duplicate insert
  auto [itDup, insertedDup] = map.insert({2, 999});
  EXPECT_FALSE(insertedDup);
  EXPECT_EQ(itDup->first, 2);
  EXPECT_EQ(itDup->second, 200);
  EXPECT_EQ(map.size(), 2u);

  // insert_or_assign
  auto [itAssign, insertedAssign] = map.insert_or_assign(2, 202);
  EXPECT_FALSE(insertedAssign);
  EXPECT_EQ(itAssign->second, 202);
  EXPECT_EQ(map[2], 202);

  auto [itNewAssign, insertedNewAssign] = map.insert_or_assign(3, 300);
  EXPECT_TRUE(insertedNewAssign);
  EXPECT_EQ(itNewAssign->second, 300);
  EXPECT_EQ(map.size(), 3u);

  // try_emplace
  auto [itTry, insertedTry] = map.try_emplace(3, 999);
  EXPECT_FALSE(insertedTry);
  EXPECT_EQ(itTry->second, 300);

  auto [itNewTry, insertedNewTry] = map.try_emplace(4, 400);
  EXPECT_TRUE(insertedNewTry);
  EXPECT_EQ(itNewTry->second, 400);
  EXPECT_EQ(map.size(), 4u);

  // find
  auto findIt = map.find(2);
  ASSERT_NE(findIt, map.end());
  EXPECT_EQ(findIt->first, 2);
  EXPECT_EQ(findIt->second, 202);

  EXPECT_EQ(map.find(9999), map.end());

  // find_or_null utility
  EXPECT_EQ(*find_or_null(map, 2), 202);
  EXPECT_EQ(find_or_null(map, 9999), nullptr);

  // at() with missing key
  EXPECT_THROW(map.at(9999), std::out_of_range);

  // erase by key
  EXPECT_EQ(map.erase(3), 1u);
  EXPECT_EQ(map.erase(3), 0u);
  EXPECT_FALSE(map.contains(3));
  EXPECT_EQ(map.size(), 3u);

  // erase by iterator
  auto eraseIt = map.find(4);
  ASSERT_NE(eraseIt, map.end());
  map.erase(eraseIt);
  EXPECT_FALSE(map.contains(4));
  EXPECT_EQ(map.size(), 2u);

  // clear
  map.clear();
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.size(), 0u);
}

TEST(SmallMapTest, Basics) {
  testBasics<SmallMap<int, int, 0>>();
  testBasics<SmallMap<int, int, 1>>();
  testBasics<SmallMap<int, int, 2>>();
  testBasics<SmallMap<int, int, 3>>();
  testBasics<SmallMap<int, int, 10>>();

  testBasics<SmallUnorderedMap<int, int, 0>>();
  testBasics<SmallUnorderedMap<int, int, 1>>();
  testBasics<SmallUnorderedMap<int, int, 2>>();
  testBasics<SmallUnorderedMap<int, int, 3>>();
  testBasics<SmallUnorderedMap<int, int, 10>>();
}

template<typename MapType> void testThresholds() {
  MapType map;
  EXPECT_TRUE(map.TEST_ONLY_NEVER_USE_usingFixed());

  // Insert 1 item: stays fixed if N >= 1
  map[1] = 10;
  EXPECT_TRUE(map.TEST_ONLY_NEVER_USE_usingFixed());

  // Insert 2nd item: exceeds N=1, switches to flexible
  map[2] = 20;
  EXPECT_FALSE(map.TEST_ONLY_NEVER_USE_usingFixed());

  // Erasing down to 1 item stays flexible
  map.erase(1);
  EXPECT_EQ(map.size(), 1u);
  EXPECT_FALSE(map.TEST_ONLY_NEVER_USE_usingFixed());

  // Erasing all items returns to fixed mode
  map.erase(2);
  EXPECT_TRUE(map.empty());
  EXPECT_TRUE(map.TEST_ONLY_NEVER_USE_usingFixed());

  // Re-inserting starts in fixed mode again
  map[100] = 1000;
  EXPECT_TRUE(map.TEST_ONLY_NEVER_USE_usingFixed());
  EXPECT_EQ(map[100], 1000);
}

TEST(SmallMapTest, Thresholds) {
  testThresholds<SmallMap<int, int, 1>>();
  testThresholds<SmallUnorderedMap<int, int, 1>>();
}

TEST(SmallMapTest, Ordering) {
  SmallMap<int, std::string, 4> map;

  // Insert out of order
  map[30] = "thirty";
  map[10] = "ten";
  map[40] = "forty";
  map[20] = "twenty";

  // Fixed storage is in sorted order
  EXPECT_TRUE(map.TEST_ONLY_NEVER_USE_usingFixed());
  std::vector<int> keys;
  for (const auto& [k, v] : map) {
    keys.push_back(k);
  }
  EXPECT_EQ(keys, std::vector<int>({10, 20, 30, 40}));

  // Switch to flexible storage (N=4, so 5th element switches)
  map[25] = "twenty-five";
  EXPECT_FALSE(map.TEST_ONLY_NEVER_USE_usingFixed());

  keys.clear();
  for (const auto& [k, v] : map) {
    keys.push_back(k);
  }
  EXPECT_EQ(keys, std::vector<int>({10, 20, 25, 30, 40}));

  // Erase from start, middle, and end
  map.erase(10);
  map.erase(25);
  map.erase(40);

  keys.clear();
  for (const auto& [k, v] : map) {
    keys.push_back(k);
  }
  EXPECT_EQ(keys, std::vector<int>({20, 30}));
}

TEST(SmallMapTest, IterationAndModification) {
  SmallMap<int, int, 3> map{{1, 10}, {2, 20}, {3, 30}};

  // Modifying via range-for
  for (auto& [k, v] : map) {
    v += 5;
  }
  EXPECT_EQ(map[1], 15);
  EXPECT_EQ(map[2], 25);
  EXPECT_EQ(map[3], 35);

  // Modifying via iterator
  for (auto it = map.begin(); it != map.end(); ++it) {
    it->second *= 2;
  }
  EXPECT_EQ(map[1], 30);
  EXPECT_EQ(map[2], 50);
  EXPECT_EQ(map[3], 70);

  // Const iteration
  const auto& constMap = map;
  int sum = 0;
  for (const auto& [k, v] : constMap) {
    sum += v;
  }
  EXPECT_EQ(sum, 150);
}

TEST(SmallMapTest, Comparisons) {
  SmallMap<int, int, 2> a{{1, 10}, {2, 20}};
  SmallMap<int, int, 2> b{{2, 20}, {1, 10}};
  EXPECT_EQ(a, b);

  b[3] = 30; // b now has 3 elements (flexible)
  EXPECT_NE(a, b);

  a[3] = 30; // a now also has 3 elements (flexible)
  EXPECT_EQ(a, b);

  b[3] = 999;
  EXPECT_NE(a, b);
}

TEST(SmallMapTest, MoveAndCopy) {
  SmallMap<int, std::string, 2> map{{1, "hello"}, {2, "world"}};

  // Copy construction
  SmallMap<int, std::string, 2> copy(map);
  EXPECT_EQ(copy, map);
  EXPECT_EQ(copy[1], "hello");

  // Move construction
  SmallMap<int, std::string, 2> moved(std::move(copy));
  EXPECT_EQ(moved, map);

  // Copy assignment
  SmallMap<int, std::string, 2> assigned;
  assigned = map;
  EXPECT_EQ(assigned, map);

  // Move assignment
  SmallMap<int, std::string, 2> moveAssigned;
  moveAssigned = std::move(assigned);
  EXPECT_EQ(moveAssigned, map);

  // Swap
  SmallMap<int, std::string, 2> s1{{1, "a"}};
  SmallMap<int, std::string, 2> s2{{2, "b"}};
  swap(s1, s2);
  EXPECT_EQ(s1.size(), 1u);
  EXPECT_EQ(s1[2], "b");
  EXPECT_EQ(s2.size(), 1u);
  EXPECT_EQ(s2[1], "a");
}

TEST(SmallMapTest, NonTrivialAndMoveOnlyTypes) {
  // std::unique_ptr value (move-only)
  SmallMap<int, std::unique_ptr<int>, 2> map;
  map[1] = std::make_unique<int>(100);
  map[2] = std::make_unique<int>(200);
  EXPECT_TRUE(map.TEST_ONLY_NEVER_USE_usingFixed());

  // Grow into flexible storage
  map[3] = std::make_unique<int>(300);
  EXPECT_FALSE(map.TEST_ONLY_NEVER_USE_usingFixed());

  EXPECT_EQ(*map[1], 100);
  EXPECT_EQ(*map[2], 200);
  EXPECT_EQ(*map[3], 300);

  // Move-construct map
  auto map2 = std::move(map);
  EXPECT_EQ(*map2[1], 100);
  EXPECT_EQ(*map2[2], 200);
  EXPECT_EQ(*map2[3], 300);
}
