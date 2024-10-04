
// Fuzz loop: generates and fuzzes wasms in an infinite loop or until we find a
// problem.
//
// Usage:
//         d8 scripts/fuzz_loop.js [ABSOLUTE_PATH../]binaryen_wasm.js
//
// The path to binaryen_wasm.js must be absolute.

// Read the argument, and clear it, as fuzz_shell should not see any argument.
const binaryen_wasm_js_path = arguments[0];
arguments.length = 0;

console.log('fuzz_loop: loading fuzz shell');
load('scripts/fuzz_shell.js');

let binaryen;

console.log(`fuzz_loop: loading binaryen_wasm.js (${binaryen_wasm_js_path})`);
import(binaryen_wasm_js_path).then((imported) => {
  return imported.default();
}).then((binaryen_) => {
  binaryen = binaryen_;

  fuzzForever();
});

function fuzzForever() {
  console.log('fuzz loop: fuzzing forever');
  let iter = 0;
  const start = performance.now();
  let totalBytes = 0;
  while (1) {
    // Pick global settings.
    pickSettings();

    // Generate the main binary for this iteration, and test it.
    const size = pickRandomSize();
    const bytes = makeBytes(size);
    const now = performance.now();
    const module = makeModule(bytes);
    fixInitialModule(module);
    const binary = module.emitBinary();
    const output = test(module, binary);

    // Generate the optimized binary that corresponds to it, and test that.
    module.optimize();
    fixOptimizedModule(module);
    const optimizedBinary = module.emitBinary();
    const optimizedOutput = test(module, optimizedBinary);

    // Any difference in execution is a problem.
    if (output != optimizedOutput) {
      console.log('Bug! Saving output to bug.wasm, bug.opt.wasm');
      writeFile('bug.wasm', binary);
      writeFile('bug.opt.wasm', optimizedBinary);
      quit();
    }

    // Log some info.
    iter++;
    totalBytes += binary.length + optimizedBinary.length;
    const elapsedSeconds = (now - start) / 1000;
    console.log(`ITERATION ${iter} random bytes: ${size} wasm bytes: ${binary.length} speed: ${iter / elapsedSeconds} iters/sec ${totalBytes / elapsedSeconds} wasm bytes/sec`);

    // Clean up.
    module.dispose();
  }
}

function pickSettings() {
  binaryen.setDebugInfo(false); // TODO needed?
  binaryen.setOptimizeLevel(3); // TODO randomize, and passes
}

function pickRandomSize() {
  const MIN = 1024;
  const MAX = 40 * 1024;
  return Math.floor(MIN + Math.random() * (MAX - MIN));
}

function makeBytes(size) {
  const bytes = new Uint8Array(size);
  for (let i = 0; i < size; i++) {
    bytes[i] = Math.random() * 256;
  }
  return bytes;
}

function makeModule(bytes) {
  // Create an empty module and add fuzz.
  const module = new binaryen.Module();
  const allowOOB = false; // TODO: fuzz with OOB too?
  module.addFuzz(bytes, allowOOB);

  // The output must be valid.
  assert(module.validate());
  return module;
}

// Given a module (Binaryen IR) and binary bytes that correspond to it, run
// some tests on it. Returns the expected output.
function test(module, binary) {
  // TODO run in binaryen interpreter
  var lines = [];
  executeWasmBytes(binary, (line) => { lines.push(line) });
  return lines.join('\n');
}

function fixInitialModule(module) {
  // Remove NaNs, which can cause differences between binaryen and V8.
  // TODO: Compare V8 to itself, without nans? (but without binaryen opts too)
  //
  // Legalize the module so we can call it from JS without issue.
  module.runPasses(['denan', 'legalize-and-prune-js-interface']);
}

function fixOptimizedModule(module) {
  // TODO?
}

