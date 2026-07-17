#include "ir/constraint.h"
#include "ir/abstract.h"
#include "gtest/gtest.h"

using namespace wasm;
using namespace wasm::Abstract;
using namespace wasm::constraint;

TEST(ConstraintTest, TestEq) {
  // x == 5 (we use "x" for the name of the thing being compared, in these
  // comments).
  Constraint c{Eq, {Literal(int32_t(5))}};

  // Sets start as proving anything, as representing unreachable code.
  AndedConstraintSet s;
  EXPECT_TRUE(s.provesEverything());
  EXPECT_EQ(s.proves(c), True);

  // We can't infer anything if told so.
  s.setProvesNothing();
  EXPECT_EQ(s.proves(c), Unknown);

  // If we add it, then things check out: a thing always proves itself true.
  s.approximateAnd(c);
  EXPECT_EQ(s.size(), 1);
  EXPECT_EQ(s.proves(c), True);

  // Ditto using set();
  s.set(c);
  EXPECT_EQ(s.proves(c), True);

  // x == 10, a different number: we can infer false.
  EXPECT_EQ(s.proves(Constraint{Eq, {Literal(int32_t(10))}}), False);

  // x != 15: we can infer true.
  EXPECT_EQ(s.proves(Constraint{Ne, {Literal(int32_t(15))}}), True);

  // x != 5: we can infer false.
  EXPECT_EQ(s.proves(Constraint{Ne, {Literal(int32_t(5))}}), False);
}

TEST(ConstraintTest, TestNe) {
  AndedConstraintSet s;
  // x != 5
  Constraint c{Ne, {Literal(int32_t(5))}};
  s.set(c);

  // Checks out versus itself.
  EXPECT_EQ(s.proves(c), True);

  // x == 10: we don't know.
  EXPECT_EQ(s.proves(Constraint{Eq, {Literal(int32_t(10))}}), Unknown);

  // x != 15: we don't know.
  EXPECT_EQ(s.proves(Constraint{Ne, {Literal(int32_t(15))}}), Unknown);

  // x == 5: we can infer false.
  EXPECT_EQ(s.proves(Constraint{Eq, {Literal(int32_t(5))}}), False);
}

TEST(ConstraintTest, TestMulti) {
  AndedConstraintSet s;
  // x != 5 && x != 10
  Constraint c{Ne, {Literal(int32_t(5))}};
  Constraint d{Ne, {Literal(int32_t(10))}};
  s.set(c);
  s.approximateAnd(d);

  // Each checks out versus itself.
  EXPECT_EQ(s.proves(c), True);
  EXPECT_EQ(s.proves(d), True);

  // x == 5: false.
  EXPECT_EQ(s.proves(Constraint{Eq, {Literal(int32_t(5))}}), False);

  // x == 10: false.
  EXPECT_EQ(s.proves(Constraint{Eq, {Literal(int32_t(10))}}), False);

  // x == 15: we don't know.
  EXPECT_EQ(s.proves(Constraint{Eq, {Literal(int32_t(15))}}), Unknown);

  // x != 15: we don't know.
  EXPECT_EQ(s.proves(Constraint{Ne, {Literal(int32_t(15))}}), Unknown);
}

TEST(ConstraintTest, TestSets) {
  // x == 5
  Constraint c{Eq, {Literal(int32_t(5))}};

  AndedConstraintSet s;

  // Any set always proves itself to be true.
  EXPECT_EQ(s.proves(s), True);

  // Ditto after adding something.
  s.set(c);
  EXPECT_EQ(s.proves(s), True);

  // Another set, empty.
  AndedConstraintSet t;

  // Make both sets contain the same stuff.
  t.set(c);
  EXPECT_EQ(s.proves(t), True);

  // Now t has *different* stuff, x == 10, which given s is false.
  t.set(Constraint{Eq, {Literal(int32_t(10))}});
  EXPECT_EQ(s.proves(t), False);

  // Same, with x != 10. Now we know it is true.
  t.set(Constraint{Ne, {Literal(int32_t(10))}});
  EXPECT_EQ(s.proves(t), True);

  // In reverse, we can infer nothing: knowing x != 10 does not say if x == 5.
  EXPECT_EQ(t.proves(s), Unknown);
}

TEST(ConstraintTest, TestSetsUnknown) {
  // x != 5
  // x != 10
  AndedConstraintSet s;
  s.set(Constraint{Ne, {Literal(int32_t(5))}});
  s.approximateAnd(Constraint{Ne, {Literal(int32_t(10))}});

  // x != 20, which is unknown by s.
  AndedConstraintSet t;
  t.set(Constraint{Ne, {Literal(int32_t(20))}});
  EXPECT_EQ(s.proves(t), Unknown);

  // Add x == 10, which is false by s, and so the whole thing is false.
  t.set(Constraint{Eq, {Literal(int32_t(10))}});
  EXPECT_EQ(s.proves(t), False);
}

TEST(ConstraintTest, TestOrTrivial) {
  // { x == 5 }
  AndedConstraintSet s;
  s.set(Constraint{Eq, {Literal(int32_t(5))}});

  // { }
  AndedConstraintSet empty;
  empty.setProvesNothing();

  // Anything ORed with the empty set becomes the empty set: if one side can
  // prove nothing, neither can the result.
  auto t = s;
  t.approximateOr(empty);
  EXPECT_EQ(t, empty);

  // Flipped.
  t = empty;
  t.approximateOr(s);
  EXPECT_EQ(t, empty);

  // ORing with oneself changes nothing
  t = s;
  t.approximateOr(s);
  EXPECT_EQ(t, s);
}

TEST(ConstraintTest, TestOrImplies) {
  // { x == 5 }
  AndedConstraintSet s;
  s.set(Constraint{Eq, {Literal(int32_t(5))}});

  // { x != 10 }
  AndedConstraintSet t;
  t.set(Constraint{Ne, {Literal(int32_t(10))}});

  // ORing these leaves us with x != 10.
  auto u = s;
  u.approximateOr(t);
  EXPECT_EQ(u, t);

  // Flipped.
  u = t;
  u.approximateOr(s);
  EXPECT_EQ(u, t);
}

TEST(ConstraintTest, TestMaxCapacity) {
  EXPECT_EQ(MaxConstraints, 3);

  // Max out with x != 10, 20, 30
  Constraint not10{Ne, {Literal(int32_t(10))}};
  Constraint not20{Ne, {Literal(int32_t(20))}};
  Constraint not30{Ne, {Literal(int32_t(30))}};

  AndedConstraintSet s;
  s.set(not10);
  s.approximateAnd(not20);
  s.approximateAnd(not30);

  // We can prove all those.
  EXPECT_EQ(s.proves(not10), True);
  EXPECT_EQ(s.proves(not20), True);
  EXPECT_EQ(s.proves(not30), True);

  // Add another, exceeding the capacity.
  Constraint not40{Ne, {Literal(int32_t(40))}};
  s.approximateAnd(not40);

  // We can prove the old ones but not the new.
  EXPECT_EQ(s.proves(not10), True);
  EXPECT_EQ(s.proves(not20), True);
  EXPECT_EQ(s.proves(not30), True);
  EXPECT_EQ(s.proves(not40), Unknown);
}

TEST(ConstraintTest, TestDeduplication) {
  Constraint eq10{Eq, {Literal(int32_t(10))}};

  AndedConstraintSet s;
  EXPECT_EQ(s.size(), 0);
  s.set(eq10);
  EXPECT_EQ(s.size(), 1);
  // The size does not increase when we add eq10 again.
  s.approximateAnd(eq10);
  EXPECT_EQ(s.size(), 1);
}

TEST(ConstraintTest, TestDeredundancy) {
  Constraint eq0{Eq, {Literal(int32_t(0))}};
  Constraint ne1{Ne, {Literal(int32_t(1))}};

  // If x == 0, then x != 1 is redundant, and does not need to be added, is it
  // is implied by x == 0.
  AndedConstraintSet s;
  s.set(eq0);
  s.approximateAnd(ne1);
  EXPECT_EQ(s.size(), 1);
  EXPECT_EQ(s[0], eq0);

  // Reverse order, same result, even though we added x == 0 last: we remove
  // x != 1.
  AndedConstraintSet t;
  t.set(ne1);
  t.approximateAnd(eq0);
  EXPECT_EQ(t.size(), 1);
  EXPECT_EQ(t[0], eq0);
}

// Check that a set is equal to a constraint.
static void check(const AndedConstraintSet& s, const Constraint& c) {
  EXPECT_EQ(s.size(), 1);
  EXPECT_EQ(s[0], c);
}

TEST(ConstraintTest, TestBasicBlockConstraintMap) {
  // Maps begin unreachable.
  BasicBlockConstraintMap map;

  EXPECT_TRUE(map.unreachable);
  map.setReachable();
  EXPECT_FALSE(map.unreachable);
}

TEST(ConstraintTest, TestBasicBlockConstraintMap_Set) {
  Constraint eq0{Eq, {Literal(int32_t(0))}};
  Constraint eq1{Eq, {Literal(int32_t(1))}};
  Constraint eq2{Eq, {Literal(int32_t(2))}};

  BasicBlockConstraintMap map;
  map.setReachable();

  // Set local 0 to 0. It should read back the same.
  map.set(0, eq0);
  check(map.get(0), eq0);

  // Set another value, replacing the first.
  map.set(0, eq1);
  check(map.get(0), eq1);

  // Set a value using an expression.
  Const c;
  c.value = Literal(int32_t(2));
  c.type = Type::i32;
  map.set(0, &c);
  check(map.get(0), eq2);

  // Set an unfamiliar expression, leading to us knowing nothing.
  Nop nop;
  map.set(0, &nop);
  EXPECT_TRUE(map.get(0).provesNothing());
}

TEST(ConstraintTest, TestAddOneSigned) {
  // Less then, signed, a constant 5.
  Constraint lts_c5{LtS, {Literal(int32_t(5))}};

  // Less than, or equal.
  Constraint les_c5{LeS, {Literal(int32_t(5))}};

  BasicBlockConstraintMap map;
  map.setReachable();

  LocalGet get;
  get.index = 0;
  get.type = Type::i32;

  Const c;
  c.value = Literal(int32_t(1));
  c.type = Type::i32;

  Binary add;
  add.op = AddInt32;
  add.type = Type::i32;
  add.left = &get;
  add.right = &c;

  // Local 0 starts out less than 5.
  map.set(0, lts_c5);
  check(map.get(0), lts_c5);

  // Local 1 is equal to local 0 plus 1. That means it is less than, or equal
  // to, 5.
  map.set(1, &add);
  check(map.get(1), les_c5);

  // Local 0 did not change.
  check(map.get(0), lts_c5);

  // Setting 0 to an add of itself also works.
  map.set(0, &add);
  check(map.get(0), les_c5);

  // As above, but the constraints reference a local, not a constant.
  Constraint lts_l7{LtS, {Index(7)}};
  Constraint les_l7{LeS, {Index(7)}};
  map.set(0, lts_l7);
  map.set(0, &add);
  check(map.get(0), les_l7);

  // As above, but using unsigned and not signed comparisons.
  Constraint ltu_l7{LtU, {Index(7)}};
  Constraint leu_l7{LeU, {Index(7)}};
  map.set(0, ltu_l7);
  map.set(0, &add);
  check(map.get(0), leu_l7);

  // As above, but with another constraint $x != $42, which must be discarded as
  // we cannot infer how it changes due to the +=1 of the add (we might now be
  // equal to local $42, for all we know).
  Constraint ne_42{Ne, {Index(42)}};
  map.set(0, ne_42);
  map.approximateAnd(0, ltu_l7);
  map.set(0, &add);
  // After discarding what we can't reason about, we are left with something
  // useful.
  check(map.get(0), leu_l7);

  // As above, but now all we have are things to discard.
  map.set(0, ne_42);
  map.set(0, &add);
  EXPECT_TRUE(map.get(0).provesNothing());
}

// TODO: test an approximateOr of { x = 10 } and { x >= 0 }, once we support
//       inequalities
