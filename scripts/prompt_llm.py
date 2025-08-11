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
import shutil
import subprocess
import sys

script_dir = os.path.dirname(os.path.abspath(__file__))
src_dir = os.path.join(os.path.dirname(os.path_dirname(os.path.abspath(__file__))), 'src')

print('importing...')

from google import genai

print('configuring...')

key = os.getenv('GOOGLE_API_KEY')
client = genai.Client(api_key=key)

model_name = 'gemini-2.5-pro'

model = genai.GenerativeModel(model_name=model_name,
                              generation_config=generation_config)


def do_prompt(prompt):
    print(f'Prompting {len(prompt)} bytes...')
    response = client.models.generate_content(model=model_name, contents=prompt)
    print(response.text)


# Files we always want to include, for context.
def get_core_files():
    return [
        os.path.join(src_dir, 'wasm-types.h'),
        os.path.join(src_dir, 'literal.h'),
        os.path.join(src_dir, 'wasm.h'),
        os.path.join(src_dir, 'wasm-traversal.h'),
        os.path.join(src_dir, 'pass.h'),
        os.path.join(src_dir, 'ir', 'effects.h'),
    ]


# Given C++ code, find the headers mentioned there.
def get_headers_used_by(code):
    pattern = re.compile(r'^#include\s*"([^"]+)"', re.MULTILINE)
    return pattern.findall(content)


# Given the code of a Binaryen pass, find the commandline flag to use it.
def get_commandline_pass_name(code):
    # The pass creates itself using something like
    #
    #   Pass* createFoo() { ..
    #
    # Using that function name, we can look in pass.cpp to find the commandline
    # argument.
    pattern = re.compile(r'^Pass[*] (\w+)[(]', re.MULTILINE)
    creators = pattern.findall(content)
    


if __name__ == "__main__":
    cmd = sys.argv[1]
    arg = sys.argv[2]

    if cmd == 'bugfind':
        main = arg
        code = open(filename).read()

        prompt = f'''
You are an expert in compilers. Please look through the attached code and
try to find bugs in it.

The main source file I would like you to focus on is {main}. I will provide other
files too, for context, and if you happen to find a bug there, please report it.

Report only one bug. Try to be sure that it is a bug, or at least that it is
likely to be done.

This code has been heavily tested and fuzzed, so trivial bugs are very unlikely,
but due to the complexity of the code, bugs probably exist.

If you do not find bugs but do find missing corner cases in the tests, that can
be useful as well, and please mention that too.
'''

        # Files to bundle. Start with core files, plus the main one.
        files = get_core_files()
        files.add(main)

        # Add all headers that the main file refers to.
        files.update(get_headers_used_by(code))

        # Find the commandline name of the pass, and find all test files with
        # that name in them.
        files.update(get_tests_with_name(get_commandline_pass_name(code)))

        print(files)
        1/0
        subprocess.check_call(['python3', os.path.join(script_dir, 'bundle_llm.py'] + files)
        do_prompt(prompt)
    else:
        print('invalid command')
        sys.exit(1)

