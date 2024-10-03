
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
  while (1) {
    const size = pickRandomSize();
    const bytes = makeBytes(size);
    console.log(`ITERATION ${iter} size: ${size}`);
    iter++;
    const module = makeModule(bytes);
    testModule(module);
    module.dispose();
  }
}

function pickRandomSize() {
  const MIN = 1024;
  const MAX = 5 * 40 * 1024;
  return MIN + Math.random() * (MAX - MIN);
}

function makeBytes(size) {
  const bytes = new Uint8Array(size);
  for (let i = 0; i < size; i++) {
    bytes[i] = Math.random() * 256;
  }
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

function testModule(module) {
}

