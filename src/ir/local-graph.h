/*
 * Copyright 2017 WebAssembly Community Group participants
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

#ifndef wasm_ir_local_graph_h
#define wasm_ir_local_graph_h

#include "ir/find_all.h"
#include "wasm.h"

namespace wasm {

//
// Generic use-def analysis. Types are provided for the use and def, and hooks
// for checking if something is a use or a def (which allows more specific
// filtering not based on the type. There is also a numerical "lane" which is a
// number that determines if two uses interfere with each other - if the lane is
// identical, they do (for example, the most common use-def analysis is on
// locals, and the "lane" is the local index, which means that two sets of the
// same local index interfere with each other as expected).
//
// The space of lanes should be a range [0..numLanes). The analysis will create
// dense vectors on that range, so only reasonable numbers of lanes should be
// used.
//
struct UseDefAnalysis {
  // Virtual methods to be implemented by users.
  
  // Check if an expression is a use.
  virtual bool isUse(Expression*) = 0;

  // Check if an expression is a def.
  virtual bool isDef(Expression*) = 0;

  // For a use or a def, return the "lane".
  virtual Index getLane(Expression*) = 0;

  // Return the total number of lanes.
  virtual Index getNumLanes() = 0;

  // Note a use-def pair that was discovered in the analysis. This is called as
  // the analysis runs.
  virtual void noteUseDef(Expression*, Expression*) = 0;

  // Main API.

  // Perform the analysis.
  void analyze(Function* func);

  // Maps expressions to the pointers to them (for easy replacing). It is
  // efficiend and convenient to collect these while performing the analysis, so
  // it is better to avoid uses of the class needing to do the work later.
  using Locations = std::map<Expression*, Expression**>;

  Locations locations;

  virtual ~UseDefAnalysis() {}
};

//
// Finds the connections between local.gets and local.sets, creating
// a graph of those ties. This is useful for "ssa-style" optimization,
// in which you want to know exactly which sets are relevant for a
// a get, so it is as if each get has just one set, logically speaking
// (see the SSA pass for actually creating new local indexes based
// on this).
//
// This builds on UseDefAnalysis, done on LocalGet and LocalSet. A specific
// convention used here is that a nullptr LocalSet means the initial value in
// the function (0 for a var, the received value for a param).
//
struct LocalGraph : public UseDefAnalysis {
  // main API

  LocalGraph(Function* func);

  virtual bool isUse(Expression*);
  virtual bool isDef(Expression*);
  virtual Index getLane(Expression*);
  virtual Index getNumLanes();
  virtual void noteUseDef(Expression*, Expression*);

  // TODO: go back to getsetes etc.
  using Defs = std::set<LocalSet*>;

  using UseDefs = std::map<LocalGet*, Defs>;

  // The defs for each use.
  UseDefs useDefs;

  // Optional API: compute the influence graphs between defs and uses
  // (useful for algorithms that propagate changes).

  void computeInfluences();

  // For each use, the defs whose values are influenced by that use
  std::unordered_map<LocalGet*, std::unordered_set<LocalSet*>> useInfluences;

  // For each def, the uses whose values are influenced by that def
  std::unordered_map<LocalSet*, std::unordered_set<LocalGet*>> defInfluences;

  // Optional: Compute the local indexes that are SSA, in the sense of
  //  * a single set for all the gets for that local index
  //  * the set dominates all the gets (logically implied by the former
  //  property)
  //  * no other set (aside from the zero-init)
  // The third property is not exactly standard SSA, but is useful since we are
  // not in SSA form in our IR. To see why it matters, consider these:
  //
  // x = 0 // zero init
  // [..]
  // x = 10
  // y = x + 20
  // x = 30 // !!!
  // f(y)
  //
  // The !!! line violates that property - it is another set for x, and it may
  // interfere say with replacing f(y) with f(x + 20). Instead, if we know the
  // only other possible set for x is the zero init, then things like the !!!
  // line cannot exist, and it is valid to replace f(y) with f(x + 20). (This
  // could be simpler, but in wasm the zero init always exists.)

  void computeSSAIndexes();

  bool isSSA(Index x);

private:
  Function* func;

  std::set<Index> SSAIndexes;
};

} // namespace wasm

#endif // wasm_ir_local_graph_h
