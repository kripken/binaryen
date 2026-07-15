#include "support/small_map.h"
#include "gtest/gtest.h"

using namespace wasm;

using SmallMapTest = ::testing::Test;

TEST_F(SmallMapTest, SmallMapBasics) {
  SmallMap<int, std::string, 3> map;

  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.size(), 0u);
  EXPECT_TRUE(map.TEST_ONLY_NEVER_USE_usingFixed());

  map[1] = "one";
  map[2] = "two";
  EXPECT_EQ(map.size(), 2u);
  EXPECT_TRUE(map.contains(1));
  EXPECT_TRUE(map.contains(2));
  EXPECT_FALSE(map.contains(3));

  EXPECT_EQ(map[1], "one");
  EXPECT_EQ(map.at(2), "two");

  // Erase element
  map.erase(1);
  EXPECT_FALSE(map.contains(1));
  EXPECT_EQ(map.size(), 1u);

  // Clear map
  map.clear();
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.size(), 0u);
}

TEST_F(SmallMapTest, SmallUnorderedMapBasics) {
  SmallUnorderedMap<int, int, 3> map;

  EXPECT_TRUE(map.empty());
  EXPECT_TRUE(map.TEST_ONLY_NEVER_USE_usingFixed());

  map.insert({10, 100});
  map.insert({20, 200});

  EXPECT_EQ(map.size(), 2u);
  EXPECT_EQ(map[10], 100);
  EXPECT_EQ(map[20], 200);

  auto it = map.find(10);
  ASSERT_NE(it, map.end());
  EXPECT_EQ(it->second, 100);

  it->second = 1000;
  EXPECT_EQ(map[10], 1000);
}

TEST_F(SmallMapTest, FixedToFlexibleTransition) {
  // N = 2
  SmallMap<int, int, 2> map;

  EXPECT_TRUE(map.TEST_ONLY_NEVER_USE_usingFixed());
  map[1] = 10;
  map[2] = 20;
  EXPECT_TRUE(map.TEST_ONLY_NEVER_USE_usingFixed());
  EXPECT_EQ(map.size(), 2u);

  // Inserting 3rd element should trigger fallback to flexible storage
  map[3] = 30;
  EXPECT_FALSE(map.TEST_ONLY_NEVER_USE_usingFixed());
  EXPECT_EQ(map.size(), 3u);

  EXPECT_EQ(map[1], 10);
  EXPECT_EQ(map[2], 20);
  EXPECT_EQ(map[3], 30);

  // Test operations in flexible mode
  map.erase(2);
  EXPECT_EQ(map.size(), 2u);
  EXPECT_FALSE(map.contains(2));
  EXPECT_TRUE(map.contains(1));
  EXPECT_TRUE(map.contains(3));

  // Cleared map returns to fixed mode
  map.clear();
  EXPECT_TRUE(map.empty());
  EXPECT_TRUE(map.TEST_ONLY_NEVER_USE_usingFixed());
}

TEST_F(SmallMapTest, InitializerListAndOrderedStorage) {
  SmallMap<int, std::string, 5> map = {{3, "three"}, {1, "one"}, {2, "two"}};

  EXPECT_EQ(map.size(), 3u);
  EXPECT_TRUE(map.TEST_ONLY_NEVER_USE_usingFixed());

  // Ordered storage keeps keys sorted in iteration
  std::vector<int> keys;
  for (const auto& [k, v] : map) {
    keys.push_back(k);
  }
  EXPECT_EQ(keys, std::vector<int>({1, 2, 3}));
}

TEST_F(SmallMapTest, ComparisonOperators) {
  SmallMap<int, int, 3> a = {{1, 10}, {2, 20}};
  SmallMap<int, int, 3> b = {{1, 10}, {2, 20}};
  SmallMap<int, int, 3> c = {{1, 10}, {2, 99}};

  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);

  // Cause b to transition to flexible storage
  b[3] = 30;
  b[4] = 40;
  EXPECT_FALSE(b.TEST_ONLY_NEVER_USE_usingFixed());
  EXPECT_NE(a, b);

  // Make a match b in flexible storage
  a[3] = 30;
  a[4] = 40;
  EXPECT_FALSE(a.TEST_ONLY_NEVER_USE_usingFixed());
  EXPECT_EQ(a, b);
}

TEST_F(SmallMapTest, IterationAndMutation) {
  SmallUnorderedMap<std::string, int, 4> map;
  map["a"] = 1;
  map["b"] = 2;
  map["c"] = 3;

  for (auto& pair : map) {
    pair.second *= 10;
  }

  EXPECT_EQ(map["a"], 10);
  EXPECT_EQ(map["b"], 20);
  EXPECT_EQ(map["c"], 30);
}
