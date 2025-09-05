/*
 * Copyright 2025 WebAssembly Community Group participants
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

#ifndef wasm_ir_locations_h
#define wasm_ir_locations_h

#include <variant>

#include "support/hash.h"
#include "wasm-type.h"
#include "wasm.h"

namespace wasm {

// The various *Location structs (ExpressionLocation, ResultLocation, etc.)
// describe particular locations where content can appear.

// The location of a specific IR expression.
struct ExpressionLocation {
  Expression* expr;
  // If this expression contains a tuple then each index in the tuple will have
  // its own location with a corresponding tupleIndex. If this is not a tuple
  // then we only use tupleIndex 0.
  Index tupleIndex;
  bool operator==(const ExpressionLocation& other) const {
    return expr == other.expr && tupleIndex == other.tupleIndex;
  }
};

// The location of one of the parameters of a function.
struct ParamLocation {
  Function* func;
  Index index;
  bool operator==(const ParamLocation& other) const {
    return func == other.func && index == other.index;
  }
};

// The location of a value in a local.
struct LocalLocation {
  Function* func;
  Index index;
  bool operator==(const LocalLocation& other) const {
    return func == other.func && index == other.index;
  }
};

// The location of one of the results of a function.
struct ResultLocation {
  Function* func;
  Index index;
  bool operator==(const ResultLocation& other) const {
    return func == other.func && index == other.index;
  }
};

// The location of a global in the module.
struct GlobalLocation {
  Name name;
  bool operator==(const GlobalLocation& other) const {
    return name == other.name;
  }
};

// The location of one of the parameters in a function signature.
struct SignatureParamLocation {
  HeapType type;
  Index index;
  bool operator==(const SignatureParamLocation& other) const {
    return type == other.type && index == other.index;
  }
};

// The location of one of the results in a function signature.
struct SignatureResultLocation {
  HeapType type;
  Index index;
  bool operator==(const SignatureResultLocation& other) const {
    return type == other.type && index == other.index;
  }
};

// The location of contents in a struct or array (i.e., things that can fit in a
// dataref). Note that this is specific to this type - it does not include data
// about subtypes or supertypes.
//
// We store the truncated bits here when the field is packed. That is, if -1 is
// written to an i8 then the value here will be 0xff. StructGet/ArrayGet
// operations that read a signed value must then perform a sign-extend
// operation.
struct DataLocation {
  HeapType type;
  // The index of the field in a struct, or 0 for an array (where we do not
  // attempt to differentiate by index). A special index is used for the
  // descriptor field.
  static const Index ArrayIndex = 0;
  static const Index DescriptorIndex = -1;
  Index index;
  bool operator==(const DataLocation& other) const {
    return type == other.type && index == other.index;
  }
};

// The location of anything written to a particular tag.
struct TagLocation {
  Name tag;
  // If the tag has more than one element, we'll have a separate TagLocation for
  // each, with corresponding indexes. If the tag has just one element we'll
  // only have one TagLocation with index 0.
  Index tupleIndex;
  bool operator==(const TagLocation& other) const {
    return tag == other.tag && tupleIndex == other.tupleIndex;
  }
};

// A null value of a particular type. For example, a nullable local reads from
// the corresponding NullLocation, for its default value.
struct NullLocation {
  Type type;
  bool operator==(const NullLocation& other) const {
    return type == other.type;
  }
};

// A location that contains anything of a particular type. This is used as a
// root of the graph, a source of values we will never learn anything about, and
// must assume they can be anything of that type (e.g., the return of a call to
// an import).
struct TypeLocation {
  Type type;
  bool operator==(const TypeLocation& other) const {
    return type == other.type;
  }
};

// A special type of location that does not refer to something concrete in the
// wasm, but is used to optimize the graph. A "cone read" is a struct.get or
// array.get of a type that is not exact, so it can read from either that type
// of some of the subtypes (up to a particular subtype depth).
//
// In general a read of a cone type + depth (as opposed to an exact type) will
// require N incoming links, from each of the N subtypes - and we need that
// for each struct.get of a cone. If there are M such gets then we have N * M
// edges for this. Instead, we make a single canonical "cone read" location, and
// add a single link to it from each get, which is only N + M (plus the cost
// of adding "latency" in requiring an additional step along the way for the
// data to flow along).
struct ConeReadLocation {
  HeapType type;
  // As in PossibleContents, this represents the how deep we go with subtypes.
  // 0 means an exact type, 1 means immediate subtypes, etc. (Note that 0 is not
  // needed since that is what DataLocation already is.)
  Index depth;
  // The index of the field in a struct, or 0 for an array (where we do not
  // attempt to differentiate by index).
  Index index;
  bool operator==(const ConeReadLocation& other) const {
    return type == other.type && depth == other.depth && index == other.index;
  }
};

// A location is a variant over all the possible flavors of locations that we
// have.
using Location = std::variant<ExpressionLocation,
                              ParamLocation,
                              LocalLocation,
                              ResultLocation,
                              GlobalLocation,
                              SignatureParamLocation,
                              SignatureResultLocation,
                              DataLocation,
                              TagLocation,
                              NullLocation,
                              TypeLocation,
                              ConeReadLocation>;

// A link between two locations.
struct LocationLink {
  Location from;
  Location to;

  LocationLink(Location from, Location to) : from(from), to(to) {}

  bool operator==(const LocationLink& other) const {
    return from == other.from && to == other.to;
  }
};

// Utility for printing a location with module info (like type names);
using ModuleLocation = std::pair<Module*, Location>;

} // namespace wasm

namespace std {

// Define hashes of all the *Location flavors so that Location itself is
// hashable and we can use it in unordered maps and sets.

template<> struct hash<wasm::ExpressionLocation> {
  size_t operator()(const wasm::ExpressionLocation& loc) const {
    return std::hash<std::pair<size_t, wasm::Index>>{}(
      {size_t(loc.expr), loc.tupleIndex});
  }
};

template<> struct hash<wasm::ParamLocation> {
  size_t operator()(const wasm::ParamLocation& loc) const {
    return std::hash<std::pair<size_t, wasm::Index>>{}(
      {size_t(loc.func), loc.index});
  }
};

template<> struct hash<wasm::LocalLocation> {
  size_t operator()(const wasm::LocalLocation& loc) const {
    return std::hash<std::pair<size_t, wasm::Index>>{}(
      {size_t(loc.func), loc.index});
  }
};

template<> struct hash<wasm::ResultLocation> {
  size_t operator()(const wasm::ResultLocation& loc) const {
    return std::hash<std::pair<size_t, wasm::Index>>{}(
      {size_t(loc.func), loc.index});
  }
};

template<> struct hash<wasm::GlobalLocation> {
  size_t operator()(const wasm::GlobalLocation& loc) const {
    return std::hash<wasm::Name>{}(loc.name);
  }
};

template<> struct hash<wasm::SignatureParamLocation> {
  size_t operator()(const wasm::SignatureParamLocation& loc) const {
    return std::hash<std::pair<wasm::HeapType, wasm::Index>>{}(
      {loc.type, loc.index});
  }
};

template<> struct hash<wasm::SignatureResultLocation> {
  size_t operator()(const wasm::SignatureResultLocation& loc) const {
    return std::hash<std::pair<wasm::HeapType, wasm::Index>>{}(
      {loc.type, loc.index});
  }
};

template<> struct hash<wasm::DataLocation> {
  size_t operator()(const wasm::DataLocation& loc) const {
    return std::hash<std::pair<wasm::HeapType, wasm::Index>>{}(
      {loc.type, loc.index});
  }
};

template<> struct hash<wasm::TagLocation> {
  size_t operator()(const wasm::TagLocation& loc) const {
    return std::hash<std::pair<wasm::Name, wasm::Index>>{}(
      {loc.tag, loc.tupleIndex});
  }
};

template<> struct hash<wasm::NullLocation> {
  size_t operator()(const wasm::NullLocation& loc) const {
    return std::hash<wasm::Type>{}(loc.type);
  }
};

template<> struct hash<wasm::TypeLocation> {
  size_t operator()(const wasm::TypeLocation& loc) const {
    return std::hash<wasm::Type>{}(loc.type);
  }
};

template<> struct hash<wasm::ConeReadLocation> {
  size_t operator()(const wasm::ConeReadLocation& loc) const {
    return std::hash<std::tuple<wasm::HeapType, wasm::Index, wasm::Index>>{}(
      {loc.type, loc.depth, loc.index});
  }
};

template<> struct hash<wasm::LocationLink> {
  size_t operator()(const wasm::LocationLink& loc) const {
    return std::hash<std::pair<wasm::Location, wasm::Location>>{}(
      {loc.from, loc.to});
  }
};

} // namespace std

namespace wasm {

// A graph of links.
struct LocationLinkGraph : public std::unordered_set<LocationLink> {
  // Given a graph, fill it out. The input graph contains links of interest, and
  // we fill out "boilerplate" links. For example, if the graph has an
  // ExpressionLocation of a global.get, then we add a link from the
  // corresponding GlobalLocation to that expression (since the expression reads
  // from there).
  void fill(Module& wasm);
};

} // namespace wasm

#endif // wasm_ir_locations_h
