
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

console.log(`fuzz_loop: loading binaryen_wasm.js (${binaryen_wasm_js_path})`);
import(binaryen_wasm_js_path).then((imported) => {
  return imported.default();
}).then((binaryen) => {
  console.log('fuzz loop: fuzzing forever');
});

