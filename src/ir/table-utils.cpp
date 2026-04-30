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

#include "table-utils.h"
#include "effects.h"
#include "element-utils.h"
#include "find_all.h"
#include "module-utils.h"
#include "properties.h"

namespace wasm::TableUtils {

std::set<Name> getFunctionsNeedingElemDeclare(Module& wasm) {
  // Without reference types there are no ref.funcs or elem declare.
  if (!wasm.features.hasReferenceTypes()) {
    return {};
  }

  // Find all the names in the tables.

  std::unordered_set<Name> tableNames;
  ElementUtils::iterAllElementFunctionNames(
    &wasm, [&](Name name) { tableNames.insert(name); });

  // Find all the names in ref.funcs.
  using Names = std::unordered_set<Name>;

  ModuleUtils::ParallelFunctionAnalysis<Names> analysis(
    wasm, [&](Function* func, Names& names) {
      if (func->imported()) {
        return;
      }
      for (auto* refFunc : FindAll<RefFunc>(func->body).list) {
        names.insert(refFunc->func);
      }
    });

  // Find the names that need to be declared.

  std::set<Name> ret;

  for (auto& kv : analysis.map) {
    auto& names = kv.second;
    for (auto name : names) {
      if (!tableNames.contains(name)) {
        ret.insert(name);
      }
    }
  }

  return ret;
}

bool usesExpressions(ElementSegment* curr, Module* module) {
  // Binaryen IR always has ref.funcs for functions in tables for uniformity,
  // so that by itself does not indicate if expressions should be used when
  // emitting the table or not. But definitely anything that is not a ref.func
  // implies we are post-MVP and must use expressions.
  bool allElementsRefFunc =
    std::all_of(curr->data.begin(), curr->data.end(), [](Expression* entry) {
      return entry->is<RefFunc>();
    });

  // If the segment has a specialized (non-MVP) type, then it must use the
  // post-MVP form of using expressions.
  bool hasSpecializedType = curr->type != Type(HeapType::func, Nullable);

  return !allElementsRefFunc || hasSpecializedType;
}

TableInfoMap computeTableInfo(Module& wasm,
                              const PassOptions& options,
                              bool initialContentsImmutable) {
  // Set up the initial info.
  TableInfoMap tables;
  if (wasm.tables.empty()) {
    return tables;
  }
  for (auto& table : wasm.tables) {
    tables[table->name].initialContentsImmutable = initialContentsImmutable;
    tables[table->name].flatTable =
      std::make_unique<TableUtils::FlatTable>(wasm, *table);
  }

  // Next, look at the imports and exports.

  for (auto& table : wasm.tables) {
    if (table->imported()) {
      tables[table->name].mayBeModified = true;
    }
  }

  for (auto& ex : wasm.exports) {
    if (ex->kind == ExternalKind::Table) {
      tables[*ex->getInternalName()].mayBeModified = true;
    }
  }

  // Find which tables have sets, by scanning for instructions. Only do so if we
  // might learn anything new.
  auto hasUnmodifiableTable = false;
  for (auto& [_, info] : tables) {
    if (!info.mayBeModified) {
      hasUnmodifiableTable = true;
      break;
    }
  }
  if (!hasUnmodifiableTable) {
    return tables;
  }

  using TablesWithSet = std::unordered_set<Name>;

  struct Finder : public PostWalker<Finder> {
    TablesWithSet& tablesWithSet;

    Finder(TablesWithSet& tablesWithSet) : tablesWithSet(tablesWithSet) {}

    void visitTableSet(TableSet* curr) { tablesWithSet.insert(curr->table); }
    void visitTableFill(TableFill* curr) { tablesWithSet.insert(curr->table); }
    void visitTableCopy(TableCopy* curr) {
      tablesWithSet.insert(curr->destTable);
    }
    void visitTableInit(TableInit* curr) { tablesWithSet.insert(curr->table); }
  };

  // Scan the start function separately: we can handle fixed offsets in the
  // start function, if it only writes constant offsets and known values, then
  // we can apply those to `tables.flatTable`, i.e., those work just like
  // constant elements. (Note: this works even if start is called more than
  // once, as it would just re-apply the same constant values.) Such constant-
  // offset operations can appear after linking modules, or can be a form of
  // compression (replace a huge segment with a table.fill).
  ModuleUtils::ParallelFunctionAnalysis<TablesWithSet> analysis(
    wasm, [&](Function* func, TablesWithSet& tablesWithSet) {
      if (func->imported() || func->name == wasm.start) {
        return;
      }

      Finder(tablesWithSet).walk(func->body);
    });

  for (auto& [_, names] : analysis.map) {
    for (auto name : names) {
      tables[name].mayBeModified = true;
    }
  }

  if (wasm.start) {
    // Scan the start function in detail, applying constant operations to the
    // flatTable. We go in order, and stop at the first operation that we cannot
    // reason about.

    // Tables we see non-constant operations in.
    TablesWithSet nonConstant;

    // Handle a possibly-constant table write. If the size is not given, it is
    // assumed to be 1 (which is the case for a set). Returns true if we
    // handled this successfuly, i.e., is is constant.
    auto handle = [&](Name table,
                      Expression* index,
                      Expression* value,
                      Expression* size = nullptr) {
      if (!Properties::isConstantExpression(index) ||
          !Properties::isConstantExpression(value) ||
          (size && !Properties::isConstantExpression(size))) {
        return false;
      }

      auto constantIndex = Properties::getLiteral(index).getUnsigned();
      auto constantValue = Properties::getLiteral(value);
      if (!constantValue.isFunction()) {
        // FlatTable only stores function names. TODO optimize more?
        return false;
      }
      auto constantFunc = constantValue.getFunc();
      auto constantSize =
        size ? Properties::getLiteral(index).getUnsigned() : 1ULL;

      auto& flatTable = tables[table].flatTable;
      if (!flatTable->ensureSpace(
            *wasm.getTable(table), constantIndex, constantSize)) {
        // Sizes overflowed.
        return false;
      }

      // Apply the value.
      auto& flatTableNames = flatTable->names;
      for (Index i = 0; i < constantSize; i++) {
        flatTableNames[constantSize + i] = constantFunc;
      }

      return true;
    };

    // Whether we've given up on analysis, because we saw something we can't
    // handle. In that case we process everything else pessimistically.
    auto giveUp = false;

    // Process the body as a block for simplicity, which handles most cases.
    Builder builder(wasm);
    auto* block = Builder(wasm).blockify(wasm.getFunction(wasm.start)->body);
    Index i = 0;
    for (; i < block->list.size(); i++) {
      auto* curr = block->list[i];
      EffectAnalyzer effects(options, wasm, curr);
      if (effects.calls || effects.transfersControlFlow() ||
          curr->type == Type::unreachable) {
        // Either arbitrary code can run here, or we may skip code after us,
        // both of which break our ability to apply changes to flatTable.
        giveUp = true;
        break;
      }
      if (!effects.writesTable) {
        // Nothing to do, proceed onward.
        continue;
      }

      if (auto* set = curr->dynCast<TableSet>()) {
        if (handle(set->table, set->index, set->value)) {
          continue;
        }
      } else if (auto* fill = curr->dynCast<TableFill>()) {
        if (handle(fill->table, fill->dest, fill->value, fill->size)) {
          continue;
        }
      }
      // Things we did not handle are non-constant. TODO: handle constant copy
      // etc.
      Finder(nonConstant).walk(curr);
    }

    if (giveUp) {
      // Handle everything else pessimistically.
      for (; i < block->list.size(); i++) {
        Finder(nonConstant).walk(block->list[i]);
      }
    }

    // Apply the non-constant things.
    for (auto name : nonConstant) {
      tables[name].mayBeModified = true;
    }
  }

  return tables;
}

} // namespace wasm::TableUtils
