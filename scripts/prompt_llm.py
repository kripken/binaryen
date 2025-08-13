"""
Prompting tool with Binaryen integration.

Usage:

GOOGLE_API_KEY=... python prompt_llm.py COMMAND FILE

For example:

GOOGLE_API_KEY=... python prompt_llm.py bugfind src/passes/OptimizeInstructions.cpp

Setup: You may need

$ pip install google-genai

Uses

https://googleapis.github.io/python-genai/
"""

import os
import re
import shutil
import subprocess
import sys
import time

script_dir = 'scripts'
src_dir = 'src'
bin_dir = 'bin'
test_dir = 'test'

bundler = ['python', os.path.join(script_dir, 'bundle_llm.py')]
wasm_opt = [os.path.join(bin_dir, 'wasm-opt')]

print('importing...')

from google import genai

print('configuring...')

key = os.getenv('GOOGLE_API_KEY')
client = genai.Client(api_key=key)

model_name = 'gemini-2.5-flash'
#model_name = 'gemini-2.5-pro'


def start_chat():
    print(f'🚀 Starting a new chat ({model_name})', file=sys.stderr)
    return client.chats.create(model=model_name)


def continue_chat(chat, prompt, bundle=''):
    print(f'🚀 Prompting:', file=sys.stderr)
    print(f'<<< PROMPT BEGINS ({len(prompt)} bytes)>>>')
    print(prompt)
    print('<<< PROMPT ENDS >>>')
    if bundle:
        print(f'<<< Appended bundle of size {len(bundle)} >>>')
    start = time.time()
    response = chat.send_message(prompt + '\n' + bundle)
    print('<<< RESPONSE BEGINS >>>')
    print(response.text)
    print('<<< RESPONSE ENDS >>>')
    print(f'🚀 Done ({len(response.text)} bytes output in {time.time() - start} seconds)',
          file=sys.stderr)
    return response.text


# Files we always want to include, for context.
def get_core_files():
    return [
        os.path.join(src_dir, 'wasm-type.h'),
        os.path.join(src_dir, 'literal.h'),
        os.path.join(src_dir, 'wasm.h'),
        os.path.join(src_dir, 'wasm-traversal.h'),
        os.path.join(src_dir, 'wasm-interpreter.h'),
        os.path.join(src_dir, 'pass.h'),
        os.path.join(src_dir, 'ir', 'effects.h'),
    ]


# Given C++ code, find the headers mentioned there.
def get_headers_used_by(code):
    includes = re.findall(r'^#include\s*"([^"]+)"', code, re.MULTILINE)
    # Add 'src/' prefix
    return [os.path.join('src', include) for include in includes]


# Given a string, find all tests that contain it in their basename. This helps
# find all tests for a particular pass.
def get_tests_with_names(search_names):
    files = []
    for dirpath, _, filenames in os.walk(test_dir):
        for filename in filenames:
            if not filename.endswith('.wast'):
                continue
            for search_name in search_names:
                if search_name in filename:
                    # Create the full path to the file
                    full_path = os.path.join(dirpath, filename)
                    files.append(full_path)
    return files


def run_wasm_opt(args):
# TODO timeout!
    print(f'🚀 Running wasm-opt {' '.join(args)}', file=sys.stderr)
    return subprocess.run(wasm_opt + args,  capture_output=True, text=True, timeout=3)


# Marker the LLM emits when it gives up.
not_a_bug = 'NOT A BUG'
# Marker the LLM emits when it thinks it is right and we are wrong, and it has
# no need to revise its testcase.
good_already = 'GOOD ALREADY'


# Given an LLM response, process it: see if the finding is valid, and if not,
# return a prompt that requests improvements. (If it is valid, return nothing.)
# Receives the last response + the pass names we are looking for bugs in.
def process_testcase(response, pass_names):
    wat = extract_testcase(response)
    open('t.wat', 'w').write(wat)
    print(f'🚀 Extracted testcase:', file=sys.stderr)
    print(wat)

    # All errors add the same suffix.
    error_suffix = f'''

Perhaps you can fix it up? If so, please attach the fixed testcase at the end of
your output. Or, if you now realize that you have not found a bug, just write
"{not_a_bug}". Or, if you believe your testcase is still valid despite what I
have shown you, and it requires no more revisions, just write "{good_already}".
'''

    # See if it is even a valid wat file.
    result = run_wasm_opt(['-all', 't.wat'])
    if result.returncode:
        return f'''
The testcase you provided does not seem to be valid wat. Here is what I get when
I load it using `wasm-opt -all`:

```
{result.stdout}
{result.stderr}
```
''' + error_suffix

    # See if it runs without hanging forever.
    try:
        run_wasm_opt(['-all', 't.wat', '--fuzz-exec-before'])
    except subprocess.TimeoutExpired:
        return f'''
The testcase you provided hangs forever when I run it with

```
wasm-opt -all --fuzz-exec-before
```

Remember, I don't want testcases that infinite loop.
''' + error_suffix

    # Try to run the command on the passes we were given, looking for bugs.
    assert pass_names, 'must be passes to run'
    for pass_name in pass_names:
        result = run_wasm_opt(['-all', 't.wat', '--print', '--' + pass_name, '--print', '--fuzz-exec'])
        if result.returncode:
            # Success! An error means we found a bug; nothing more to prompt
            return ''

    # No bug found. Report the last output.
    return f'''
The testcase you provided does not seem to show a bug. Here is what I get when I
run `wasm-opt -all --print --{pass_names[-1]} --print --fuzz-exec`, which
prints the module before and after the optimization, in addition to executing it
before and after, so you can see exactly what happens:

```
{result.stdout}
{result.stderr}
```

''' + error_suffix


# Given a text response, find the last wat testcast in it, and return that. The
# LLM is always told to put the testcase at the end, so any other wat fragments
# earlier (in the explanation, say) can be ignored.
def extract_testcase(response):
    start = response.rfind('(module')
    end = response.find('\n)', start)
    return response[start:end + 2]


# Given the code of a Binaryen pass, find the commandline flag(s) to use it.
def get_commandline_pass_names(code):
    # The pass creates itself using something like
    #
    #   Pass* createFoo() { ..
    #
    # Using that function name, we can look in pass.cpp to find the commandline
    # argument, where it will appear as something like
    #
    #   registerPass("foo", "description of foo", createFoo);
    #
    creators = re.findall(r'^Pass\* (\w+)\(', code, re.MULTILINE)

    pass_cpp_code = open(os.path.join(src_dir, 'passes', 'pass.cpp')).read()
    pairs = re.findall(r'registerPass[(]\s*"([\w-]+)",\s*"[^"]+",\s*(\w+)[)]',
                       pass_cpp_code)

    # Find matches among them.
    flags = []
    for flag, creator in pairs:
        if creator in creators:
            flags.append(flag)
    return flags


if __name__ == "__main__":
    cmd = sys.argv[1]
    main = sys.argv[2]

    code = open(main).read()

    # Files to bundle. Start with main.
    files = [main]

    # Next, add core files and headers used by main.
    others = get_core_files()
    for header in get_headers_used_by(code):
        if header not in others:
            others.append(header)
    files += others

    # Find the commandline name(s) of the pass, and find all test files with
    # that name in them.
    pass_names = get_commandline_pass_names(code)
    files += get_tests_with_names(pass_names)

    print('🚀 Invoking bundler:', file=sys.stderr)

    # Bundle them up in an LLM-friendly manner.
    bundle = subprocess.check_output(bundler + files, encoding='utf-8')

    print(f'🚀 Bundle size: {len(bundle)} bytes', file=sys.stderr)

    # The commands we hope to find a bug using.
    commands = '\n'.join(['wasm-opt -all --fuzz-exec --' + name for name in pass_names])

    prompt = f'''
You are an expert in compilers. Please look through the attached code and
try to find a bug in it, of one of these types:

1. A crash or internal error in the optimizer.
2. A correctness error, where the optimizer generates incorrect code.

This code is from Binaryen, an optimizer for WebAssembly.

The main source file I would like you to focus on is

{main}

I will provide other files too, for context, and if you happen to find a bug in
them there, please report it.

This code has been heavily tested and fuzzed, so trivial bugs are very unlikely,
but due to the complexity of the code, bugs probably exist. Look very carefully.

Report only one bug. Try to be sure that it is a bug, or at least that it is
likely to be one.

For the bug you find, provide a full wat testcase at the end of your output. I
will run that testcase through `wasm-opt` to see what happens when it runs the
code I asked you to focus on. If it crashes, or if it behaves differently after
optimizations then it is a valid bug. Specifically, I will be running

```
{commands}
```

The testcase should not infinite loop, as then I cannot verify it shows a bug.
(Ignore bugs related to preserving infinite loops.)

Note that you cannot run wasm-opt on your side, but I will do so and inform you
what I see, and we can continue from there.

Some important notes:

* The Binaryen optimizer ignores differences between traps (as mentioned in
  effects.h). We consider all traps equal, so it is ok to reorder them, even if
  the logged message is different. For example, `[trap unreachable]` is the same
  as `[trap i32.div_u by 0]`.

The code follows:
'''
    # TODO: maybe look for missing test coverage too?

    # Start the conversation.
    chat = start_chat()
    response = continue_chat(chat, prompt, bundle)

    # Check if the given testcase shows an actual bug, and if not, tell the
    # AI and see if it can fix things. Give it several chances to do so
    # before giving up.
    i = 0
    while True:
        # If the testcase is not valid, we get a prompt to issue.
        prompt = process_testcase(response, pass_names)
        if not prompt:
            print(f'🚀 Success!', file=sys.stderr)
            sys.exit(0)

        # Failure.
        i += 1
        if i == 10:
            print(f'❌ Giving up.', file=sys.stderr)
            sys.exit(1)

        # Keep hoping...
        print(f'❌ Not valid in iteration {i}', file=sys.stderr)
        response = continue_chat(chat, prompt)

        if not_a_bug in response:
            print(f'❌ LLM gave up.', file=sys.stderr)
            sys.exit(1)
        if good_already in response:
            print(f'❌ LLM is being stubborn.', file=sys.stderr)
            sys.exit(1)

