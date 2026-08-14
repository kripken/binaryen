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
// A map of key-value pairs, which is often small. While the number of items is
// small, the implementation stores them in fixed inline storage. Once the size
// exceeds the fixed capacity, we switch to using a std::map or
// std::unordered_map.
//

#ifndef wasm_support_small_map_h
#define wasm_support_small_map_h

#include <cassert>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "support/small_set.h"
#include "support/utilities.h"

namespace wasm {

template<typename Key,
         typename Value,
         size_t N,
         typename FixedStorage,
         typename FlexibleMap>
class SmallMapBase {
  // fixed-space storage
  FixedStorage fixed;

  // flexible additional storage
  FlexibleMap flexible;

  bool usingFixed() const {
    // If the flexible storage contains something, then we are using it.
    // Otherwise we use the fixed storage. Note that if we grow and shrink then
    // we will stay in flexible mode until we reach a size of zero, at which
    // point we return to fixed mode. This is intentional, to avoid a lot of
    // movement in switching between fixed and flexible mode.
    return flexible.empty();
  }

public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<const Key, Value>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using map_type = FlexibleMap;

  // iteration

  template<typename Parent,
           typename FlexibleIterator,
           typename Ref,
           typename Ptr>
  struct IteratorBase {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = Ptr;
    using reference = Ref;

    Parent* parent = nullptr;
    bool usingFixed = true;
    size_t fixedIndex = 0;
    FlexibleIterator flexibleIterator{};

    IteratorBase() = default;

    IteratorBase(Parent* parent)
      : parent(parent), usingFixed(parent->usingFixed()) {}

    IteratorBase(Parent* parent, size_t index)
      : parent(parent), usingFixed(true), fixedIndex(index) {}

    IteratorBase(Parent* parent, FlexibleIterator fit)
      : parent(parent), usingFixed(false), flexibleIterator(fit) {}

    template<typename OtherParent,
             typename OtherFlexIt,
             typename OtherRef,
             typename OtherPtr>
    IteratorBase(
      const IteratorBase<OtherParent, OtherFlexIt, OtherRef, OtherPtr>& other)
      : parent(other.parent), usingFixed(other.usingFixed),
        fixedIndex(other.fixedIndex), flexibleIterator(other.flexibleIterator) {
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

    bool operator==(const IteratorBase& other) const {
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

    bool operator!=(const IteratorBase& other) const {
      return !(*this == other);
    }

    IteratorBase& operator++() {
      if (usingFixed) {
        fixedIndex++;
      } else {
        flexibleIterator++;
      }
      return *this;
    }

    IteratorBase operator++(int) {
      IteratorBase tmp = *this;
      ++(*this);
      return tmp;
    }

    Ref operator*() const {
      if (usingFixed) {
        return parent->fixed.get(fixedIndex);
      } else {
        return *flexibleIterator;
      }
    }

    Ptr operator->() const {
      if (usingFixed) {
        return &parent->fixed.get(fixedIndex);
      } else {
        return &*flexibleIterator;
      }
    }
  };

  using iterator = IteratorBase<SmallMapBase,
                                typename FlexibleMap::iterator,
                                std::pair<const Key, Value>&,
                                std::pair<const Key, Value>*>;
  using const_iterator = IteratorBase<const SmallMapBase,
                                      typename FlexibleMap::const_iterator,
                                      const std::pair<const Key, Value>&,
                                      const std::pair<const Key, Value>*>;
  using Iterator = iterator;
  using ConstIterator = const_iterator;

  SmallMapBase() = default;

  SmallMapBase(std::initializer_list<value_type> init) {
    insert(init.begin(), init.end());
  }

  template<typename InputIt> SmallMapBase(InputIt first, InputIt last) {
    insert(first, last);
  }

  SmallMapBase(const SmallMapBase& other)
    : fixed(other.fixed), flexible(other.flexible) {}

  SmallMapBase(SmallMapBase&& other) noexcept
    : fixed(std::move(other.fixed)), flexible(std::move(other.flexible)) {}

  SmallMapBase& operator=(const SmallMapBase& other) {
    if (this != &other) {
      fixed = other.fixed;
      flexible = other.flexible;
    }
    return *this;
  }

  SmallMapBase& operator=(SmallMapBase&& other) noexcept {
    if (this != &other) {
      fixed = std::move(other.fixed);
      flexible = std::move(other.flexible);
    }
    return *this;
  }

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

  size_t size() const {
    if (usingFixed()) {
      return fixed.used;
    } else {
      return flexible.size();
    }
  }

  bool empty() const { return size() == 0; }

  void clear() {
    fixed.clear();
    flexible.clear();
  }

  iterator find(const Key& k) {
    if (usingFixed()) {
      size_t i = fixed.findIndex(k);
      if (i != fixed.used) {
        return iterator(this, i);
      }
      return end();
    } else {
      auto flexIt = flexible.find(k);
      if (flexIt != flexible.end()) {
        return iterator(this, flexIt);
      }
      return end();
    }
  }

  const_iterator find(const Key& k) const {
    if (usingFixed()) {
      size_t i = fixed.findIndex(k);
      if (i != fixed.used) {
        return const_iterator(this, i);
      }
      return end();
    } else {
      auto flexIt = flexible.find(k);
      if (flexIt != flexible.end()) {
        return const_iterator(this, flexIt);
      }
      return end();
    }
  }

  bool contains(const Key& k) const {
    if (usingFixed()) {
      return fixed.findIndex(k) != fixed.used;
    } else {
      return flexible.contains(k);
    }
  }

  size_t count(const Key& k) const { return contains(k) ? 1 : 0; }

  Value& operator[](const Key& k) {
    if (usingFixed()) {
      size_t i = fixed.findIndex(k);
      if (i != fixed.used) {
        return fixed.get(i).second;
      }
      if (fixed.used < N) {
        size_t newIdx = fixed.findInsertionIndex(k);
        fixed.insert(value_type(k, Value{}));
        return fixed.get(newIdx).second;
      }
      switchToFlexible();
      return flexible[k];
    } else {
      return flexible[k];
    }
  }

  Value& operator[](Key&& k) {
    if (usingFixed()) {
      size_t i = fixed.findIndex(k);
      if (i != fixed.used) {
        return fixed.get(i).second;
      }
      if (fixed.used < N) {
        size_t newIdx = fixed.findInsertionIndex(k);
        fixed.insert(value_type(std::move(k), Value{}));
        return fixed.get(newIdx).second;
      }
      switchToFlexible();
      return flexible[std::move(k)];
    } else {
      return flexible[std::move(k)];
    }
  }

  Value& at(const Key& k) {
    auto it = find(k);
    if (it == end()) {
      throw std::out_of_range("SmallMap::at: key not found");
    }
    return it->second;
  }

  const Value& at(const Key& k) const {
    auto it = find(k);
    if (it == end()) {
      throw std::out_of_range("SmallMap::at: key not found");
    }
    return it->second;
  }

  std::pair<iterator, bool> insert(const value_type& val) {
    if (usingFixed()) {
      size_t i = fixed.findIndex(val.first);
      if (i != fixed.used) {
        return {iterator(this, i), false};
      }
      if (fixed.used < N) {
        size_t newIdx = fixed.findInsertionIndex(val.first);
        fixed.insert(val);
        return {iterator(this, newIdx), true};
      }
      switchToFlexible();
      auto [flexIt, inserted] = flexible.insert(val);
      return {iterator(this, flexIt), inserted};
    } else {
      auto [flexIt, inserted] = flexible.insert(val);
      return {iterator(this, flexIt), inserted};
    }
  }

  std::pair<iterator, bool> insert(value_type&& val) {
    if (usingFixed()) {
      size_t i = fixed.findIndex(val.first);
      if (i != fixed.used) {
        return {iterator(this, i), false};
      }
      if (fixed.used < N) {
        size_t newIdx = fixed.findInsertionIndex(val.first);
        fixed.insert(std::move(val));
        return {iterator(this, newIdx), true};
      }
      switchToFlexible();
      auto [flexIt, inserted] = flexible.insert(std::move(val));
      return {iterator(this, flexIt), inserted};
    } else {
      auto [flexIt, inserted] = flexible.insert(std::move(val));
      return {iterator(this, flexIt), inserted};
    }
  }

  template<typename P>
    requires std::is_constructible_v<value_type, P&&>
  std::pair<iterator, bool> insert(P&& val) {
    return insert(value_type(std::forward<P>(val)));
  }

  template<typename InputIt> void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  void insert(std::initializer_list<value_type> init) {
    insert(init.begin(), init.end());
  }

  template<typename... Args> std::pair<iterator, bool> emplace(Args&&... args) {
    return insert(value_type(std::forward<Args>(args)...));
  }

  template<typename... Args>
  std::pair<iterator, bool> try_emplace(const Key& k, Args&&... args) {
    if (usingFixed()) {
      size_t i = fixed.findIndex(k);
      if (i != fixed.used) {
        return {iterator(this, i), false};
      }
      if (fixed.used < N) {
        size_t newIdx = fixed.findInsertionIndex(k);
        fixed.insert(
          value_type(std::piecewise_construct,
                     std::forward_as_tuple(k),
                     std::forward_as_tuple(std::forward<Args>(args)...)));
        return {iterator(this, newIdx), true};
      }
      switchToFlexible();
      auto [flexIt, inserted] =
        flexible.try_emplace(k, std::forward<Args>(args)...);
      return {iterator(this, flexIt), inserted};
    } else {
      auto [flexIt, inserted] =
        flexible.try_emplace(k, std::forward<Args>(args)...);
      return {iterator(this, flexIt), inserted};
    }
  }

  template<typename M>
  std::pair<iterator, bool> insert_or_assign(const Key& k, M&& obj) {
    if (usingFixed()) {
      size_t i = fixed.findIndex(k);
      if (i != fixed.used) {
        fixed.get(i).second = std::forward<M>(obj);
        return {iterator(this, i), false};
      }
      if (fixed.used < N) {
        size_t newIdx = fixed.findInsertionIndex(k);
        fixed.insert(value_type(k, std::forward<M>(obj)));
        return {iterator(this, newIdx), true};
      }
      switchToFlexible();
      auto [flexIt, inserted] =
        flexible.insert_or_assign(k, std::forward<M>(obj));
      return {iterator(this, flexIt), inserted};
    } else {
      auto [flexIt, inserted] =
        flexible.insert_or_assign(k, std::forward<M>(obj));
      return {iterator(this, flexIt), inserted};
    }
  }

  size_t erase(const Key& k) {
    if (usingFixed()) {
      size_t i = fixed.findIndex(k);
      if (i != fixed.used) {
        fixed.eraseAt(i);
        return 1;
      }
      return 0;
    } else {
      return flexible.erase(k);
    }
  }

  iterator erase(const_iterator pos) {
    assert(pos != end());
    if (usingFixed()) {
      size_t idx = pos.fixedIndex;
      fixed.eraseAt(idx);
      return iterator(this, idx);
    } else {
      auto nextFlexIt = flexible.erase(pos.flexibleIterator);
      return iterator(this, nextFlexIt);
    }
  }

  iterator erase(iterator pos) { return erase(const_iterator(pos)); }

  void swap(SmallMapBase& other) {
    if (this == &other) {
      return;
    }
    SmallMapBase tmp(std::move(*this));
    *this = std::move(other);
    other = std::move(tmp);
  }

  bool operator==(const SmallMapBase& other) const {
    if (size() != other.size()) {
      return false;
    }
    for (const auto& [k, v] : *this) {
      auto otherIt = other.find(k);
      if (otherIt == other.end() || otherIt->second != v) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const SmallMapBase& other) const { return !(*this == other); }

  // Test-only method to allow unit tests to verify the right internal
  // behavior.
  bool TEST_ONLY_NEVER_USE_usingFixed() const { return usingFixed(); }

private:
  void switchToFlexible() {
    assert(fixed.used == N);
    assert(flexible.empty());
    for (size_t j = 0; j < fixed.used; j++) {
      flexible.insert(std::move(fixed.get(j)));
    }
    fixed.clear();
  }
};

template<typename Key, typename Value, size_t N>
class SmallMap
  : public SmallMapBase<Key,
                        Value,
                        N,
                        OrderedFixedStorage<std::pair<const Key, Value>,
                                            N,
                                            Key,
                                            PairKey<Key, Value>>,
                        std::map<Key, Value>> {
public:
  using Base = SmallMapBase<Key,
                            Value,
                            N,
                            OrderedFixedStorage<std::pair<const Key, Value>,
                                                N,
                                                Key,
                                                PairKey<Key, Value>>,
                            std::map<Key, Value>>;
  using Base::Base;
};

template<typename Key, typename Value, size_t N>
class SmallUnorderedMap
  : public SmallMapBase<Key,
                        Value,
                        N,
                        UnorderedFixedStorage<std::pair<const Key, Value>,
                                              N,
                                              Key,
                                              PairKey<Key, Value>>,
                        std::unordered_map<Key, Value>> {
public:
  using Base = SmallMapBase<Key,
                            Value,
                            N,
                            UnorderedFixedStorage<std::pair<const Key, Value>,
                                                  N,
                                                  Key,
                                                  PairKey<Key, Value>>,
                            std::unordered_map<Key, Value>>;
  using Base::Base;
};

template<typename Key, typename Value, size_t N>
inline void swap(SmallMap<Key, Value, N>& a, SmallMap<Key, Value, N>& b) {
  a.swap(b);
}

template<typename Key, typename Value, size_t N>
inline void swap(SmallUnorderedMap<Key, Value, N>& a,
                 SmallUnorderedMap<Key, Value, N>& b) {
  a.swap(b);
}

} // namespace wasm

#endif // wasm_support_small_map_h
