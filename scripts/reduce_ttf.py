
help = '''
Helps reduce a fuzz testcase that fails right after the -ttf stage. That is,
if the command that converts random bytes into a valid wasm emits an *in*valid
wasm, then reducing the wasm is often not possible, and you need to reduce the
input to -ttf. There is no perfect way to do that, since the input to -ttf is
random bytes, but we can at least find the smallest change that causes the
problem, which might help debug.

USAGE:

reduce_ttf.py $TEST_SCRIPT $INITIAL_TTF_INPUT

The script then runs

  $TEST_SCRIPT $CURRENT_TTF_INPUT

on various current -ttf input files as it reduces, and it checks each one for
validity by running the TEST_SCRIPT, which should return an exit code of non-
zero for the "interesting" cases. TEST_SCRIPT receives a parameter which is the
name of the current -ttf input.

TEST_SCRIPT must be an executable script.

All temp files are stored in out/test/.
'''

import os, shlex, subprocess, sys

try:
    test_script = shlex.split(sys.argv[1])
    initial_ttf_input = sys.argv[2]
except:
    print('Invalid input; showing help text.')
    print(help)
    sys.exit(1)

curr_ttf_input_file = os.path.join('out', 'test', 't.dat')

print('test script:', test_script)
print('initial_ttf_input:', initial_ttf_input)

# Read the initial data
with open(initial_ttf_input, mode='rb') as f:
    initial_ttf_input_data = f.read()
print('initial data size:', len(initial_ttf_input_data))


def is_interesting(data):
    # Write the data.
    with open(curr_ttf_input_file, mode='wb') as f:
        f.write(data)

    # Test the data. G Any non-zero return code is interesting.
    cmd = test_script + [curr_ttf_input_file]
    print('    ', shlex.join(cmd))
    p = subprocess.run(cmd, stderr=subprocess.PIPE)
    return p.returncode != 0


# Verify that we see the full input as "interesting"
if not is_interesting(initial_ttf_input_data):
    print('Initial ttf input is not interesting (return code should be != 0)')
    sys.exit(1)

# Verify that we see a trivial input as "boring".
if is_interesting(initial_ttf_input_data[0:1]):
    print('Trivial ttf input is interesting (return code should be == 0)')
    sys.exit(1)

# Bisect on the length of the initial input.
# TODO: Additional clever things.
low = 1
high = len(initial_ttf_input_data)
print(f'Bisecting between {low} and {high}')
while high - low > 1:
    mid = (low + high) // 2
    print(f'trying {mid}')
    if is_interesting(initial_ttf_input_data[0:mid]):
        high = mid
    else:
        low = mid
print(f'Finished bisection: the difference happens at {low} - {high}).')
low_file = curr_ttf_input_file + '.low'
with open(low_file, mode='wb') as f:
    f.write(initial_ttf_input_data[:low])
high_file = curr_ttf_input_file + '.high'
with open(high_file, mode='wb') as f:
    f.write(initial_ttf_input_data[:high])
print(f'Wrote low and high files for those: {low_file} - {high_file}).')

