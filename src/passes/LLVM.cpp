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
// Run LLVM to optimize the wasm.
//

#include "ir/module-utils.h"
#include "pass.h"
#include "support/process.h"
#include "support/string.h"
#include "wasm-builder.h"
#include "wasm-io.h"
#include "wasm.h"

namespace wasm {

struct LLVMOpt : public Pass {
  void run(PassRunner* runner, Module* module) override {
    std::string base = runner->options.getArgument(
      "llvm-opt",
      "LLVMOpt usage:  wasm-opt --llvm-opt=BASENAME\n"
      "  BASENAME will be used as the base name for the temporary files that\n"
      "  we create (BASENAME.1.wasm, BASENAME.2.c, etc.");

    std::cout << "[LLVMOpt] preprocessing...\n";

    // Save features, which would not otherwise make it through a round trip if
    // the target features section has been stripped.
    auto features = module->features;

    // Note the original exports, which are the only things we will want to have
    // exported at the end.
    std::set<Name> originalExports;
    for (auto& e : module->exports) {
      originalExports.insert(e->name);
    }
    // We will add additional exports, some of which we can forget about
    // imediately, but others we need to keep around until almost the very end
    // because we need to use them.
    // For example, we add an export for the start function if there is one, and
    // we find it in the output wasm and use it to find the start function. We
    // can then remove the export
    std::set<Name> usedExports;

    // Add exports for more things that we need, either for the wasm2c runtime,
    // or to make it easy for us to find what we need afterwards. (We will
    // remove all non-original exports at the end anyhow.)
    Builder builder(*module);

    // Ensure there is a memory.
    module->memory.exists = true;
    // Ensure the memory is exported, as the wasm2c runtime uses it
    if (!module->getExportOrNull("memory")) {
      module->addExport(
        builder.makeExport("memory", "0", ExternalKind::Memory));
    }
    // Ensure a _start is exported, which wasm2c expects.
    if (!module->getExportOrNull("_start")) {
      Name name("byn_llvm_runtime_start");
      module->addFunction(builder.makeFunction(
        name, {Type::none, Type::none}, {}, builder.makeNop()));
      module->addExport(
        builder.makeExport("_start", name, ExternalKind::Function));
    }
    // Export the wasm start function, if there is one.
    Name wasmStartExport("byn_llvm_wasm_start");
    if (module->start.is()) {
      module->addExport(
        builder.makeExport(wasmStartExport, module->start, ExternalKind::Function));
      usedExports.insert(wasmStartExport);
    }

    // Write the module to a temp file.
    std::cout << "[LLVMOpt] writing wasm...\n";
    std::string tempWasmA = base + ".1.wasm";
    ModuleWriter().writeBinary(*module, tempWasmA);
    // Compile the wasm to C, which we do by "compiling" it to wasm + wasm2c. By
    // running emcc with --post-link, the input wasm file is left as is, and we
    // only do the wasm2c bit.
    std::cout << "[LLVMOpt] wasm => C...\n";
    std::string tempWasmB = base + ".2.wasm";
    std::string tempC = tempWasmB + ".c";
    ProgramResult wasm2c(
      "emcc " + tempWasmA + " -o " + tempWasmB +
      " -s WASM2C --post-link -s ASSERTIONS=0 -s ERROR_ON_UNDEFINED_SYMBOLS=0");
    if (wasm2c.failed()) {
      wasm2c.dump(std::cout);
      Fatal() << "LLVMOpt: failed to convert to C";
    }
    // Modify the C.
    std::cout << "[LLVMOpt] rewrite C...\n";
    auto cCode(read_file<std::string>(tempC, Flags::Text));
    // Remove "static" as we want direct access to the wasm exports actually.
    String::replaceAll(cCode, "static ", "");
    // Remove extra sandboxing: we are compiling back to wasm!
    String::replaceAll(cCode, "FUNC_PROLOGUE;", "");
    String::replaceAll(cCode, "FUNC_EPILOGUE;", "");
    Output(tempC, Flags::Text).getStream() << cCode;
    // Compile the C to wasm.
    std::cout << "[LLVMOpt] C => LLVM optimizer => wasm...\n";
    std::string tempWasmC = base + ".3.wasm";
    std::string cmd =
      "emcc " + tempC + " -o " + tempWasmC + " -O1 -s EXPORTED_FUNCTIONS=";
    bool first = true;
    auto addExports = [&](const std::set<Name>& names) {
      for (auto e : names) {
        if (first) {
          first = false;
        } else {
          cmd += ',';
        }
        cmd += std::string("_w2c_") + e.str;
      }
    };
    addExports(originalExports);
    addExports(usedExports);
    ProgramResult c2wasm(cmd);
    if (c2wasm.failed()) {
      c2wasm.dump(std::cout);
      Fatal() << "LLVMOpt: failed to convert to wasm";
    }
    // Clear the module in preparation for reading, and read it.
    ModuleUtils::clearModule(*module);
    std::cout << "[LLVMOpt] reading wasm...\n";
    ModuleReader().readBinary(tempWasmC, *module);
    // Filter out any new exports, and rename the existing ones.
    module->exports.erase(std::remove_if(module->exports.begin(),
                                         module->exports.end(),
                                         [&](const std::unique_ptr<Export>& e) {
                                           // The exports we want to erase are
                                           // all those that are not one of our
                                           // original exports, whose name is
                                           // now "w2c_${ORIGINAL_NAME}"
                                           if (!e->name.startsWith("w2c_")) {
                                             return true;
                                           }
                                           e->name = e->name.str + 4;
                                           return false;
                                         }),
                          module->exports.end());
    module->updateMaps();
    // Find the important things we exported so that we could find them later.
    if (auto* e = module->getExportOrNull(wasmStartExport)) {
      module->start = e->value;
    }
    // Remove the table: the "native" table contains things the new sandboxing
    // layer in C added and needs. We want to look into the wasm in that
    // sandbox.
    // TODO: find the sandboxed table items (export them first)
    module->tables.clear();
    module->elementSegments.clear();
    module->updateMaps();
    // Do a cleanup (we may optimize anyhow, though?)
    {
      std::cout << "[LLVMOpt] postprocessing...\n";
      PassRunner postRunner(runner);
      postRunner.add("remove-unused-module-elements");
      postRunner.setIsNested(true);
      postRunner.run();
    }
    // Reapply features
    module->features = features;
  }
};

Pass* createLLVMOptPass() { return new LLVMOpt(); }

} // namespace wasm
