
// Fuzz loop: generates and fuzzes wasms in an infinite loop or until we find a
// problem.
//
// Usage:
//         d8 scripts/fuzz_loop.js [PATH../]binaryen_wasm.js
//
// This should be run from the toplevel of the binaryen repo, as the paths are
// relative to there.

// Read the argument, and clear it, as fuzz_shell should not see any argument.
const binaryen_wasm_js_path = arguments[0];
arguments.length = 0;

console.log(`fuzz_loop: loading binaryen_wasm.js ({binaryen_wasm_js_path})`);
load(binaryen_wasm_js_path);

console.log('fuzz_loop: loading fuzz shell');
load('scripts/fuzz_shell.js');
