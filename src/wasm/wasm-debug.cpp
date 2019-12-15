/*
 * Copyright 2019 WebAssembly Community Group participants
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

#include "wasm-debug.h"
#include "wasm.h"

#ifdef USE_LLVM_DWARF
#include "thirdparty/llvm/ObjectYAML/DWARFEmitter.h"
#include "thirdparty/llvm/ObjectYAML/DWARFYAML.h"
#include "thirdparty/llvm/include/llvm/DebugInfo/DWARFContext.h"

std::error_code dwarf2yaml(llvm::DWARFContext& DCtx, llvm::DWARFYAML::Data& Y);
#endif

namespace wasm {

namespace Debug {

bool isDWARFSection(Name name) { return name.startsWith(".debug_"); }

bool hasDWARFSections(const Module& wasm) {
  for (auto& section : wasm.userSections) {
    if (isDWARFSection(section.name)) {
      return true;
    }
  }
  return false;
}

#ifdef USE_LLVM_DWARF

struct BinaryenDWARFInfo {
  llvm::StringMap<std::unique_ptr<llvm::MemoryBuffer>> sections;
  std::unique_ptr<llvm::DWARFContext> context;

  BinaryenDWARFInfo(const Module& wasm) {
    // Get debug sections from the wasm.
    for (auto& section : wasm.userSections) {
      if (Name(section.name).startsWith(".debug_")) {
        // TODO: efficiency
        sections[section.name.substr(1)] = llvm::MemoryBuffer::getMemBufferCopy(
          llvm::StringRef(section.data.data(), section.data.size()));
      }
    }
    // Parse debug sections.
    uint8_t addrSize = 4;
    context = llvm::DWARFContext::create(sections, addrSize);
  }
};

void dumpDWARF(const Module& wasm) {
  BinaryenDWARFInfo info(wasm);
  std::cout << "DWARF debug info\n";
  std::cout << "================\n\n";
  for (auto& section : wasm.userSections) {
    if (Name(section.name).startsWith(".debug_")) {
      std::cout << "Contains section " << section.name << " ("
                << section.data.size() << " bytes)\n";
    }
  }
  llvm::DIDumpOptions options;
  options.Verbose = true;
  info.context->dump(llvm::outs(), options);
}

//
// Big picture: We use a DWARFContext to read data, then DWARFYAML support
// code to write it. That is not the main LLVM Dwarf code used for writing
// object files, but it avoids us create a "fake" MC layer, and provides a
// simple way to write out the debug info. Likely the level of info represented
// in the DWARFYAML::Data object is sufficient for Binaryen's needs, but if not,
// we may need a different approach.
//
// In more detail:
//
// 1. Binary sections => DWARFContext:
//
//     llvm::DWARFContext::create(sections..)
//
// 2. DWARFContext => DWARFYAML::Data
//
//     std::error_code dwarf2yaml(DWARFContext &DCtx, DWARFYAML::Data &Y) {
//
// 3. DWARFYAML::Data => binary sections
//
//     StringMap<std::unique_ptr<MemoryBuffer>>
//     EmitDebugSections(llvm::DWARFYAML::Data &DI, bool ApplyFixups);
//

// Represents the state when parsing a line table.
struct LineState {
  const llvm::DWARFYAML::LineTable& table;

  uint32_t addr = 0;
  // TODO sectionIndex?
  uint32_t line = 1;
  uint32_t col = 0;
  // TODO uint32_t file = 1;
  // TODO uint32_t isa = 0;
  // TODO Discriminator = 0;
  bool isStmt;
  bool basicBlock = false;
  bool endSequence = false;
  bool prologueEnd = false;
  bool epilogueBegin = false;

  LineState(const llvm::DWARFYAML::LineTable& table) : table(table), isStmt(table.DefaultIsStmt) {}

  void update(llvm:DWARFYAML::LineTableOpcode& opcode) {
    switch (Opcode) {
      case 0: {
        // Extended opcodes
        switch (SubOpcode) {
          case DW_LNE_set_address: {
            addr = Data;
            break;
          }
          default: {
            Fatal() << "unknown debug line sub-opcode: " << std::hex << SubOpcode;
          }
        }
        break;
      }
      case DW_LNS_set_column: {
        col = Data;
        break;
      }
      case DW_LNS_set_prologue_end: {
        prologueEnd = true;
        break;
      }
      case DW_LNS_advance_pc: {
        addr += Data; // XXX
        break;
      }
      default: {
        if (isSpecial()) {
          // Special opcode: adjust line and addr using some math.
          uint8_t AdjustOpcode = Opcode - table.OpcodeBase;
          uint64_t AddrOffset =
              (AdjustOpcode / table.LineRange) * table.MinInstLength;
          int32_t LineOffset =
              table.LineBase + (AdjustOpcode % table.LineRange);
          line += LineOffset;
          addr += AddrOffset;
        } else {
          Fatal() << "unknown debug line opcode: " << std::hex << Opcode;
        }
      }
    }
  }

  bool isSpecial() {
    return Opcode >= table.OpcodeBase;
  }

  bool addsRow() {
    return isSpecial() || endSequence;
  }

  // Given an old state, emit the diff from it to this state into a new line
  // table.
  void emitDiff(const LineState& old, std::vector<llvm::DWARFYAML::LineTableOpcode>& newOpcodes) {
    auto makeItem = [](dwarf::LineNumberOps opcode) {
      llvm::DWARFYAML::LineTableOpcode item;
      memset(&item, 0, sizeof(item));
      item.Opcode = opcode;
      return item;
    };

    bool usedSpecial = false;
    if (addr != oldState.addr || line != oldState.line) {
      // Try to use a special opcode TODO
    }
    if (addr != oldState.addr && !usedSpecial) {
      auto item = makeItem(DW_LNE_set_address);
      item.Data = addr;
      newOpcodes.push_back(item);
      // TODO file and all the other fields
    }
    if (line != oldState.line && !usedSpecial) {
      auto item = makeItem(DW_LNE_set_line);
      item.Data = line;
      newOpcodes.push_back(item);
      // TODO file and all the other fields
    }
    if (col != oldState.col && !usedSpecial) {
      auto item = makeItem(DW_LNE_set_col);
      item.Data = col;
      newOpcodes.push_back(item);
      // TODO file and all the other fields
    }
    if (isStmt != oldState.isStmt) {
      abort();
    }
    if (basicBlock != oldState.basicBlock) {
      abort();
    }
    if (endSequence != oldState.endSequence) {
      auto item = makeItem(DW_LNE_end_sequence);
      newOpcodes.push_back(item);
    }
    if (prologueEnd != oldState.prologueEnd) {
      abort();
    }
    if (epilogueBegin != oldState.epilogueBegin) {
      abort();
    }
  }
};

// Represents a mapping of addresses to expressions.
struct AddrExprMap {
  std::unordered_map<uint32_t, Expression*> map;

  // Construct the map from the binaryLocations loaded from the wasm.
  AddrExprMap(const Module& wasm) {
    for (auto& func : wasm.functions) {
      for (auto pair : func->binaryLocations) {
        assert(map.count(pair.second) == 0);
        map[pair.second] = pair.first;
      }
    }
  }

  // Construct the map from new binaryLocations just written
  AddrExprMap(const BinaryLocationsMap& newLocations) {
    for (auto pair : newLocations) {
      assert(map.count(pair.second) == 0);
      map[pair.second] = pair.first;
    }
  }

  Expression* get(uint32_t addr) {
    auto iter = map.find(addr);
    if (iter != map.end()) {
      return *iter;
    }
    return nullptr;
  }
};

static void updateDebugLines(const Module& wasm, llvm::DWARFYAML::Data& data, const BinaryLocationsMap& newLocations) {
  // TODO: for memory efficiency, we may want to do this in a streaming manner,
  //       binary to binary, without YAML IR.

  AddrExprMap oldAddrMap(wasm);
  AddrExprMap newAddrMap(newLocations);

  for (auto& table : data.DebugLines) {
    // Parse the original opcodes and emit new ones.
    LineState state(table);
    // All the addresses we need to write out.
    std::vector<uint32_t> newAddrs;
    std::unordered_map<uint32_t, LineState> newAddrInfo;
    for (auto& opcode : table.Opcodes) {
      state.update(opcode.Opcode);
      if (state.addsRow()) {
        // An expression may not exist for this line table item, if we optimized
        // it away.
        if (auto* expr = oldAddrMap.get(state.addr)) {
          auto iter = newLocations.find(expr);
          if (iter != newLocations.end()) {
            uint32_t newAddr = *iter;
            newAddrs.push_back(newAddr);
            auto& updatedState = newAddrInfo[newAddr] = state;
            updatedState.addr = newAddr;
          }
        }
      }
    }
    // Sort the new addresses (which may be substantially different from the
    // original layout after optimization).
    std::sort(newAddrs.begin(), newAddrs.end(), [](uint32_t a, uint32_t b) {
      return a < b;
    });
    // Emit a new line table.
    std::vector<llvm::DWARFYAML::LineTableOpcode> newOpcodes;
    LineState state(table);
    for (uint32_t addr : newAddrs) {
      LineState oldState(state);
      state = newAddrInfo[addr];
      state.emitDiff(oldState, newOpcodes);
    }
    table.Opcodes.swap(newOpcodes);
  }
}

void updateDWARF(Module& wasm, const BinaryLocationsMap& newLocations) {
  BinaryenDWARFInfo info(wasm);

  // Convert to Data representation, which YAML can use to write.
  llvm::DWARFYAML::Data data;
  if (dwarf2yaml(*info.context, data)) {
    Fatal() << "Failed to parse DWARF to YAML";
  }

  updateDebugLines(wasm, data, newLocations);

  // TODO: Actually update, and remove sections we don't know how to update yet?

  // Convert to binary sections.
  auto newSections = EmitDebugSections(
    data,
    true);

  // Update the custom sections in the wasm.
  // TODO: efficiency
  for (auto& section : wasm.userSections) {
    if (Name(section.name).startsWith(".debug_")) {
      auto llvmName = section.name.substr(1);
      if (newSections.count(llvmName)) {
        auto llvmData = newSections[llvmName]->getBuffer();
        section.data.resize(llvmData.size());
        std::copy(llvmData.begin(), llvmData.end(), section.data.data());
      }
    }
  }
}

#else // USE_LLVM_DWARF

void dumpDWARF(const Module& wasm) {
  std::cerr << "warning: no DWARF dumping support present\n";
}

void updateDWARF(Module& wasm, const BinaryLocationsMap& newLocations) {
  std::cerr << "warning: no DWARF updating support present\n";
}

#endif // USE_LLVM_DWARF

} // namespace Debug

} // namespace wasm
