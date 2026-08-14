/*
 * Copyright 2021 WebAssembly Community Group participants
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
// A set of elements, which is often small. While the number of items is small,
// the implementation simply stores them in an array that is linearly looked
// through. Once the size is large enough, we switch to using a std::set or
// std::unordered_set.
//

#ifndef wasm_support_small_set_h
#define wasm_support_small_set_h

#include <algorithm>
#include <array>
#include <cassert>
#include <set>
#include <unordered_set>

#include "utilities.h"

namespace wasm {

template<typename T> struct Identity {
  constexpr const T& operator()(const T& x) const { return x; }
};

template<typename Key, typename Value> struct PairKey {
  constexpr const Key& operator()(const std::pair<Key, Value>& x) const {
    return x.first;
  }
  constexpr const Key& operator()(const std::pair<const Key, Value>& x) const {
    return x.first;
  }
};

template<typename T, size_t N> struct FixedStorageBase {
  size_t used = 0;

  enum InsertResult {
    // Either we inserted a new item, or the item already existed, so no error
    // occurred.
    NoError,
    // We needed to insert (the item did not exist), but we were already at full
    // size, so we could not insert, which is an error condition that the caller
    // must handle.
    CouldNotInsert
  };

private:
  union Entry {
    T val;
    Entry() {}
    ~Entry() {}
  };
  std::array<Entry, N == 0 ? 1 : N> storage_;

public:
  FixedStorageBase() = default;

  FixedStorageBase(const FixedStorageBase& other) : used(other.used) {
    for (size_t i = 0; i < used; i++) {
      constructAt(i, other.get(i));
    }
  }

  FixedStorageBase(FixedStorageBase&& other) noexcept : used(other.used) {
    for (size_t i = 0; i < used; i++) {
      moveConstructAt(i, std::move(other.get(i)));
    }
    other.clear();
  }

  FixedStorageBase& operator=(const FixedStorageBase& other) {
    if (this != &other) {
      clear();
      used = other.used;
      for (size_t i = 0; i < used; i++) {
        constructAt(i, other.get(i));
      }
    }
    return *this;
  }

  FixedStorageBase& operator=(FixedStorageBase&& other) noexcept {
    if (this != &other) {
      clear();
      used = other.used;
      for (size_t i = 0; i < used; i++) {
        moveConstructAt(i, std::move(other.get(i)));
      }
      other.clear();
    }
    return *this;
  }

  ~FixedStorageBase() { clear(); }

  void clear() {
    for (size_t i = 0; i < used; i++) {
      destroyAt(i);
    }
    used = 0;
  }

  template<typename... Args> void constructAt(size_t i, Args&&... args) {
    assert(i < (N == 0 ? 1 : N));
    new (&storage_[i].val) T(std::forward<Args>(args)...);
  }

  void destroyAt(size_t i) {
    assert(i < (N == 0 ? 1 : N));
    storage_[i].val.~T();
  }

  void moveConstructFromIndex(size_t dst, size_t src) {
    assert(dst < (N == 0 ? 1 : N));
    assert(src < (N == 0 ? 1 : N));
    new (&storage_[dst].val) T(std::move(storage_[src].val));
    destroyAt(src);
  }

  template<typename U> void moveConstructAt(size_t dst, U&& src) {
    assert(dst < (N == 0 ? 1 : N));
    new (&storage_[dst].val) T(std::forward<U>(src));
  }

  T& get(size_t i) {
    assert(i < used);
    return storage_[i].val;
  }

  const T& get(size_t i) const {
    assert(i < used);
    return storage_[i].val;
  }

  T& operator[](size_t i) { return get(i); }
  const T& operator[](size_t i) const { return get(i); }
};

template<typename T, size_t N, typename Key = T, typename GetKey = Identity<T>>
struct UnorderedFixedStorage : public FixedStorageBase<T, N> {
  using InsertResult = typename FixedStorageBase<T, N>::InsertResult;

  size_t findIndex(const Key& k) const {
    GetKey getKey;
    for (size_t i = 0; i < this->used; i++) {
      if (getKey(this->get(i)) == k) {
        return i;
      }
    }
    return this->used;
  }

  size_t findInsertionIndex(const Key& /*k*/) const { return this->used; }

  InsertResult insert(const T& x) {
    GetKey getKey;
    if (findIndex(getKey(x)) != this->used) {
      return InsertResult::NoError;
    }
    assert(this->used <= N);
    if (this->used == N) {
      return InsertResult::CouldNotInsert;
    }
    this->constructAt(this->used++, x);
    return InsertResult::NoError;
  }

  InsertResult insert(T&& x) {
    GetKey getKey;
    if (findIndex(getKey(x)) != this->used) {
      return InsertResult::NoError;
    }
    assert(this->used <= N);
    if (this->used == N) {
      return InsertResult::CouldNotInsert;
    }
    this->constructAt(this->used++, std::move(x));
    return InsertResult::NoError;
  }

  void erase(const Key& k) {
    size_t i = findIndex(k);
    if (i != this->used) {
      eraseAt(i);
    }
  }

  void eraseAt(size_t i) {
    assert(i < this->used);
    this->destroyAt(i);
    if (i != this->used - 1) {
      this->moveConstructFromIndex(i, this->used - 1);
    }
    this->used--;
  }
};

template<typename T, size_t N, typename Key = T, typename GetKey = Identity<T>>
struct OrderedFixedStorage : public FixedStorageBase<T, N> {
  using InsertResult = typename FixedStorageBase<T, N>::InsertResult;

  size_t findInsertionIndex(const Key& k) const {
    GetKey getKey;
    size_t i = 0;
    while (i < this->used && getKey(this->get(i)) < k) {
      i++;
    }
    return i;
  }

  size_t findIndex(const Key& k) const {
    size_t i = findInsertionIndex(k);
    if (i < this->used) {
      GetKey getKey;
      if (getKey(this->get(i)) == k) {
        return i;
      }
    }
    return this->used;
  }

  InsertResult insert(const T& x) {
    GetKey getKey;
    const Key& k = getKey(x);
    size_t i = findInsertionIndex(k);
    if (i < this->used && getKey(this->get(i)) == k) {
      return InsertResult::NoError;
    }
    assert(this->used <= N);
    if (this->used == N) {
      return InsertResult::CouldNotInsert;
    }
    if (i != this->used) {
      this->moveConstructFromIndex(this->used, this->used - 1);
      for (size_t j = this->used - 1; j > i; j--) {
        this->moveConstructFromIndex(j, j - 1);
      }
    }
    this->constructAt(i, x);
    this->used++;
    return InsertResult::NoError;
  }

  InsertResult insert(T&& x) {
    GetKey getKey;
    const Key& k = getKey(x);
    size_t i = findInsertionIndex(k);
    if (i < this->used && getKey(this->get(i)) == k) {
      return InsertResult::NoError;
    }
    assert(this->used <= N);
    if (this->used == N) {
      return InsertResult::CouldNotInsert;
    }
    if (i != this->used) {
      this->moveConstructFromIndex(this->used, this->used - 1);
      for (size_t j = this->used - 1; j > i; j--) {
        this->moveConstructFromIndex(j, j - 1);
      }
    }
    this->constructAt(i, std::move(x));
    this->used++;
    return InsertResult::NoError;
  }

  void erase(const Key& k) {
    size_t i = findIndex(k);
    if (i != this->used) {
      eraseAt(i);
    }
  }

  void eraseAt(size_t i) {
    assert(i < this->used);
    this->destroyAt(i);
    for (size_t j = i + 1; j < this->used; j++) {
      this->moveConstructFromIndex(j - 1, j);
    }
    this->used--;
  }
};

template<typename T, size_t N, typename FixedStorage, typename FlexibleSet>
class SmallSetBase {
  // fixed-space storage
  FixedStorage fixed;

  // flexible additional storage
  FlexibleSet flexible;

  bool usingFixed() const {
    // If the flexible storage contains something, then we are using it.
    // Otherwise we use the fixed storage. Note that if we grow and shrink then
    // we will stay in flexible mode until we reach a size of zero, at which
    // point we return to fixed mode. This is intentional, to avoid a lot of
    // movement in switching between fixed and flexible mode.
    return flexible.empty();
  }

public:
  using value_type = T;
  using key_type = T;
  using reference = T&;
  using const_reference = const T&;
  using set_type = FlexibleSet;
  using size_type = size_t;

  SmallSetBase() {}
  SmallSetBase(std::initializer_list<T> init) {
    for (T item : init) {
      insert(item);
    }
  }

  void insert(const T& x) {
    if (usingFixed()) {
      if (fixed.insert(x) == FixedStorage::InsertResult::CouldNotInsert) {
        // We need to add an item but no fixed storage remains to grow. Switch
        // to flexible.
        assert(fixed.used == N);
        assert(flexible.empty());
        for (size_t i = 0; i < fixed.used; i++) {
          flexible.insert(std::move(fixed.get(i)));
        }
        flexible.insert(x);
        assert(!usingFixed());
        fixed.clear();
      }
    } else {
      flexible.insert(x);
    }
  }

  void erase(const T& x) {
    if (usingFixed()) {
      fixed.erase(x);
    } else {
      flexible.erase(x);
    }
  }

  bool contains(const T& x) const {
    if (usingFixed()) {
      return fixed.findIndex(x) != fixed.used;
    } else {
      return flexible.contains(x);
    }
  }

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

  bool
  operator==(const SmallSetBase<T, N, FixedStorage, FlexibleSet>& other) const {
    if (size() != other.size()) {
      return false;
    }
    if (usingFixed()) {
      for (size_t i = 0; i < fixed.used; i++) {
        if (!other.contains(fixed.get(i))) {
          return false;
        }
      }
      return true;
    } else if (other.usingFixed()) {
      for (size_t i = 0; i < other.fixed.used; i++) {
        if (!contains(other.fixed.get(i))) {
          return false;
        }
      }
      return true;
    } else {
      return flexible == other.flexible;
    }
  }

  bool
  operator!=(const SmallSetBase<T, N, FixedStorage, FlexibleSet>& other) const {
    return !(*this == other);
  }

  // iteration

  template<typename Parent, typename FlexibleIterator> struct IteratorBase {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = long;
    using value_type = T;
    using pointer = const value_type*;
    using reference = const value_type&;

    const Parent* parent;

    using Iterator = IteratorBase<Parent, FlexibleIterator>;

    // Whether we are using fixed storage in the parent. When doing so we have
    // the index in fixedIndex. Otherwise, we are using flexible storage, and we
    // will use flexibleIterator.
    bool usingFixed;
    size_t fixedIndex;
    FlexibleIterator flexibleIterator;

    IteratorBase(const Parent* parent)
      : parent(parent), usingFixed(parent->usingFixed()) {}

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
      // std::set allows changes while iterating. For us here, though, it would
      // be nontrivial to support that given we have two iterators that we
      // generalize over (switching "in the middle" would not be easy or fast),
      // so error on that.
      if (usingFixed != other.usingFixed) {
        Fatal() << "SmallSet does not support changes while iterating";
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

    const value_type& operator*() const {
      if (this->usingFixed) {
        return this->parent->fixed.get(this->fixedIndex);
      } else {
        return *this->flexibleIterator;
      }
    }

    const value_type* operator->() const { return &**this; }
  };

  using Iterator = IteratorBase<SmallSetBase<T, N, FixedStorage, FlexibleSet>,
                                typename FlexibleSet::const_iterator>;

  Iterator begin() {
    auto ret = Iterator(this);
    ret.setBegin();
    return ret;
  }
  Iterator end() {
    auto ret = Iterator(this);
    ret.setEnd();
    return ret;
  }
  Iterator begin() const {
    auto ret = Iterator(this);
    ret.setBegin();
    return ret;
  }
  Iterator end() const {
    auto ret = Iterator(this);
    ret.setEnd();
    return ret;
  }

  using iterator = Iterator;
  using const_iterator = Iterator;

  // Test-only method to allow unit tests to verify the right internal
  // behavior.
  bool TEST_ONLY_NEVER_USE_usingFixed() { return usingFixed(); }
};

template<typename T, size_t N>
class SmallSet
  : public SmallSetBase<T, N, OrderedFixedStorage<T, N>, std::set<T>> {};

template<typename T, size_t N>
class SmallUnorderedSet : public SmallSetBase<T,
                                              N,
                                              UnorderedFixedStorage<T, N>,
                                              std::unordered_set<T>> {};

} // namespace wasm

#endif // wasm_support_small_set_h
