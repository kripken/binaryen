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

script_dir = 'scripts'
src_dir = 'src'
test_dir = 'test'

bundler = ['python', os.path.join(script_dir, 'bundle_llm.py')]

'''
print('importing...')

from google import genai

print('configuring...')

key = os.getenv('GOOGLE_API_KEY')
client = genai.Client(api_key=key)

model_name = 'gemini-2.5-pro'
'''

def do_prompt(prompt):
    print(f'Prompting {len(prompt)} bytes...')
    response = client.models.generate_content(model=model_name, contents=prompt)
    print(response.text)


# Files we always want to include, for context.
def get_core_files():
    return [
        os.path.join(src_dir, 'wasm-type.h'),
        os.path.join(src_dir, 'literal.h'),
        os.path.join(src_dir, 'wasm.h'),
        os.path.join(src_dir, 'wasm-traversal.h'),
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
    arg = sys.argv[2]

    if cmd == 'bugfind':
        main = arg
        code = open(main).read()

        # Files to bundle. Start with main.
        files = [main]

        # Next, add core files and headers used by main.
        others = get_core_files()
        for header in get_headers_used_by(code):
            if header not in others:
                others.append(header)
        files += others

        # Find the commandline name of the pass, and find all test files with
        # that name in them.
        files += get_tests_with_names(get_commandline_pass_names(code))

        print('🚀 Invoking bundler:', file=sys.stderr)

        # Bundle them up in an LLM-friendly manner.
        bundle = subprocess.check_output(bundler + files, encoding='utf-8')

        print(f'🚀 Bundle size: {len(bundle)} bytes', file=sys.stderr)

        prompt = f'''
You are an expert in compilers. Please look through the attached code and
try to find a bug in it.

The main source file I would like you to focus on is

{main}

I will provide other files too, for context, and if you happen to find a bug in
them there, please report it.

Report only one bug. Try to be sure that it is a bug, or at least that it is
likely to be one.

This code has been heavily tested and fuzzed, so trivial bugs are very unlikely,
but due to the complexity of the code, bugs probably exist. Look very carefully.

If you do not find bugs but do find missing corner cases in the tests, that can
be useful as well, and please mention them, with suggestions for what testing to
add.

'''

        prompt += bundle
        print(prompt)
        1/0
        do_prompt(prompt)
    else:
        print('invalid command')
        sys.exit(1)

