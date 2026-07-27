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

#include <optional>

#include "ir/constraint.h"
#include "ir/match.h"
#include "ir/properties.h"
#include "wasm.h"

namespace wasm::constraint {

// To faciliate loop optimization, we implement two special opcodes, which
// represent an abstract/symbolic way to represent incrementing loop variables.
// As motivation, consider this loop:
//
//  x = 0
//  do {
//    ..work..
//    x++
//  } while (x < 100)
//
// If we do a naive flow here, then the loop is reached twice, once with x=0 and
// one that is initially unreachable. At the top of the loop we therefore start
// with 0, and the ++ turns that to 1. Now the top of the loop receives 0 and 1,
// so we can infer constraints x >= 0 && x <= 1, and we can proceed here to
// literally interpret the loop at compile time. But this is not what we want!
// To handle long loops and ones against unknown variables, we want to find the
// limit of what the loop variable can contain.
//
// To infer such limits, we make the x++ operation emit an "Incremented"
// constraint. Incremented(0) means that the value was 0, has been incremented
// once already, and is still in the process of incrementing, i.e., loosely, it
// can be described as "this can be 1, or 2, or 3, and so forth". There is a
// process of incrementation here which we do not know the bounds of yet, but we
// know where it started. Using this, here is how a flow in ConstraintAnalysis
// can operate:
//
//  * As mentioned above, initially the loop receives a single input 0.
//  * x++ turns that 0 into Incremented(0).
//  * The branch returning to the top of the loop applies the constraint
//    x < 100, and ANDing that with x = Incremented(0) gives us
//    { x >= 1 && x < 100 }. That is, when Incremented(x) is true at the same
//    time as a limit on x, that limit tells us where the process of
//    incrementing stops, giving us a range.
//  * Returning to the top of the loop, 0 is also possible, for a total of
//    x >= 0 && x < 100, which is exactly what we want to infer, allowing us to
//    remove bounds checks in the ..work.. code.
//
// We also want to optimize loops like this, where the condition is at the top:
//
//  x = 0
//  while (x < 100) {
//    ..work..
//    x++
//  }
//
// Now the flow looks like this:
//
//  * Again, initially the loop receives a single input 0.
//  * { x = 0 && x < 100 } is still x = 0.
//  * x++ turns that 0 into Incremented(0).
//  * At the top of the loop, we merge 0 and Incremented(0) into
//    Incrementing(0) (note: |ed| replaced by |ing|). Incrementing is the same
//    as Incremented, except it has not necessarily been incremented once
//    already, so Incrementing(0) means, loosely, "this can be 0, or 1, or 2,
//    and so forth".
//  * Incrementing(0) && x < 100 turns into x >= 0 && x < 100, as we want.
//
// Note that Incremented(0) is the same as Incrementing(1). We provide both
// because we need them to handle the non-constant case, i.e., Incremented(z)
// for a local z.
//
// One way of looking at Incremented/Incrementing is as abstract operations with
// specific semantics defined over them. Another perspective is to see them as
// sort of like infinity or episilon in calculus: when proving limits, those are
// not actual values, but represent a *process* that unfolds (i.e. f(infinity)
// does not mean "calculate f on this one value" but rather "calculate f on a
// sequence of values and see what happens").
const auto Incremented = Abstract::User1;
const auto Incrementing = Abstract::User2;

namespace {

Result TrueFalse(bool x) { return x ? True : False; }

Result TrueFalse(Literal x) { return TrueFalse(x.getUnsigned()); }

// Evaluate whether a => b, where a and b are operations on constants.
Result provesConstantPair(Abstract::Op aOp,
                          const Literal& aConstant,
                          Abstract::Op bOp,
                          const Literal& bConstant,
                          bool recursing = false) {
  using namespace Abstract;

  // a == A =?=> a op B. Simply apply A to the operation against B.
  if (aOp == Eq) {
    switch (bOp) {
      case Eq:
        return TrueFalse(aConstant == bConstant);
      case Ne:
        return TrueFalse(aConstant != bConstant);
      case LtS:
        return TrueFalse(aConstant.ltS(bConstant));
      case LeS:
        return TrueFalse(aConstant.leS(bConstant));
      case GtS:
        return TrueFalse(aConstant.gtS(bConstant));
      case GeS:
        return TrueFalse(aConstant.geS(bConstant));
      case LtU:
        return TrueFalse(aConstant.ltU(bConstant));
      case LeU:
        return TrueFalse(aConstant.leU(bConstant));
      case GtU:
        return TrueFalse(aConstant.gtU(bConstant));
      case GeU:
        return TrueFalse(aConstant.geU(bConstant));
      default: {
      }
    }
  }

  // a != A =?=> a == B. False if A = B, else unknown.
  if (aOp == Ne && bOp == Eq) {
    if (aConstant == bConstant) {
      return False;
    }
  }

  // a != A =?=> a != B. True if A = B, else unknown.
  if (aOp == Ne && bOp == Ne) {
    if (aConstant == bConstant) {
      return True;
    }
  }

  if (!recursing) {
    // The flipped operation may tell us something:  y ==> !x  implies
    // x ==> y  is false (because if not, then x would prove y, and y would
    // prove !x, a contradiction).
    if (provesConstantPair(bOp, bConstant, aOp, aConstant, true) == False) {
      return False;
    }
  }

  // TODO: handle all the rest of >, >=, <, and <=
  return Unknown;
}

// Core comparison of two constraints: whether a => b
Result provesPair(const Constraint& a, const Constraint& b) {
  // A thing always implies itself.
  if (a == b) {
    return True;
  }

  // A thing always implies its negation is false.
  if (a == b.negate()) {
    return False;
  }

  // Comparisons of two constants.
  auto* aConstant = std::get_if<Literal>(&a.term);
  auto* bConstant = std::get_if<Literal>(&b.term);
  if (aConstant && bConstant) {
    return provesConstantPair(a.op, *aConstant, b.op, *bConstant);
  }

  return Unknown;
}

} // anonymous namespace

Result AndedConstraintSet::proves(const Constraint& condition) const {
  if (provesEverything()) {
    return True;
  }
  // Note we do not need to handle the provesNothing case in a special way: the
  // loop below finds nothing.

  // Sometimes a single constraint is enough to determine the condition.
  for (auto& c : *this) {
    auto result = provesPair(c, condition);
    if (result != Unknown) {
      return result;
    }
  }

  // TODO smarts for multiple constraints

  // Otherwise, who knows.
  return Unknown;
}

Result AndedConstraintSet::proves(const AndedConstraintSet& other) const {
  if (provesEverything()) {
    return True;
  }

  if (other.provesEverything()) {
    // We are not a contradiction, but other is, so we prove it false.
    return False;
  }

  bool hasUnknown = false;

  for (auto& c : other) {
    auto result = proves(c);
    if (result == False) {
      // The entire conjunction is proven false.
      return False;
    }
    if (result == Unknown) {
      hasUnknown = true;
    }
  }

  return hasUnknown ? Unknown : True;
}

namespace {

// Do an AND on a pair of constraints, looking for a way to fuse them together
// into a single constraint that represents them both, while assuming the
// constraints have an equal term. If we fail, return nullopt.
std::optional<Constraint> fusedApproximateAndTermEqualPair(
  const Abstract::Op aOp, const Abstract::Op bOp, const Term& term) {
  using namespace Abstract;

  // x < C && x <= C  ===  x < C
  if (aOp == LtS && bOp == LeS) {
    return Constraint{LtS, term};
  }
  if (aOp == LtU && bOp == LeU) {
    return Constraint{LtU, term};
  }

  // TODO: all the rest

  return {};
}

#if 0
// A variable in a match. If we see the same Var - identified by address - in
// two places, it must be equal in them. For example,
//
//  Var A;
//  ..match expression with foo(A, A)..
//
// The matcher will check that foo is sent the same thing twice.
struct Var {};

// A matcher constraint: an abstraction over a normal Constraint, which can also
// contain Vars.
struct MatcherConstraint {
  Abstract::Op op;
  // Var addresses are how we identify them, so we store a pointer.
  Var* term = nullptr;

  MatcherConstraint() = default;
  MatcherConstraint(Abstract::Op op, Var& term) : op(op), term(&term) {}

  bool operator==(const MatcherConstraint&) const = default;
  bool operator<(const MatcherConstraint& other) const {
    // Ordered in a parallel way to Constraints, so that when we compare, things
    // line up.
    if (op != other.op) {
      return op < other.op;
    }
    return term < other.term;
  }
};

// A matcher object. This pattern-matches over abstractions of Constraints.
struct Matcher {
  // Set up a pattern containing two sets of constraints.
  Matcher(const MatcherConstraint& mc1_, const MatcherConstraint& mc2_);

  // Add a requirement on this pattern, a demand on the Vars. For example, we
  // can require that Var A - whatever we matched it as - is less than Var B -
  // whatever we matched that as.
  //
  // For convenience, return this Matcher object (i.e. the builder pattern).
  Matcher& require(Var& a, Abstract::Op op, Var& b);

  // When a match succeeds, we return a map of Vars to Terms, allowing the user
  // to find out what each Var matched against. For example, the pattern foo(A)
  // when matched successfully against foo(5) will provide a mapping of A to 5.
  struct VarTermMap : public std::unordered_map<Var*, Term> {
    // As a convenience, allow using the object instead of the pointer.
    Term& operator[](Var& var) {
      return std::unordered_map<Var*, Term>::operator[](&var);
    }
  };

  // Check if the pattern matches given inputs. Returns the mapping described
  // above, if we succeed.
  std::optional<VarTermMap> check(const Constraint& a, const Constraint& b);

private:
  MatcherConstraint mc1;
  MatcherConstraint mc2;

  struct Requirement {
    Var* a;
    Abstract::Op op;
    Var* b;
  };

  SmallVector<Requirement, 2> requirements;
};

Matcher::Matcher(const MatcherConstraint& mc1_, const MatcherConstraint& mc2_)
  : mc1(mc1_), mc2(mc2_) {
  // Sort the sets like Constraints are sorted, so that when we compare, things
  // line up.
  std::sort(mc1.begin(), mc1.end());
  std::sort(mc2.begin(), mc2.end());
}

Matcher& Matcher::require(Var& a, Abstract::Op op, Var& b) {
  requirements.push_back({&a, op, &b});
  return *this;
}

std::optional<Matcher::VarTermMap> Matcher::checkInternal(const Constraint& a,
                                                          const Constraint& b) {

  // TODO: optimize all this for speed

  if (a.size() != mc1.size() || b.size() != mc2.size()) {
    return {};
  }

  // The sizes match, at least. Parse in more detail, building up a mapping of
  // Vars to concrete Terms.
  VarTermMap varTermMap;

  auto parse = [&](const Constraint& input, const MatcherConstraint& pattern) {
    // The operation must match.
    if (input[i].op != pattern[i].op) {
      return false;
    }

    // The term must match, or define a new unknown value.
    auto [iter, inserted] = varTermMap.insert({pattern[i].term, input[i].term});
    if (!inserted) {
      // The Var in the pattern is already mapped and known. The input here
      // must match the prior appearance.
      if (input[i].term != iter->second) {
        return false;
      }
    }

    return true;
  };

  if (!parse(a, mc1) || !parse(b, mc2)) {
    return {};
  }

  // Check requirements on the vars.
  for (auto& [a, op, b] : requirements) {
    auto aTerm = varTermMap[*a];
    auto bTerm = varTermMap[*b];

    // Check if { x == a } proves { x op b } is true.
    if (provesPair({Abstract::Eq, aTerm}, {op, bTerm}) != True) {
      return {};
    }
  }

  return varTermMap;
}

/*
  Var C, D;
  // XXX do we need Matcher... maybe nott
  if (auto result = Matcher({Incremented, C}, {LeS, D})
                      .require(C, LtS, D)
                      .check(self, other)) {
    auto& values = *result;
    return ConstraintVector{{GtS, values[C]}, {LeS, values[D]}};
  }
*/
#endif

// Do an AND on a pair of constraints, where a.op is Incremented.
std::optional<ConstraintVector> approximateAndIncremented(const Constraint& a,
                                                        const Constraint& b) {
  assert(a.op == Incremented);

  using namespace Abstract;

  // If we see an upper bound to the process of incrementation, which does not
  // allow for overflows, we know we can increment at most to that bound:
  //
  //   x = Incremented(C) && x <= D, where C < D  =>  x > C && x <= D
  //
  // Note what happens if we don't have C < D: x = Incremented(10) && x <= 5 can
  // only be true if x incremented up from 10, overflowed, and then became <= 5.
  // In this case, we can't apply the constraint x > 10.
  if (b.op == LeS) {
    if (auto* ac = std::get_if<Literal>(&a.term)) {
      if (auto* bc = std::get_if<Literal>(&b.term)) {
        if (TrueFalse(ac->ltS(*bc)) == True) {
          return ConstraintVector{Constraint{GtS, a.term}, b};
        }
      }
    }
    // Otherwise, there might be an overflow, but we can at least keep b (x <= 5
    // in the example above).
    return ConstraintVector{b};
  }

  return std::nullopt;
}

// Do an AND on a pair of constraints, looking for a way to fuse them together
// into a list of constraints that represents them both. If we fail, return
// nullopt.
std::optional<ConstraintVector> approximateAndPair(const Constraint& a,
                                                   const Constraint& b,
                                                   bool recursing = false) {
  // If a proves b is true, all we need is a (e.g. { x == 5 && x > 0 } => x == 5
  if (provesPair(a, b) == True) {
    return ConstraintVector{a};
  }

  if (a.term == b.term) {
    if (auto result = fusedApproximateAndTermEqualPair(a.op, b.op, a.term)) {
      return ConstraintVector{*result};
    }
  }

  if (a.op == Incremented) {
    if (auto result = approximateAndIncremented(a, b)) {
      return result;
    }
  }

  if (!recursing) {
    // The flipped form may be recognized.
    return approximateAndPair(b, a, true);
  }

  return {};
}

} // anonymous namespace

void AndedConstraintSet::approximateAnd(const Constraint& c) {
  if (provesEverything()) {
    // Nothing to add.
    return;
  }

  auto result = proves(c);
  if (result == True) {
    // We already prove c to be true, so it adds nothing.
    return;
  } else if (result == False) {
    // We are now a contradiction.
    setProvesEverything();
    return;
  }

  for (auto& existing : *this) {
    // Some ANDed constraints fuse together into a new constraint.
    if (auto fused = approximateAndPair(existing, c)) {
      // Atm no pattern returns nothing, so we do not need to handle that case.
      assert(!fused->empty());

      // The first item can just replace |existing|.
      existing = (*fused)[0];

      // The rest can be appended, if we have room.
      for (Index i = 1; i < fused->size() && size() < MaxConstraints; i++) {
        push_back((*fused)[i]);
      }

      // Sort to ensure everything is in the right place.
      std::sort(begin(), end());

      return;
    }
  }

  if (size() < MaxConstraints) {
    // Insert into the right place, keeping us sorted.
    insert(std::upper_bound(begin(), end(), c), c);
    return;
  }

  // Otherwise, just do not add this one.
  // TODO: We could try to be clever and see if one of the existing ones makes
  //       more sense to drop. In particular, we should prefer "better" ones
  //       like > over >= and so forth (sorting more precise ones earlier may be
  //       useful to implement that).
}

namespace {

// Do an OR of a pair of constraints where the terms are known to be equal. If
// we can't find a good way to express their ORing, return nullopt.
std::optional<Constraint> approximateOrTermEqualPair(const Abstract::Op aOp,
                                                     const Abstract::Op bOp,
                                                     const Term& term) {
  using namespace Abstract;

  // x == C || x > C  ===  x >= C
  if (aOp == Eq && bOp == GtS) {
    return Constraint{GeS, term};
  }

  // TODO: all the rest

  return {};
}

// Do an OR of a pair of constraints. If we can't find a good way to express
// their ORing, return nullopt.
std::optional<Constraint> approximateOrPair(const Constraint& a,
                                            const Constraint& b,
                                            bool recursing = false) {
  if (a.term == b.term) {
    if (auto result = approximateOrTermEqualPair(a.op, b.op, a.term)) {
      return result;
    }
  }

  // If a proves b, e.g. x = 5 proves x >= 0 is true, then the OR is b.
  if (provesPair(a, b) == True) {
    return b;
  }

  // TODO: more smarts

  if (!recursing) {
    // The flipped form may be recognized.
    return approximateOrPair(b, a, true);
  }

  return {};
}

// Do an OR in full detail, looking at every constraint in each of the given
// sets.
AndedConstraintSet detailedApproximateOr(const AndedConstraintSet& a,
                                         const AndedConstraintSet& b) {
  // We can process this in full detail by looking at all the combinations of
  // individual constraints, because of the distributive property:
  //
  // (A & B) | (C & D) == ((A & B) | C) & ((A & B) | D)
  //                   == (A | C) & (B | C) & (A | D) & (B | D)
  //
  // This is quadratic, but constraint sets are limited to a very small size,
  // making this reasonable.
  //
  // Also, note that we don't need to worry about new contradictions here: ORing
  // things never leads to a contradiction, and we can assume the inputs are
  // not contradictions.
  assert(!a.provesEverything() && !b.provesEverything());

  auto result = AndedConstraintSet::makeProvesNothing();
  for (auto& ac : a) {
    for (auto& bc : b) {
      if (auto combined = approximateOrPair(ac, bc)) {
        // We found something useful by ORing them, keep it.
        result.approximateAnd(*combined);
      }
    }
  }
  return result;
}

} // anonymous namespace

bool AndedConstraintSet::approximateOr(const AndedConstraintSet& other) {
  // If one proves everything, the only thing that matters is the other.
  if (other.provesEverything()) {
    return false;
  }
  if (provesEverything()) {
    *this = other;
    return true;
  }

  // If this is already implied by current constraints, then it is redundant.
  // E.g. if we are { x = 10 } and other is { x >= 0 } then all we need is
  // { x >= 0 } as the result of the OR.
  if (other.proves(*this) == True) {
    return false;
  }
  if (proves(other) == True) {
    *this = other;
    return true;
  }

  // For more complex cases, do a detailed analysis.
  auto result = detailedApproximateOr(*this, other);
  auto changed = (result != *this);
  *this = result;
  return changed;
}

std::optional<LocalConstraint> LocalConstraint::parse(Expression* curr) {
  auto parseEqZArgument =
    [&](Expression* value) -> std::optional<LocalConstraint> {
    if (auto* get = value->dynCast<LocalGet>()) {
      // Canonicalize EqZ to Eq of 0.
      auto value = Literal::makeZero(get->type);
      return LocalConstraint{get->index, Constraint{Abstract::Eq, {value}}};
    }
    // TODO: Recursively parse and reverse a constraint
    return {};
  };

  if (auto* unary = curr->dynCast<Unary>()) {
    if (Abstract::getUnary(unary->type, Abstract::EqZ) == unary->op) {
      return parseEqZArgument(unary->value);
    }
    return {};
  }

  if (auto* refIsNull = curr->dynCast<RefIsNull>()) {
    return parseEqZArgument(refIsNull->value);
  }

  // Parse a get or a constant.
  auto parseTerm = [&](Expression* expr) -> std::optional<Term> {
    if (auto* get = expr->dynCast<LocalGet>()) {
      return Term{get->index};
    }
    if (Properties::isSingleConstantExpression(expr)) {
      return Term{Properties::getLiteral(expr)};
    }
    return {};
  };

  auto parseBinaryArguments =
    [&](Abstract::Op op,
        Expression* left,
        Expression* right) -> std::optional<LocalConstraint> {
    // The left must be a get.
    if (auto* get = left->dynCast<LocalGet>()) {
      // The right can be any term.
      if (auto value = parseTerm(right)) {
        return LocalConstraint{get->index, Constraint{op, *value}};
      }
    }
    return {};
  };

  if (auto* binary = curr->dynCast<Binary>()) {
    // The operation must be one we recognize.
    for (auto op : {Abstract::Eq,
                    Abstract::Ne,
                    Abstract::LtS,
                    Abstract::LtU,
                    Abstract::LeS,
                    Abstract::LeU,
                    Abstract::GtS,
                    Abstract::GtU,
                    Abstract::GeS,
                    Abstract::GeU}) {
      if (Abstract::getBinary(binary->type, op) == binary->op) {
        return parseBinaryArguments(op, binary->left, binary->right);
      }
    }
    return {};
  }

  if (auto* refEq = curr->dynCast<RefEq>()) {
    return parseBinaryArguments(Abstract::Eq, refEq->left, refEq->right);
  }

  return {};
}

std::optional<LocalConstraint>
LocalConstraint::parseCondition(Expression* curr) {
  // A get by itself is a check for not being null.
  if (auto* get = curr->dynCast<LocalGet>()) {
    auto value = Literal::makeZero(get->type);
    return LocalConstraint{get->index, Constraint{Abstract::Ne, {value}}};
  }

  // Otherwise, parse normally.
  return parse(curr);
};

void LocalConstraint::flip() {
  auto other = std::get<Index>(constraint.term);
  constraint.term = Term{local};
  local = other;
  if (Abstract::isRelationalAntisymmetric(constraint.op)) {
    constraint.op = Abstract::negateRelational(constraint.op);
  } else {
    // All we support for now are symmetric and antisymmetric operations.
    assert(Abstract::isRelationalSymmetric(constraint.op));
  }
}

void BasicBlockConstraintMap::set(Index index, const Constraint& c) {
  // We should not set values in unreachable code.
  assert(!unreachable);

  // Clear the old state.
  eraseStaleRefs(index);
  map.erase(index);

  // Apply the constraint.
  approximateAnd(index, c);
}

void BasicBlockConstraintMap::set(Index index,
                                  const AndedConstraintSet& constraints) {
  // As above, but with a loop after.
  assert(!unreachable);
  eraseStaleRefs(index);
  map.erase(index);

  // Apply the constraints.
  if (constraints.provesNothing()) {
    setProvesNothing(index);
  } else if (constraints.provesEverything()) {
    // The entire basic block is unreachable.
    unreachable = true;
    map.clear();
  } else {
    for (auto& c : constraints) {
      approximateAnd(index, c);
    }
  }
}

void BasicBlockConstraintMap::set(Index index, Expression* value) {
  using namespace Match;
  using namespace Abstract;

  // Apply a constraint to a value.
  if (Properties::isSingleConstantExpression(value)) {
    set(index, Constraint{Eq, {Properties::getLiteral(value)}});
    return;
  }

  // Apply a constraint to a local.
  if (auto* get = value->dynCast<LocalGet>()) {
    set(index, Constraint{Eq, {get->index}});
    return;
  }

  // Special handling for Incrementing/Incremented.
  {
    // x = y + 1
    Index y;
    if (matches(value, binary(Add, local(&y), ival(1)))) {
      // If we know that y is equal to a constant, we can set x as equal to
      // Incremented(that thing).
      //
      // TODO: Handle locals and not just constants, but it is not clear we can
      //       infer enough in those cases (without knowing a concrete initial
      //       value, we can't infer that the incrementation process does not
      //       overflow, for example).
      for (auto& c : get(y)) {
        if (c.op == Eq && std::holds_alternative<Literal>(c.term)) {
          set(index, Constraint{Incremented, c.term});
          return;
        }
      }

      // Otherwise, fall through to below: we don't know how to increment this.
    }
  }

  // We know and can prove nothing.
  setProvesNothing(index);
}

void BasicBlockConstraintMap::setProvesNothing(Index index) {
  assert(!unreachable);
  eraseStaleRefs(index);
  map.erase(index);
}

bool BasicBlockConstraintMap::approximateOr(
  const BasicBlockConstraintMap& other) {
  // If one is unreachable, it adds nothing to the other.
  if (other.unreachable) {
    return false;
  }
  if (unreachable) {
    *this = other;
    return true;
  }

  // We only need to loop on our locals, as any local that is missing in us is
  // one that would end up proving nothing (and get removed).
  bool changed = false;
  for (auto& [local, constraints] : map) {
    changed |= constraints.approximateOr(other.get(local));
  }

  // Anything that became trivial after the OR must be removed.
  std::erase_if(map, [&](const auto& item) {
    const auto& [local, constraints] = item;
    // We do not store contradictions.
    assert(!constraints.provesEverything());
    if (constraints.provesNothing()) {
      changed = true;
      return true;
    }
    return false;
  });

  return changed;
}

void BasicBlockConstraintMap::approximateAndInternal(Index index,
                                                     const Constraint& c,
                                                     bool flip,
                                                     bool isCopy) {
  // We should not be applying constraints when already unreachable.
  assert(!unreachable);

  Constraint actual = c;
  if (flip && (Abstract::isRelationalSymmetric(c.op) ||
               Abstract::isRelationalAntisymmetric(c.op))) {
    LocalConstraint flipped{index, c};
    flipped.flip();
    index = flipped.local;
    actual = flipped.constraint;
  }

  // Never add constraints to ourselves (x == x, etc., which can happen due to
  // copying/flipping).
  if (auto* other = std::get_if<Index>(&actual.term)) {
    if (*other == index) {
      return;
    }
  }

  // Refer to the constraints for this index. If this is the first access of
  // the local, then we insert a new item into the map, which has a default of
  // proxesEverything, which we need to flip (provesEverything cannot otherwise
  // be found in the map, as we never store it).
  auto [iter, _] = map.insert({index, AndedConstraintSet::makeProvesNothing()});
  auto& indexConstraints = iter->second;
  // As in ::set(), this makes the map temporarily invalid until the
  // approximateAnd, as we don't store proves-nothing in the map, normally.

  indexConstraints.approximateAnd(actual);

  if (indexConstraints.provesEverything()) {
    // We just proved we are in unreachable code.
    unreachable = true;
    map.clear();
    return;
  }

  // We just added a constraint, so we can prove something (we may lose some
  // information as this is an approximate AND, but we cannot lose it all).
  assert(!indexConstraints.provesNothing());

  // Add a ref of what we are adding. Note that the approximation above may end
  // up not actually adding this, or adding only part of this, but it is safe to
  // always add a ref (at the cost of minor wasted work).
  noteRefs(index, actual);

  // If this is not the flipped version, and it refers to a local, add the
  // flipped one too.
  if (!flip && std::holds_alternative<Index>(actual.term)) {
    approximateAndInternal(index, actual, true, isCopy);
    if (unreachable) {
      // We just found a contradiction.
      return;
    }
  }

  // If this constraint is simply "== x", then we are equal to that other local
  // x, and can copy its constraints (if we are not already such a copy).
  if (!isCopy) {
    if (auto* other = std::get_if<Index>(&actual.term)) {
      if (actual.op == Abstract::Eq) {
        for (auto& otherC : get(*other)) {
          approximateAndInternal(index, otherC, false, true);
          if (unreachable) {
            return;
          }
        }
      }
    }
  }
}

void BasicBlockConstraintMap::noteRefs(Index index, const Constraint& c) {
  if (auto* i = std::get_if<Index>(&c.term)) {
    refs[*i].insert(index);
  }
}

void BasicBlockConstraintMap::eraseStaleRefs(Index index) {
  auto iter = refs.find(index);
  if (iter == refs.end()) {
    return;
  }

  auto& refIndexes = iter->second;

  for (auto refIndex : refIndexes) {
    if (auto iter = map.find(refIndex); iter != map.end()) {
      auto& refConstraints = iter->second;
      std::erase_if(refConstraints, [&](const auto& c) {
        if (auto* i = std::get_if<Index>(&c.term)) {
          if (*i == index) {
            return true;
          }
        }
        return false;
      });
      if (refConstraints.empty()) {
        // This became trivial.
        map.erase(iter);
      }
    }
  }
}

std::ostream& operator<<(std::ostream& o, const Constraint& c) {
  o << "Constraint{" << c.op << ", ";
  if (auto* cc = std::get_if<Literal>(&c.term)) {
    o << *cc;
  } else if (auto* i = std::get_if<Index>(&c.term)) {
    o << "Index(" << *i << ')';
  }
  o << '}';
  return o;
}

std::ostream& operator<<(std::ostream& o, const AndedConstraintSet& set) {
  if (set.provesEverything()) {
    o << "AndedConstraintSet(contradiction)";
    return o;
  }
  o << "AndedConstraintSet{";
  bool first = true;
  for (auto& constraint : set) {
    if (first) {
      first = false;
    } else {
      o << ", ";
    }
    o << constraint;
  }
  o << '}';
  return o;
}

std::ostream& operator<<(std::ostream& o, const BasicBlockConstraintMap& map) {
  if (map.unreachable) {
    o << "BasicBlockConstraintMap(unreachable)";
    return o;
  }
  o << "BasicBlockConstraintMap{";
  bool first = true;
  for (auto& [local, constraints] : map.map) {
    if (first) {
      first = false;
    } else {
      o << ", ";
    }
    o << local << ": " << constraints;
  }
  o << '}';
  return o;
}

} // namespace wasm::constraint
