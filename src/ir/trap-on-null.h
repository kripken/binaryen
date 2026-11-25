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
// Shared compile-time metadata for instructions that trap when one or more of
// their reference operands is null.
//
// Note: EffectAnalyzer::trapOnNull in ir/effects.h also tracks trap-on-null
// behavior alongside other side effects; keep these in sync when adding new
// instructions.
//

#ifndef wasm_ir_trap_on_null_h
#define wasm_ir_trap_on_null_h

#include <type_traits>

#include "wasm-traversal.h"
#include "wasm.h"

namespace wasm {

namespace detail {

template<typename F> inline bool invokeTrapOnNull(F&& f, Expression*& ref) {
  if constexpr (std::is_void_v<std::invoke_result_t<F, Expression*&>>) {
    f(ref);
    return false;
  } else {
    return f(ref);
  }
}

} // namespace detail

// Primary template: expressions do not trap on null by default.
template<typename T> struct TrapOnNull {
  static constexpr bool value = false;

  template<typename F> static bool forEach(T*, F&&) { return false; }
};

// Helpers for the common shapes of trap-on-null operands.
template<typename T, Expression* T::*Field> struct SingleTrapOnNull {
  static constexpr bool value = true;

  template<typename F> static bool forEach(T* curr, F&& f) {
    return detail::invokeTrapOnNull(f, curr->*Field);
  }
};

template<typename T, Expression* T::*Field> struct OptionalTrapOnNull {
  static constexpr bool value = true;

  template<typename F> static bool forEach(T* curr, F&& f) {
    if (curr->*Field) {
      return detail::invokeTrapOnNull(f, curr->*Field);
    }
    return false;
  }
};

template<typename T, Expression* T::*Field1, Expression* T::*Field2>
struct PairTrapOnNull {
  static constexpr bool value = true;

  template<typename F> static bool forEach(T* curr, F&& f) {
    return detail::invokeTrapOnNull(f, curr->*Field1) ||
           detail::invokeTrapOnNull(f, curr->*Field2);
  }
};

// Single list of all expression types that can trap on a null input.
// Categories:
//   SINGLE(Type, field)        - always traps if `curr->field` is null
//   OPTIONAL(Type, field)      - traps if `curr->field` is present and null
//   PAIR(Type, field1, field2) - always traps if either field is null
//   CUSTOM(Type)               - conditional on op/type (specialized below)
#define TRAP_ON_NULL_EXPRESSIONS(SINGLE, OPTIONAL, PAIR, CUSTOM)               \
  SINGLE(CallRef, target)                                                      \
  SINGLE(ThrowRef, exnref)                                                     \
  SINGLE(I31Get, i31)                                                          \
  CUSTOM(RefCast)                                                              \
  SINGLE(RefGetDesc, ref)                                                      \
  OPTIONAL(BrOn, desc)                                                         \
  OPTIONAL(StructNew, desc)                                                    \
  SINGLE(StructGet, ref)                                                       \
  SINGLE(StructSet, ref)                                                       \
  SINGLE(StructRMW, ref)                                                       \
  SINGLE(StructCmpxchg, ref)                                                   \
  PAIR(StructWait, ref, waitqueue)                                             \
  SINGLE(WaitqueueNotify, waitqueue)                                           \
  SINGLE(ArrayGet, ref)                                                        \
  SINGLE(ArraySet, ref)                                                        \
  SINGLE(ArrayLoad, ref)                                                       \
  SINGLE(ArrayStore, ref)                                                      \
  SINGLE(ArrayLen, ref)                                                        \
  PAIR(ArrayCopy, destRef, srcRef)                                             \
  SINGLE(ArrayFill, ref)                                                       \
  SINGLE(ArrayInitData, ref)                                                   \
  SINGLE(ArrayInitElem, ref)                                                   \
  SINGLE(ArrayRMW, ref)                                                        \
  SINGLE(ArrayCmpxchg, ref)                                                    \
  CUSTOM(RefAs)                                                                \
  CUSTOM(StringNew)                                                            \
  SINGLE(StringMeasure, ref)                                                   \
  PAIR(StringEncode, str, array)                                               \
  PAIR(StringConcat, left, right)                                              \
  CUSTOM(StringEq)                                                             \
  SINGLE(StringWTF16Get, ref)                                                  \
  SINGLE(StringSliceWTF, ref)                                                  \
  SINGLE(ContNew, func)                                                        \
  SINGLE(ContBind, cont)                                                       \
  SINGLE(Resume, cont)                                                         \
  SINGLE(ResumeThrow, cont)                                                    \
  SINGLE(StackSwitch, cont)

#define TRAP_ON_NULL_DEF_SINGLE(Type, field)                                   \
  template<> struct TrapOnNull<Type> : SingleTrapOnNull<Type, &Type::field> {};
#define TRAP_ON_NULL_DEF_OPTIONAL(Type, field)                                 \
  template<>                                                                   \
  struct TrapOnNull<Type> : OptionalTrapOnNull<Type, &Type::field> {};
#define TRAP_ON_NULL_DEF_PAIR(Type, field1, field2)                            \
  template<>                                                                   \
  struct TrapOnNull<Type>                                                      \
    : PairTrapOnNull<Type, &Type::field1, &Type::field2> {};
#define TRAP_ON_NULL_DEF_CUSTOM(Type)

TRAP_ON_NULL_EXPRESSIONS(TRAP_ON_NULL_DEF_SINGLE,
                         TRAP_ON_NULL_DEF_OPTIONAL,
                         TRAP_ON_NULL_DEF_PAIR,
                         TRAP_ON_NULL_DEF_CUSTOM)

#undef TRAP_ON_NULL_DEF_SINGLE
#undef TRAP_ON_NULL_DEF_OPTIONAL
#undef TRAP_ON_NULL_DEF_PAIR
#undef TRAP_ON_NULL_DEF_CUSTOM

template<> struct TrapOnNull<RefCast> {
  static constexpr bool value = true;

  template<typename F> static bool forEach(RefCast* curr, F&& f) {
    if (curr->type.isNonNullableRef() &&
        detail::invokeTrapOnNull(f, curr->ref)) {
      return true;
    }
    if (curr->desc && detail::invokeTrapOnNull(f, curr->desc)) {
      return true;
    }
    return false;
  }
};

template<> struct TrapOnNull<RefAs> {
  static constexpr bool value = true;

  template<typename F> static bool forEach(RefAs* curr, F&& f) {
    if (curr->op == RefAsNonNull) {
      return detail::invokeTrapOnNull(f, curr->value);
    }
    return false;
  }
};

template<> struct TrapOnNull<StringNew> {
  static constexpr bool value = true;

  template<typename F> static bool forEach(StringNew* curr, F&& f) {
    switch (curr->op) {
      case StringNewLossyUTF8Array:
      case StringNewWTF16Array:
        return detail::invokeTrapOnNull(f, curr->ref);
      case StringNewFromCodePoint:
        return false;
    }
    WASM_UNREACHABLE("unexpected op");
  }
};

template<> struct TrapOnNull<StringEq> {
  static constexpr bool value = true;

  template<typename F> static bool forEach(StringEq* curr, F&& f) {
    switch (curr->op) {
      case StringEqEqual:
        return false;
      case StringEqCompare:
        return detail::invokeTrapOnNull(f, curr->left) ||
               detail::invokeTrapOnNull(f, curr->right);
    }
    WASM_UNREACHABLE("unexpected op");
  }
};

// Runtime query: returns whether `curr`'s expression class can trap on null.
inline bool canTrapOnNull(const Expression* curr) {
  switch (curr->_id) {
#define CASE_ONE(Type, ...) case Expression::Id::Type##Id:
    TRAP_ON_NULL_EXPRESSIONS(CASE_ONE, CASE_ONE, CASE_ONE, CASE_ONE)
#undef CASE_ONE
      return true;
    default:
      return false;
  }
}

// Wraps a Walker (such as PostWalker or CFGWalker) so that for every expression
// type T with TrapOnNull<T>::value == true, `self->visitTrapOnNull(curr)` is
// invoked right before `self->visitT(curr)`.
//
// If `visitTrapOnNull` returns bool and returns true, `visitT` is skipped
// (useful when the expression was replaced, e.g. in OptimizeInstructions). If
// it returns void, `visitT` is always called afterward.
//
// Expression types that cannot trap on null do NOT override `doVisit*`, so
// `PostWalker::scan` skips pushing tasks for them at compile time.
template<typename SubType, typename BaseWalker>
struct TrapOnNullWalker : public BaseWalker {
  template<typename T, typename VisitFn>
  static void runWithTrapOnNull(SubType* self, T* curr, VisitFn&& visitFn) {
    if constexpr (std::is_void_v<decltype(self->visitTrapOnNull(curr))>) {
      self->visitTrapOnNull(curr);
      visitFn();
    } else {
      if (!self->visitTrapOnNull(curr)) {
        visitFn();
      }
    }
  }

#define DO_VISIT_ONE(Type, ...)                                                \
  static void doVisit##Type(SubType* self, Expression** currp) {               \
    auto* curr = (*currp)->cast<Type>();                                       \
    runWithTrapOnNull(self, curr, [&]() { self->visit##Type(curr); });         \
  }
  TRAP_ON_NULL_EXPRESSIONS(DO_VISIT_ONE,
                           DO_VISIT_ONE,
                           DO_VISIT_ONE,
                           DO_VISIT_ONE)
#undef DO_VISIT_ONE

  void visit(Expression* curr) {
    assert(curr);

    switch (curr->_id) {
#define DELEGATE(CLASS_TO_VISIT)                                               \
  case Expression::Id::CLASS_TO_VISIT##Id: {                                   \
    auto* self = static_cast<SubType*>(this);                                  \
    auto* cast = static_cast<CLASS_TO_VISIT*>(curr);                           \
    if constexpr (TrapOnNull<CLASS_TO_VISIT>::value) {                         \
      runWithTrapOnNull(                                                       \
        self, cast, [&]() { self->visit##CLASS_TO_VISIT(cast); });             \
    } else {                                                                   \
      self->visit##CLASS_TO_VISIT(cast);                                       \
    }                                                                          \
    return;                                                                    \
  }
#include "wasm-delegations.def"
      default:
        WASM_UNREACHABLE("unexpected expression type");
    }
  }
};

#undef TRAP_ON_NULL_EXPRESSIONS

} // namespace wasm

#endif // wasm_ir_trap_on_null_h
