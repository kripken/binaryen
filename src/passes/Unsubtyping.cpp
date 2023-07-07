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
//

#include "ir/find_all.h"
#include "ir/gc-type-utils.h"
#include "ir/lubs.h"
#include "ir/possible-contents.h"
#include "pass.h"
#include "wasm-type.h"
#include "wasm.h"

namespace wasm {

namespace {

struct Unsubtyping : public Pass {
  // Only modifies types.
  bool requiresNonNullableLocalFixups() override { return false; }

  // Maps each heap type to the LUB of the subtypes of the places it is written
  // to.
  std::unordered_map<HeapType, HeapLUBFinder> typeLUBs;

  Module* module;

  void run(Module* module_) override {
    module = module_;

    if (!module->features.hasGC()) {
      return;
    }
std::cout << "wark1\n";
    // Build the graph of all places where content can flow. Using this graph we
    // can optimize according to the simple rule of modifying the parent of a
    // type to be the least-refined type that the type can be written to. That
    // is, if we have A :> B :> C and
    //
    //  B* bloc1 = new B(); // a B is written to a place of type B
    //  A* aloc1 = new C(); // A C is written to a place of type A
    //
    // If we have no other uses of these types, then B does not need any
    // (non-basic) supertype, as it is not written to any location of type A.
    // C, on the other hand, is written to a location of type A, so's supertype
    // can be A and not C, giving us A :> C, skipping B in the middle. If we
    // also had
    //
    //  B* bloc2 = new C(); // A C is written to a place of type B
    //
    // when we'd need to have A :> B :> C, that is, we need to keep using the
    // entire original subtyping tree, including the connection between B and A,
    // simply for C's benefit, since C is written to locations of both types A
    // and B.
    PossibleContentsGraph graph(*module);

    for (Index index = 0; index < graph.locations.size(); index++) {
std::cout << " wark2\n";
      // Get the location for this index, and the targets it sends data to.
      auto& sourceInfo = graph.locations[index];
      auto& source = sourceInfo.location;

      auto sourceType = getLocationType(source);
      if (!sourceType.isRef()) {
        continue;
      }

      auto& targets = sourceInfo.targets;

      for (auto targetIndex : targets) {
        auto target = graph.getLocation(targetIndex);
        auto targetType = getLocationType(target);
        if (!targetType.isRef()) {
          continue;
        }
std::cout << "  wark3\n";

        // We have a connection where data goes from source => target, which
        // means we are writing something of type sourceType into a slot of
        // type targetType.
        noteWrite(sourceType.getHeapType(), targetType.getHeapType());
      }

      // TODO: childParent stuff
    }

    for (auto& [k, v] : typeLUBs) {
      std::cout << k << " has lub " << v.getLUB() << '\n';
    }
  }

  // Note that we write something of sourceType into a slot of type targetType.
  void noteWrite(HeapType sourceType, HeapType targetType) {
std::cout << "note source " << sourceType << " into " << targetType << '\n';
    if (sourceType == targetType) {
      // Writing the same type has no effect.
      return;
    }

// XXX locals don;t work because we use localgraph!!1
    typeLUBs[sourceType].note(targetType);
  }

  // Get the type of a location. This is the type of the location itself, and
  // not the content that might be present there (which could be more precise,
  // or could be nonexistent).
  // TODO: move to possible-contents.h?
  Type getLocationType(Location location) {
    if (auto* loc = std::get_if<ExpressionLocation>(&location)) {
      return loc->expr->type;
    } else if (auto* loc = std::get_if<DataLocation>(&location)) {
      return GCTypeUtils::getField(loc->type, loc->index)->type;
    } else if (auto* loc = std::get_if<TagLocation>(&location)) {
      return module->getTag(loc->tag)->sig.params[loc->tupleIndex];
    } else if (auto* loc = std::get_if<ParamLocation>(&location)) {
      return loc->func->type.getSignature().params[loc->index];
    } else if (auto* loc = std::get_if<ResultLocation>(&location)) {
      return loc->func->type.getSignature().results[loc->index];
    } else if (auto* loc = std::get_if<GlobalLocation>(&location)) {
      return module->getGlobal(loc->name)->type;
    } else if (auto* loc = std::get_if<BreakTargetLocation>(&location)) {
      auto iter = functionBlockTypes.find(loc->func);
      if (iter == functionBlockTypes.end()) {
        iter = functionBlockTypes.emplace(loc->func, NameTypeMap()).first;
        auto& map = iter->second;
        for (auto* block : FindAll<Block>(loc->func->body).list) {
          if (block->name.is()) {
            map[block->name] = block->type;
          }
        }
      }
      auto& map = iter->second;
      return map[loc->target];
    } else if (auto* loc = std::get_if<SignatureParamLocation>(&location)) {
      return loc->type.getSignature().params[loc->index];
    } else if (auto* loc = std::get_if<SignatureResultLocation>(&location)) {
      return loc->type.getSignature().results[loc->index];
    } else if (auto* loc = std::get_if<NullLocation>(&location)) {
      return loc->type;
    } else {
      WASM_UNREACHABLE("bad loc");
    }
  }

  // Map functions to maps of their block names to block types.
  using NameTypeMap = std::unordered_map<Name, Type>;
  std::unordered_map<Function*, NameTypeMap> functionBlockTypes;
};

} // anonymous namespace

Pass* createUnsubtypingPass() { return new Unsubtyping(); }

} // namespace wasm
