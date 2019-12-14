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

static void updateDebugLines(const Module& wasm, llvm::DWARFYAML::Data& data, const BinaryLocationsMap& newLocations) {
  // TODO: for memory efficiency, we may want to do this in a streaming manner,
  //       binary to binary, without YAML IR.

  struct LineState {
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

    LineState(const llvm::DWARFYAML::LineTable& table) : isStmt(table.DefaultIsStmt) {}
  };

  for (auto& table : data.DebugLines) {
    // Parse the original opcodes and emit new ones.
    LineState state(table);
    std::vector<llvm::DWARFYAML::LineTableOpcode> newOpcodes;
    for (auto& opcode : table.Opcodes) {
      LineState oldState(state);
      switch (opcode.Opcode) {
        case 0: {
          // Extended opcodes
          switch (opcode.SubOpcode) {
            case DW_LNE_set_address: {
              state.addr = opcode.Data;
              break;
            }
            default: {
              Fatal() << "unknown debug line sub-opcode: " << std::hex << opcode.SubOpcode;
            }
          }
          break;
        }
        case DW_LNS_set_column: {
          state.col = opcode.Data;
          break;
        }
        case DW_LNS_set_prologue_end: {
          state.prologueEnd = true;
          break;
        }
        case DW_LNS_advance_pc: {
          state.addr += opcode.Data; // XXX
          break;
        }
        default: {
          if (opcode.Opcode >= table.OpcodeBase) {
            // Special opcode: adjust line and addr using some math.
            uint8_t AdjustOpcode = opcode.Opcode - table.OpcodeBase;
            uint64_t AddrOffset =
                (AdjustOpcode / table.LineRange) * table.MinInstLength;
            int32_t LineOffset =
                table.LineBase + (AdjustOpcode % table.LineRange);
            state.line += LineOffset;
            state.addr += AddrOffset;
          } else {
            Fatal() << "unknown debug line opcode: " << std::hex << opcode.Opcode;
          }
        }
      }

      // TODO: apply the wasm new locations here..! i.e. find the wasm instr
      //       this referred to, then find its new location, and that is the
      //       actual new state.

      // Add the diff between the old state and the new state.
      if (state.line != oldState.line && state.addr != oldState.addr) {
        // Try to use a special opcode TODO
        // if we succeed remove that diff so the next ifs fail to hit
      }
      if (state.line != oldState.line) {
      }
      if (state.addr != oldState.addr) {
      }
      if (state.col != oldState.col) {
      }
      if (state.prologueEnd != oldState.prologueEnd) {
      }

      //newOpcodes.push_back(opcode);
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
