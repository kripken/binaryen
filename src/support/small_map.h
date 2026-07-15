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
// A map of elements, which is often small. While the number of items is small,
// the implementation simply stores them in an array that is linearly looked
// through. Once the size is large enough, we switch to using a std::map or
// std::unordered_map.
//

#ifndef wasm_support_small_map_h
#define wasm_support_small_map_h

#include <algorithm>
#include <array>
#include <cassert>
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "support/small_set.h"
#include "utilities.h"

namespace wasm {

template<typename Key,
         typename Value,
         size_t N,
         typename FixedStorage,
         typename FlexibleMap>
class SmallMapBase {
  FixedStorage fixed;
  FlexibleMap flexible;

  bool usingFixed() const { return flexible.empty(); }

public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<const Key, Value>;
  using size_type = size_t;
  using map_type = FlexibleMap;

  SmallMapBase() {}

  SmallMapBase(std::initializer_list<std::pair<Key, Value>> init) {
    for (const auto& item : init) {
      insert(item);
    }
  }

  Value& operator[](const Key& key) {
    if (usingFixed()) {
      if (auto* pair = fixed.find(key)) {
        return pair->second;
      }
      if (fixed.insert(std::make_pair(key, Value())) ==
          FixedStorage::InsertResult::CouldNotInsert) {
        assert(fixed.used == N);
        assert(flexible.empty());
        flexible.insert(fixed.storage.begin(),
                        fixed.storage.begin() + fixed.used);
        fixed.used = 0;
        return flexible[key];
      }
      return fixed.find(key)->second;
    } else {
      return flexible[key];
    }
  }

  Value& operator[](Key&& key) {
    if (usingFixed()) {
      if (auto* pair = fixed.find(key)) {
        return pair->second;
      }
      Key copyKey = key;
      if (fixed.insert(std::make_pair(std::move(key), Value())) ==
          FixedStorage::InsertResult::CouldNotInsert) {
        assert(fixed.used == N);
        assert(flexible.empty());
        flexible.insert(fixed.storage.begin(),
                        fixed.storage.begin() + fixed.used);
        fixed.used = 0;
        return flexible[std::move(copyKey)];
      }
      return fixed.find(copyKey)->second;
    } else {
      return flexible[std::move(key)];
    }
  }

  Value& at(const Key& key) {
    if (usingFixed()) {
      if (auto* pair = fixed.find(key)) {
        return pair->second;
      }
      throw std::out_of_range("SmallMap::at key not found");
    } else {
      return flexible.at(key);
    }
  }

  const Value& at(const Key& key) const {
    if (usingFixed()) {
      if (const auto* pair = fixed.find(key)) {
        return pair->second;
      }
      throw std::out_of_range("SmallMap::at key not found");
    } else {
      return flexible.at(key);
    }
  }

  struct Iterator;
  struct ConstIterator;

  std::pair<Iterator, bool> insert(const std::pair<Key, Value>& pair) {
    if (usingFixed()) {
      if (auto* p = fixed.find(pair.first)) {
        size_t idx = p - &fixed.storage[0];
        Iterator it(this);
        it.usingFixed = true;
        it.fixedIndex = idx;
        return {it, false};
      }
      if (fixed.insert(pair) == FixedStorage::InsertResult::CouldNotInsert) {
        assert(fixed.used == N);
        assert(flexible.empty());
        flexible.insert(fixed.storage.begin(),
                        fixed.storage.begin() + fixed.used);
        auto res = flexible.insert(pair);
        assert(!usingFixed());
        fixed.used = 0;
        Iterator it(this);
        it.usingFixed = false;
        it.flexibleIterator = res.first;
        return {it, res.second};
      } else {
        return {find(pair.first), true};
      }
    } else {
      auto res = flexible.insert(pair);
      Iterator it(this);
      it.usingFixed = false;
      it.flexibleIterator = res.first;
      return {it, res.second};
    }
  }

  std::pair<Iterator, bool> insert(std::pair<Key, Value>&& pair) {
    Key keyCopy = pair.first;
    if (usingFixed()) {
      if (auto* p = fixed.find(keyCopy)) {
        size_t idx = p - &fixed.storage[0];
        Iterator it(this);
        it.usingFixed = true;
        it.fixedIndex = idx;
        return {it, false};
      }
      if (fixed.insert(std::move(pair)) ==
          FixedStorage::InsertResult::CouldNotInsert) {
        assert(fixed.used == N);
        assert(flexible.empty());
        flexible.insert(fixed.storage.begin(),
                        fixed.storage.begin() + fixed.used);
        auto res = flexible.insert(std::make_pair(std::move(keyCopy), Value()));
        assert(!usingFixed());
        fixed.used = 0;
        Iterator it(this);
        it.usingFixed = false;
        it.flexibleIterator = res.first;
        return {it, res.second};
      } else {
        return {find(keyCopy), true};
      }
    } else {
      auto res = flexible.insert(std::move(pair));
      Iterator it(this);
      it.usingFixed = false;
      it.flexibleIterator = res.first;
      return {it, res.second};
    }
  }

  template<typename InputIt> void insert(InputIt first, InputIt last) {
    for (auto it = first; it != last; ++it) {
      insert(*it);
    }
  }

  void insert_or_assign(const Key& key, const Value& val) {
    (*this)[key] = val;
  }

  void insert_or_assign(const Key& key, Value&& val) {
    (*this)[key] = std::move(val);
  }

  void erase(const Key& key) {
    if (usingFixed()) {
      fixed.erase(key);
    } else {
      flexible.erase(key);
    }
  }

  void erase(ConstIterator it) {
    if (it.usingFixed) {
      erase(it->first);
    } else {
      flexible.erase(it.flexibleIterator);
    }
  }

  bool contains(const Key& key) const {
    if (usingFixed()) {
      return fixed.find(key) != nullptr;
    } else {
      return flexible.contains(key);
    }
  }

  size_t count(const Key& key) const { return contains(key) ? 1 : 0; }

  size_t size() const {
    if (usingFixed()) {
      return fixed.used;
    } else {
      return flexible.size();
    }
  }

  bool empty() const { return size() == 0; }

  void clear() {
    fixed.used = 0;
    flexible.clear();
  }

  bool operator==(
    const SmallMapBase<Key, Value, N, FixedStorage, FlexibleMap>& other)
    const {
    if (size() != other.size()) {
      return false;
    }
    if (usingFixed()) {
      return std::all_of(fixed.storage.begin(),
                         fixed.storage.begin() + fixed.used,
                         [&other](const std::pair<Key, Value>& pair) {
                           auto it = other.find(pair.first);
                           return it != other.end() && it->second == pair.second;
                         });
    } else if (other.usingFixed()) {
      return std::all_of(other.fixed.storage.begin(),
                         other.fixed.storage.begin() + other.fixed.used,
                         [this](const std::pair<Key, Value>& pair) {
                           auto it = find(pair.first);
                           return it != end() && it->second == pair.second;
                         });
    } else {
      return flexible == other.flexible;
    }
  }

  bool operator!=(
    const SmallMapBase<Key, Value, N, FixedStorage, FlexibleMap>& other)
    const {
    return !(*this == other);
  }

  // Iteration

  struct Iterator {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = value_type*;
    using reference = value_type&;

    SmallMapBase* parent = nullptr;
    bool usingFixed = true;
    size_t fixedIndex = 0;
    typename FlexibleMap::iterator flexibleIterator;

    Iterator() = default;
    Iterator(SmallMapBase* parent)
      : parent(parent), usingFixed(parent ? parent->usingFixed() : true),
        fixedIndex(0) {}

    void setBegin() {
      if (usingFixed) {
        fixedIndex = 0;
      } else {
        flexibleIterator = parent->flexible.begin();
      }
    }

    void setEnd() {
      if (usingFixed) {
        fixedIndex = parent->size();
      } else {
        flexibleIterator = parent->flexible.end();
      }
    }

    bool operator==(const Iterator& other) const {
      if (parent != other.parent) {
        return false;
      }
      if (usingFixed != other.usingFixed) {
        Fatal() << "SmallMap does not support changes while iterating";
      }
      if (usingFixed) {
        return fixedIndex == other.fixedIndex;
      } else {
        return flexibleIterator == other.flexibleIterator;
      }
    }

    bool operator!=(const Iterator& other) const { return !(*this == other); }

    Iterator& operator++() {
      if (usingFixed) {
        fixedIndex++;
      } else {
        flexibleIterator++;
      }
      return *this;
    }

    Iterator operator++(int) {
      Iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    reference operator*() const {
      if (usingFixed) {
        return *reinterpret_cast<pointer>(
          &parent->fixed.storage[fixedIndex]);
      } else {
        return *flexibleIterator;
      }
    }

    pointer operator->() const {
      if (usingFixed) {
        return reinterpret_cast<pointer>(&parent->fixed.storage[fixedIndex]);
      } else {
        return flexibleIterator.operator->();
      }
    }
  };

  struct ConstIterator {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    const SmallMapBase* parent = nullptr;
    bool usingFixed = true;
    size_t fixedIndex = 0;
    typename FlexibleMap::const_iterator flexibleIterator;

    ConstIterator() = default;
    ConstIterator(const SmallMapBase* parent)
      : parent(parent), usingFixed(parent ? parent->usingFixed() : true),
        fixedIndex(0) {}

    ConstIterator(const Iterator& other)
      : parent(other.parent), usingFixed(other.usingFixed),
        fixedIndex(other.fixedIndex) {
      if (!usingFixed) {
        flexibleIterator = other.flexibleIterator;
      }
    }

    void setBegin() {
      if (usingFixed) {
        fixedIndex = 0;
      } else {
        flexibleIterator = parent->flexible.begin();
      }
    }

    void setEnd() {
      if (usingFixed) {
        fixedIndex = parent->size();
      } else {
        flexibleIterator = parent->flexible.end();
      }
    }

    bool operator==(const ConstIterator& other) const {
      if (parent != other.parent) {
        return false;
      }
      if (usingFixed != other.usingFixed) {
        Fatal() << "SmallMap does not support changes while iterating";
      }
      if (usingFixed) {
        return fixedIndex == other.fixedIndex;
      } else {
        return flexibleIterator == other.flexibleIterator;
      }
    }

    bool operator!=(const ConstIterator& other) const {
      return !(*this == other);
    }

    ConstIterator& operator++() {
      if (usingFixed) {
        fixedIndex++;
      } else {
        flexibleIterator++;
      }
      return *this;
    }

    ConstIterator operator++(int) {
      ConstIterator tmp = *this;
      ++(*this);
      return tmp;
    }

    reference operator*() const {
      if (usingFixed) {
        return *reinterpret_cast<pointer>(
          &parent->fixed.storage[fixedIndex]);
      } else {
        return *flexibleIterator;
      }
    }

    pointer operator->() const {
      if (usingFixed) {
        return reinterpret_cast<pointer>(&parent->fixed.storage[fixedIndex]);
      } else {
        return flexibleIterator.operator->();
      }
    }
  };

  using iterator = Iterator;
  using const_iterator = ConstIterator;

  iterator begin() {
    auto ret = iterator(this);
    ret.setBegin();
    return ret;
  }
  iterator end() {
    auto ret = iterator(this);
    ret.setEnd();
    return ret;
  }
  const_iterator begin() const {
    auto ret = const_iterator(this);
    ret.setBegin();
    return ret;
  }
  const_iterator end() const {
    auto ret = const_iterator(this);
    ret.setEnd();
    return ret;
  }
  const_iterator cbegin() const { return begin(); }
  const_iterator cend() const { return end(); }

  iterator find(const Key& key) {
    if (usingFixed()) {
      if (auto* p = fixed.find(key)) {
        auto it = iterator(this);
        it.usingFixed = true;
        it.fixedIndex = p - &fixed.storage[0];
        return it;
      }
      return end();
    } else {
      auto it = iterator(this);
      it.usingFixed = false;
      it.flexibleIterator = flexible.find(key);
      return it;
    }
  }

  const_iterator find(const Key& key) const {
    if (usingFixed()) {
      if (const auto* p = fixed.find(key)) {
        auto it = const_iterator(this);
        it.usingFixed = true;
        it.fixedIndex = p - &fixed.storage[0];
        return it;
      }
      return end();
    } else {
      auto it = const_iterator(this);
      it.usingFixed = false;
      it.flexibleIterator = flexible.find(key);
      return it;
    }
  }

  bool TEST_ONLY_NEVER_USE_usingFixed() const { return usingFixed(); }
};

template<typename Key, typename Value, size_t N>
class SmallMap
  : public SmallMapBase<Key,
                        Value,
                        N,
                        OrderedFixedStorage<std::pair<Key, Value>, N>,
                        std::map<Key, Value>> {
public:
  using SmallMapBase<Key,
                     Value,
                     N,
                     OrderedFixedStorage<std::pair<Key, Value>, N>,
                     std::map<Key, Value>>::SmallMapBase;
};

template<typename Key, typename Value, size_t N>
class SmallUnorderedMap
  : public SmallMapBase<Key,
                        Value,
                        N,
                        UnorderedFixedStorage<std::pair<Key, Value>, N>,
                        std::unordered_map<Key, Value>> {
public:
  using SmallMapBase<Key,
                     Value,
                     N,
                     UnorderedFixedStorage<std::pair<Key, Value>, N>,
                     std::unordered_map<Key, Value>>::SmallMapBase;
};

} // namespace wasm

#endif // wasm_support_small_map_h
