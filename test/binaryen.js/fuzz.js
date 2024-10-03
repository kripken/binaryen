// Some "random" data.
const NUM = 2048;
const bytes = new Uint8Array(NUM);
for (let i = 0; i < NUM; i++) {
  bytes[i] = (i * i) & 0xff;
}

// Build with and without allowOOB.
const sizes = [];
for (let oob = 0; oob < 2; oob++) {
  // Create an empty module and add fuzz.
  const module = new binaryen.Module();
  module.addFuzz(bytes, oob);

  // The output must be valid.
  assert(module.validate());

  // The size must be reasonable.
  const size = module.emitText().length;
  assert(size >= 20);
  sizes[oob] = size;

  module.dispose();
}

// Test that the OOB mode had an effect.
assert(sizes[0] != sizes[1]);

