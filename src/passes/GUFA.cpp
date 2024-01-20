/*
 * Copyright 2022 WebAssembly Community Group participants
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
// Grand Unified Flow Analysis (GUFA)
//
// Optimize based on information about what content can appear in each location
// in the program. This does a whole-program analysis to find that out and
// hopefully learn more than the type system does - for example, a type might be
// $A, which means $A or any subtype can appear there, but perhaps the analysis
// can find that only $A', a particular subtype, can appear there in practice,
// and not $A or any subtypes of $A', etc. Or, we may find that no type is
// actually possible at a particular location, say if we can prove that the
// casts on the way to that location allow nothing through. We can also find
// that only a particular value is possible of that type.
//
// GUFA will infer constants and unreachability, and add those to the code. This
// can increase code size if further optimizations are not run later like dead
// code elimination and vacuum. The "optimizing" variant of this pass will run
// such followup opts automatically in functions where we make changes, and so
// it is useful if GUFA is run near the end of the optimization pipeline.
//
// A variation of this pass will add casts anywhere we can infer a more specific
// type, see |castAll| below.
//
// TODO: GUFA + polymorphic devirtualization + traps-never-happen. If we see
//       that the possible call targets are {A, B, C}, and GUFA info lets us
//       prove that A, C will trap if called - say, if they cast the first
//       parameter to something GUFA proved it cannot be - then we can ignore
//       them, and devirtualize to a call to B.
//

#include "ir/drop.h"
#include "ir/eh-utils.h"
#include "ir/possible-contents.h"
#include "ir/properties.h"
#include "ir/utils.h"
#include "pass.h"
#include "wasm.h"

namespace wasm {

namespace {

struct GUFAOptimizer
  : public WalkerPass<
      PostWalker<GUFAOptimizer, UnifiedExpressionVisitor<GUFAOptimizer>>> {
  bool isFunctionParallel() override { return true; }

  ContentOracle& oracle;

  // Whether to run further optimizations in functions we modify.
  bool optimizing;

  // Whether to add casts to all things that we have inferred a more refined
  // type for. This increases code size immediately, but later optimizations
  // generally benefit enough from these casts that overall code size actually
  // decreases, even if some of these casts remain. However, aside from code
  // size there may be an increase in the number of casts performed at runtime,
  // so benchmark carefully.
  // TODO: Add a pass to remove casts not needed for validation, which users
  //       could run at the very end. However, even with such a pass we may end
  //       up with casts that are needed for validation that were not present
  //       before.
  bool castAll;

  GUFAOptimizer(ContentOracle& oracle, bool optimizing, bool castAll)
    : oracle(oracle), optimizing(optimizing), castAll(castAll) {}

  std::unique_ptr<Pass> create() override {
    return std::make_unique<GUFAOptimizer>(oracle, optimizing, castAll);
  }

  bool optimized = false;

  // As we optimize, we replace expressions and create new ones. For new ones
  // we can infer their contents based on what they replaced, e.g., if we
  // replaced a local.get with a const, then the PossibleContents of the const
  // are the same as the local.get (in this simple example, we could also just
  // infer them from the const itself, of course). Rather than update the
  // ContentOracle with new contents, which is a shared object among threads,
  // each function-parallel worker stores a map of new things it created to the
  // contents for them.
  std::unordered_map<Expression*, PossibleContents> newContents;

  Expression* replaceCurrent(Expression* rep) {
    newContents[rep] = oracle.getContents(getCurrent());

    return WalkerPass<
      PostWalker<GUFAOptimizer,
                 UnifiedExpressionVisitor<GUFAOptimizer>>>::replaceCurrent(rep);
  }

  const PossibleContents getContents(Expression* curr) {
    // If this is something we added ourselves, use that; otherwise the info is
    // in the oracle.
    if (auto iter = newContents.find(curr); iter != newContents.end()) {
      return iter->second;
    }

    return oracle.getContents(curr);
  }

  void visitExpression(Expression* curr) {
    // Skip things we can't improve in any way.
    auto type = curr->type;
    if (type == Type::unreachable || type == Type::none ||
        Properties::isConstantExpression(curr)) {
      return;
    }

    if (type.isTuple()) {
      // TODO: tuple types.
      return;
    }

    // Ok, this is an interesting location that we might optimize. See what the
    // oracle says is possible there.
    auto contents = getContents(curr);

    auto& options = getPassOptions();
    auto& wasm = *getModule();
    Builder builder(wasm);

    if (contents.getType() == Type::unreachable) {
      // This cannot contain any possible value at all. It must be unreachable
      // code.
      replaceCurrent(getDroppedChildrenAndAppend(
        curr, wasm, options, builder.makeUnreachable()));
      optimized = true;
      return;
    }

    // This is reachable. Check if we can emit something optimized for it.
    // TODO: can we handle more general things here too?
    if (!contents.canMakeExpression()) {
      return;
    }

    if (contents.isNull() && curr->type.isNullable()) {
      // Null values are all identical, so just fix up the type here if we need
      // to (the null's type might not fit in this expression, if it passed
      // through casts).
      if (!Type::isSubType(contents.getType(), curr->type)) {
        contents = PossibleContents::literal(
          Literal::makeNull(curr->type.getHeapType()));
      }

      // Note that if curr's type is *not* nullable, then the code will trap at
      // runtime (the null must arrive through a cast that will trap). We handle
      // that below, so we don't need to think about it here.

      // TODO: would emitting a more specific null be useful when valid?
    }

    auto* c = contents.makeExpression(wasm);

    // We can only place the constant value here if it has the right type. For
    // example, a block may return (ref any), that is, not allow a null, but in
    // practice only a null may flow there if it goes through casts that will
    // trap at runtime.
    // TODO: GUFA should eventually do this, but it will require it properly
    //       filtering content not just on ref.cast as it does now, but also
    //       ref.as etc. Once it does those we could assert on the type being
    //       valid here.
    if (Type::isSubType(c->type, curr->type)) {
      replaceCurrent(getDroppedChildrenAndAppend(curr, wasm, options, c));
      optimized = true;
    } else {
      // The type is not compatible: we cannot place |c| in this location, even
      // though we have proven it is the only value possible here.
      if (Properties::isConstantExpression(c)) {
        // The type is not compatible and this is a simple constant expression
        // like a ref.func. That means this code must be unreachable. (See below
        // for the case of a non-constant.)
        replaceCurrent(getDroppedChildrenAndAppend(
          curr, wasm, options, builder.makeUnreachable()));
        optimized = true;
      } else {
        // This is not a constant expression, but we are certain it is the right
        // value. Atm the only such case we handle is a global.get of an
        // immutable global. We don't know what the value will be, nor its
        // specific type, but we do know that a global.get will get that value
        // properly. However, in this case it does not have the right type for
        // this location. That can happen since the global.get does not have
        // exactly the proper type for the contents: the global.get might be
        // nullable, for example, even though the contents are not actually a
        // null. For example, consider what happens here:
        //
        //  (global $foo (ref null any) (struct.new $Foo))
        //  ..
        //    (ref.as_non_null
        //      (global.get $foo))
        //
        // We create a $Foo in the global $foo, so its value is not a null. But
        // the global's type is nullable, so the global.get's type will be as
        // well. When we get to the ref.as_non_null, we then want to replace it
        // with a global.get - in fact that's what its child already is, showing
        // it is the right content for it - but that global.get would not have a
        // non-nullable type like a ref.as_non_null must have, so we cannot
        // simply replace it.
        //
        // For now, do nothing here, but in some cases we could probably
        // optimize (e.g. by adding a ref.as_non_null in the example) TODO
        assert(c->is<GlobalGet>());
      }
    }
  }

  void visitRefEq(RefEq* curr) {
    if (curr->type == Type::unreachable) {
      // Leave this for DCE.
      return;
    }

    auto leftContents = getContents(curr->left);
    auto rightContents = getContents(curr->right);

    if (!PossibleContents::haveIntersection(leftContents, rightContents)) {
      // The contents prove the two sides cannot contain the same reference, so
      // we infer 0.
      //
      // Note that this is fine even if one of the sides is None. In that case,
      // no value is possible there, and the intersection is empty, so we will
      // get here and emit a 0. That 0 will never be reached as the None child
      // will be turned into an unreachable, so it does not cause any problem.
      auto* result = Builder(*getModule()).makeConst(Literal(int32_t(0)));
      replaceCurrent(getDroppedChildrenAndAppend(
        curr, *getModule(), getPassOptions(), result));
    }
  }

  void visitRefTest(RefTest* curr) {
    if (curr->type == Type::unreachable) {
      // Leave this for DCE.
      return;
    }

    auto refContents = getContents(curr->ref);
    auto refType = refContents.getType();
    if (refType.isRef()) {
      // We have some knowledge of the type here. Use that to optimize: RefTest
      // returns 1 if the input is of a subtype of the intended type, that is,
      // we are looking for a type in that cone of types.
      auto intendedContents = PossibleContents::fullConeType(curr->castType);

      auto optimize = [&](int32_t result) {
        auto* last = Builder(*getModule()).makeConst(Literal(int32_t(result)));
        replaceCurrent(getDroppedChildrenAndAppend(
          curr, *getModule(), getPassOptions(), last));
      };

      if (!PossibleContents::haveIntersection(refContents, intendedContents)) {
        optimize(0);
        return;
      } else if (PossibleContents::isSubContents(refContents,
                                                 intendedContents)) {
        optimize(1);
        return;
      }
    }

    // Stash all ref.test of struct.get that we see. There is a whole-program
    // optimization we can try for them later.
    // if (auto* get = curr->ref->dynCast<StructGet>()) {
    //  testsOfStructGets.push_back(curr);
    //}
  }

  // std::vector<RefTest*> testsOfStructGets;

  void visitRefCast(RefCast* curr) {
    auto currType = curr->type;
    auto inferredType = getContents(curr).getType();
    if (inferredType.isRef() && inferredType != currType &&
        Type::isSubType(inferredType, currType)) {
      // We have inferred that this will only contain something of a more
      // refined type, so we might as well cast to that more refined type.
      //
      // Note that we could in principle apply this in all expressions by adding
      // a cast. However, to be careful with code size, we only refine existing
      // here. See addNewCasts() for where we add entirely new casts.
      curr->type = inferredType;
      optimized = true;
    }

    // Apply the usual optimizations as well, such as potentially replacing this
    // with a constant.
    visitExpression(curr);
  }

  // TODO: If an instruction would trap on null, like struct.get, we could
  //       remove it here if it has no possible contents and if we are in
  //       traps-never-happen mode (that is, we'd have proven it can only trap,
  //       but we assume no traps happen, so it must be dead code). That info
  //       is present in OptimizeInstructions where it removes redundant
  //       ref.as_non_null (it removes them because it knows that the parent
  //       will trap on null anyhow), so maybe there is a way to share that
  //       information about parents.

  void visitFunction(Function* func) {
    if (optimized) {
      // Optimization may introduce more unreachables, which we need to
      // propagate.
      ReFinalize().walkFunctionInModule(func, getModule());
    }

    // Potentially add new casts after we do our first pass of optimizations +
    // refinalize (doing it after refinalizing lets us add as few new casts as
    // possible).
    if (castAll && addNewCasts(func)) {
      optimized = true;
    }

    if (!optimized) {
      return;
    }

    // We may add blocks around pops, which we must fix up.
    EHUtils::handleBlockNestedPops(func, *getModule());

    // If we are in "optimizing" mode, we'll also run some more passes on this
    // function that we just optimized. If not, leave now.
    if (!optimizing) {
      return;
    }

    PassRunner runner(getPassRunner());
    // New unreachables we added have created dead code we can remove. If we do
    // not do this, then running GUFA repeatedly can actually increase code size
    // (by adding multiple unneeded unreachables).
    runner.add("dce");
    // New drops we added allow us to remove more unused code and values. As
    // with unreachables, without a vacuum we may increase code size as in
    // nested expressions we may apply the same value multiple times:
    //
    //  (block $out
    //   (block $in
    //    (i32.const 10)))
    //
    // In each of the blocks we'll infer the value must be 10, so we'll end up
    // with this repeating code:
    //
    //  (block ;; a new block just to drop the old outer block
    //   (drop
    //    (block $out
    //     (drop
    //      (block $in
    //       (i32.const 10)
    //      )
    //     )
    //     (i32.const 10)
    //    )
    //   )
    //   (i32.const 10)
    //  )
    runner.add("vacuum");
    runner.runOnFunction(func);
  }

  // Add a new cast whenever we know a value contains a more refined type than
  // in the IR. Returns whether we optimized anything.
  bool addNewCasts(Function* func) {
    // Subtyping and casts only make sense if GC is enabled.
    if (!getModule()->features.hasGC()) {
      return false;
    }

    struct Adder : public PostWalker<Adder, UnifiedExpressionVisitor<Adder>> {
      GUFAOptimizer& parent;

      Adder(GUFAOptimizer& parent) : parent(parent) {}

      bool optimized = false;

      void visitExpression(Expression* curr) {
        if (!curr->type.isRef()) {
          // Ignore anything we cannot infer a type for.
          return;
        }

        auto oracleType = parent.getContents(curr).getType();
        if (oracleType.isRef() && oracleType != curr->type &&
            Type::isSubType(oracleType, curr->type)) {
          replaceCurrent(Builder(*getModule()).makeRefCast(curr, oracleType));
          optimized = true;
        }
      }
    };

    Adder adder(*this);
    adder.walkFunctionInModule(func, getModule());
    if (adder.optimized) {
      ReFinalize().walkFunctionInModule(func, getModule());
      return true;
    }
    return false;
  }
};

struct GUFAPass : public Pass {
  bool optimizing;
  bool castAll;

  GUFAPass(bool optimizing, bool castAll)
    : optimizing(optimizing), castAll(castAll) {}

  void run(Module* module) override {
    ContentOracle oracle(*module, getPassOptions());

    GUFAOptimizer(oracle, optimizing, castAll).run(getPassRunner(), module);

    // TODO: move this to before GUFAOPpt and pass it in to use in the Walk

    // Imagine that we see a test like this:
    //
    //  (ref.test $specific.vtable
    //    (struct.get $generic.object $vtable
    //      (REF)
    //    )
    //  )
    //
    // Here we read the vtable and then check if it has a specific type. If
    // vtable types parallel class types, which can be the case for languages
    // like Java, then we can test the class directly, saving the struct.get:
    //
    //  (ref.test $A
    //    (REF)
    //  )
    //
    // That is, when we find parallel class hierarchies then we can map
    // operations from one to the other, saving work. Such hierarchies look like
    // this:
    //
    //         $X             $vtable
    //        |  |             |   |
    //       $A  $B     $A.vtable $B.vtable
    //
    // And $A,$B are assigned $A.vtable,$B.vtable respectively in their vtable
    // field.
    //
    // To see if we have such opportunities, we look for fields (like the vtable
    // field in the example) whose type parallels the struct it is inside.
    SubTypes subTypes(*module); // TODO reuse from ContentOracle

    // We will build up a map of the information we've found. Specifically, for
    // each field - that is, for each HeapType+Index - we need a sub-map of the
    // relationship of things written there to the parent class. In the example
    // above, we'd find this:
    //
    //  infoMap[$X:$vtable] = { $A.vtable: $A, $B.vtable: $B }
    //                        |............sub-map...........|
    //
    // That indicates that $A.vtable is always written to $A and $B.vtable to
    // $B, in that field. All the subtypes must be present in such a sub-map, or
    // else a type exists for which the type hierarchies are not parallel and we
    // cannot optimize. We will also fill out these, though they are not that
    // useful:
    //
    //  infoMap[$A:$vtable] = { $A.vtable: $A }
    //  infoMap[$B:$vtable] = { $B.vtable: $B }
    //
    // Note that the direction of the sub-map is to allow optimization later:
    // when we see a ref.test of $A.vtable, we will see that we can instead do a
    // test on $A instead (and without reading the vtable).
    //
    // We also need a way to indicate that we cannot optimize because we cannot
    // build such a sub-map, which we do as follows: InfoMap maps to a
    // std::optional, and if it is nullopt (as it is when we first do
    // infoMap[loc]) that means we have yet to process it. If it is not nullopt
    // and the map is empty then that means we found invalid data, and so we
    // cleared the map to indicate failure to optimize there.
    using SubMap = std::unordered_map<HeapType, HeapType>;
    struct InfoMap : public std::unordered_map<DataLocation, std::optional<SubMap>> {
      void dump(Module* module) const {
        std::cerr << "dumping infoMap\n";
        for (auto& [loc, maybeSubMap] : *this) {
          std::cerr << "  " << module->typeNames[loc.type].name << ":" << loc.index << "\n";
          if (!maybeSubMap) {
            std::cerr << "    (nullopt)\n";
          } else {
            auto& subMap = *maybeSubMap;
            if (subMap.empty()) {
              std::cerr << "    (failed to infer parallel hierarchies)\n";
            } else {
              for (auto& [key, value] : subMap) {
                std::cerr << "    " << module->typeNames[key].name << ": " << module->typeNames[value].name << "\n";
              }
            }
          }
        }
      }
    };
    InfoMap infoMap;

    for (auto type : subTypes.types) {
      if (!type.isStruct()) {
        continue;
      }

      auto& fields = type.getStruct().fields;
      for (Index i = 0; i < fields.size(); i++) {
        // We only optimize immutable references here. (Subtyping is not
        // possible on mutable ones anyhow; and vtables are normally immutable.)
        auto field = fields[i];
        if (!field.type.isRef() || !field.mutable_ == Immutable) {
          continue;
        }

        auto typeLoc = DataLocation{type, i};
        auto typeContents = oracle.getContents(typeLoc);
        // We can only reason about exact types: for each type with the field,
        // we must see a specific type written. In the example above, type $A's
        // $vtable field must always contain $A.vtable and not a subtype.
        if (typeContents.isGlobal()) {
          // The global may have an exact type, use its content (we don't care
          // about global identity here).
          auto* global = module->getGlobal(typeContents.getGlobal());
          if (global->init) {
            typeContents = oracle.getContents(ExpressionLocation{global->init, 0});
            std::cerr << "  => " << typeContents << "\n";
          }
        }
        if (!typeContents.hasExactType()) {
          // Anything non-exact is bad for us.
          typeContents = PossibleContents::many();
        }

        // We need to apply the information here to all supertypes: loop on
        // them.
        auto t = type;
        while (1) {
          auto tLoc = DataLocation{t, i};
          auto& maybeSubMap = infoMap[tLoc];
          if (maybeSubMap && maybeSubMap->empty()) {
            // We found inexact data here earlier, and gave up, so ignore this
            // (all supers will also be marked so, already).
            break;
          }
          // Initialize this submap, if it has not been earlier.
          if (!maybeSubMap) {
            maybeSubMap = SubMap();
          }
          auto& subMap = *maybeSubMap;

          // Consider typeContents in combination with this sub-map.
          auto contents = typeContents;
          if (contents != PossibleContents::many()) {
            // The sub-map entry we would like to make, $A.vtable -> $A in the
            // example above, is contents.type -> type. See if that fits with
            // what is already there, as any discrepancy proves the hierarchies
            // are not parallel.
            auto& subMapValue = subMap[contents.getType().getHeapType()];
            if (subMapValue == HeapType()) {
              // This is the first thing we see here: write it.
              subMapValue = type;
            } else if (subMapValue != type) {
              // This is different, so we found a problem.
              contents = PossibleContents::many();
              std::cerr << "many1\n";
            }
          }
          if (contents == PossibleContents::many()) {
              std::cerr << "many2\n";
            // We ran into a problem. Clear the sub-map to indicate that.
            subMap.clear();
          }

          // Proceed to the super.
          auto super = t.getDeclaredSuperType();
          if (!super) {
            break;
          }
          t = *super;
        }
      }
    }

    // We built up the mapping described earlier, which describes when there is
    // a relationship between the type assigned in a field and the type itself.
    // We also need to verify that subtyping is isomorphic. That is, given
    //
    //         $X             $X.vtable
    //        |   |            |     |
    //       $A   $B     $A.vtable $B.vtable
    //
    // we've seen that each type $T is always written $T.vtable to its vtable.
    // We also need $A's and $B's vtable to be a subtype of $X's. Our data
    // structure looks like this:
    //
    //  infoMap[$X:$vtable] = { $X.vtable: $X, $A.vtable: $A, $B.vtable: $B }
    //  infoMap[$A:$vtable] = { $A.vtable: $A }
    //  infoMap[$B:$vtable] = { $B.vtable: $B }
    //
    // For each submap, we find the immediate subtypes of each entry and check
    // them.
    //
    // TODO: Avoid redundant work here.
    for (auto& [_, maybeSubMap] : infoMap) {
      // All submaps should be filled (or else there would be no entry at all).
      assert(maybeSubMap);
      auto& subMap = *maybeSubMap;

      auto fail = false;

      // In the example above, iterating on infoMap[$X:$vtable] will start with
      // key = $X.vtable and value = $X.
      for (auto [key, value] : subMap) {
        // Continuing the example, subKey will begin with $A.vtable (the first
        // immediate subtype of $X.vtable).
        for (auto subKey : subTypes.getImmediateSubTypes(key)) {
          auto iter = subMap.find(subKey);
          if (iter == subMap.end()) {
            // This subtype is not used and does not pose a problem.
            continue;
          }

          // Continuing the example, this is $A (the entry for $A.vtable in the
          // sub-map).
          auto expectedSubValue = iter->second;

          // Continuing the example, $A (expectedSubValue) must be an
          // immediate subtype of $X (value).
          auto super = expectedSubValue.getSuperType();
          if (!super || *super != value) {
            // Unfortunately we found a problem.
              std::cerr << "fail1\n"; // TODO invrestigate this noww
std::cerr << "key: " << module->typeNames[key].name << '\n';
std::cerr << "value: " << module->typeNames[value].name << '\n';
std::cerr << "subkey: " << module->typeNames[subKey].name << '\n';
std::cerr << "expectedSubValue: " << module->typeNames[expectedSubValue].name << '\n';
std::cerr << "*super          : " << module->typeNames[*super].name << '\n';

            fail = true;
            break;
          }
        }
        if (fail) {
          break;
        }
      }

      if (fail) {
        // We ran into a problem. Clear the sub-map to indicate that.
        // TODO: Perhaps we could only clear a subset of it?
        subMap.clear();
      }
    }

    infoMap.dump(module);

    // Now we know which fields are optimizable and which are not, and can
    // optimize. TODO p aralelize
    struct Optimizer : PostWalker<Optimizer> {
      const InfoMap& infoMap;

      Optimizer(const InfoMap& infoMap) : infoMap(infoMap) {}

      void visitRefTest(RefTest* curr) {
        if (auto* get = curr->ref->dynCast<StructGet>()) {
          // This is in the shape we are looking for:
          //
          //  (ref.test $A.vtable
          //    (struct.get $X $vtable
          //      (REF)
          //    )
          //  )
          //
          // See if we have a valid type there to try to optimize with.
          if (!get->ref->type.isRef()) {
            return;
          }
          auto heapType = get->ref->type.getHeapType();
          if (!heapType.isStruct()) {
            return;
          }

          // The location the get reads from must be in |infoMap| as we
          // computed values for all possible locations ahead of time. Each
          // entry in infoMap must exist, and the sub-map must exist as well
          // (but it may be of size zero, if we failed to optimize).
          auto getLoc = DataLocation{heapType, get->index};
          //std::cerr << "getLoc for " << getModule()->typeNames[heapType].name << ":" << get->index << '\n';
          auto iter = infoMap.find(getLoc);
          assert(iter != infoMap.end());
          auto& maybeSubMap = iter->second;
          assert(maybeSubMap);
          auto& subMap = *maybeSubMap;

          // If the sub-map has an entry for us, then we can optimize. (Note
          // that that handles the case of us unable to optimize anything for
          // this field, in which case the sub-map is empty of all entries.)
          auto subIter = subMap.find(curr->castType.getHeapType());
          if (subIter == subMap.end()) {
            return;
          }

          // We can optimize! We are testing on something like a vtable, and can
          // instead test on the object, since the type hierarchies are
          // parallel. And the object type is exactly what is in the map. Switch
          // to that, and skip the struct.get.
          // TODO: add RefAsNonNull
          curr->castType = Type(subIter->second, curr->castType.getNullability());
          curr->ref = get->ref;
        }
      }
    } optimizer(infoMap);
    optimizer.walkModule(module);
  }
};

} // anonymous namespace

Pass* createGUFAPass() { return new GUFAPass(false, false); }
Pass* createGUFAOptimizingPass() { return new GUFAPass(true, false); }
Pass* createGUFACastAllPass() { return new GUFAPass(false, true); }

} // namespace wasm
