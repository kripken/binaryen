#!/usr/bin/python3

import argparse
import logging
import os
import pathlib
import random
import subprocess
import sys
import tempfile
import time

from google import genai
from google.genai import types


# Binaryen paths

my_dir = os.path.dirname(os.path.abspath(__file__))
binaryen_root = os.path.dirname(my_dir)


def in_binaryen(*args):
    return os.path.join(binaryen_root, *args)


def in_bin(tool):
    return os.path.join(params.binaryen_bin or in_binaryen('bin'), tool)


# Global parameters from the user
params = None


# Execution


def run(*args):
    if params.verbose:
        print("  ", *args)
    return subprocess.check_output(list(args), text=True)


def run_wasm_opt(*args):
    return run(in_bin('wasm-opt'), *args)


def run_vm(*args):
    return run(params.vm, *args)


def run_node(*args):
    return run('node', *args)


# Execution utilities


# Verify if a JS file parses correctly.
def check_js_parsing(filename):
    return run_node('--check', filename)


# Logging

logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
logger = logging.getLogger(__name__)


# History: If asked to, we keep a history of prompts and fuzzers. A single index
# is used to track them all over time.
history_index = 0

def save_history(what, contents):
    if not params.save_history:
        return

    global history_index
    history_index += 1
    filename = f'{params.fuzzer_file}-{history_index}-{what}.txt'
    open(filename, 'w').write(contents)


# LLM client handling

SYSTEM_INSTRUCTION = '''
You are a software engineer that knows Python, with experience with fuzzers as
a way to find bugs by generating random testcases.
'''


class GeminiClient:
    def __init__(self):
        # Looks for GEMINI_API_KEY env var automatically.
        self.client = genai.Client()
        self.model = params.model
        self.temperature = params.temperature
        self.system_instruction = SYSTEM_INSTRUCTION
        self.chat_session = None

    def _execute_with_retry(self, func, *args, max_retries=5, **kwargs):
        """
        Handles exponential backoff for rate limits and transient server errors.
        """
        for attempt in range(max_retries):
            try:
                return func(*args, **kwargs)
            except Exception as e:
                # The new SDK raises various exceptions; we check for common retryable ones
                err_msg = str(e).lower()
                if "429" in err_msg or "rate limit" in err_msg:
                    wait = (2 ** attempt) + 1
                    logger.warning(f"Rate limit hit. Retrying in {wait}s...")
                    time.sleep(wait)
                elif "500" in err_msg or "503" in err_msg or "deadline" in err_msg:
                    wait = (2 ** attempt)
                    logger.warning(f"Server error/Timeout. Retrying in {wait}s...")
                    time.sleep(wait)
                else:
                    logger.error(f"Non-retryable error: {e}")
                    raise e
        raise Exception("Maximum retries reached. Operation failed.")

    def one_off(self, prompt):
        """Standard one-off prompt/response."""
        config = types.GenerateContentConfig(
            system_instruction=self.system_instruction,
            temperature=self.temperature,
        )

        save_history('prompt', prompt)

        response = self._execute_with_retry(
            self.client.models.generate_content,
            model=self.model,
            contents=prompt,
            config=config,
        )

        save_history('response', response.text)

        return response.text

    def chat(self, message):
        """Handles stateful conversation."""
        if self.chat_session is None:
            # Initialize the chat session if it doesn't exist
            config = types.GenerateContentConfig(
                system_instruction=self.system_instruction,
                temperature=self.temperature,
            )
            self.chat_session = self.client.chats.create(
                model=self.model,
                config=config,
            )

        save_history('chat-prompt', message)

        response = self._execute_with_retry(
            self.chat_session.send_message,
            message=message,
        )

        save_history('chat-response', response.text)

        return response.text

    def end_chat(self):
        self.chat_session = None


# Bundle text files into a prompt, with a header for each
def bundle_files(filenames):
    chunks = []
    for filename in filenames:
        # Header
        chunk = f">>>> {os.path.basename(filename)}\n"

        # Content
        content = open(filename, encoding='utf-8').read()
        chunk += content
        # Ensure there's a newline at the end of the content
        if not content.endswith('\n'):
            chunk += '\n'
        # Add an extra newline for clear separation between files
        chunk += '\n'
        chunks.append(chunk)

    return '\n'.join(chunks)


def read_fuzzer():
    return open(params.fuzzer_file).read()


# Write the fuzzer after receiving a response containing it
def write_fuzzer(response):
    # The LLM may wrap the fuzzer with
    #
    # ```python
    # ..code..
    # ```
    prefix = "```python"
    postfix = "```"
    if response.startswith(prefix):
        response = response[len(prefix):]
    if response.endswith(postfix):
        response = response[:-len(postfix)]
    open(params.fuzzer_file, 'w').write(response)
    save_history('fuzzer', response)


# Generate a random seed for the fuzzer
def random_seed():
    return random.randint(0, 1 << 64)


# The fuzzer's output has both JS and wasm, JS first, then this separator before
# the wasm:
JS_WAT_SEP = '>>>> wat'


# Temporary files for testcases
js_temp = tempfile.NamedTemporaryFile(prefix='testcase', suffix='.mjs')
wat_temp = tempfile.NamedTemporaryFile(prefix='testcase', suffix='.wat')
wasm_temp = tempfile.NamedTemporaryFile(prefix='testcase', suffix='.wasm')

error_temp = tempfile.NamedTemporaryFile(prefix='error', suffix='.txt')

def wat_to_wasm():
    run_wasm_opt('-all', wat_temp.name, '-o', wasm_temp.name)


# Run the fuzzer on a seed. Returns the raw js and wat output.
def run_fuzzer(seed):
    cmd = [sys.executable, params.fuzzer_file, str(seed)]
    output = subprocess.check_output(cmd, text=True)
    assert output.count(JS_WAT_SEP) == 1
    js, wat = output.split(JS_WAT_SEP)
    return js, wat


# Main prompt contents

FUZZER_GOALS = f'''

The fuzzer takes a single commandline parameter, and uses that number to
deterministically generate a testcase (this determinism makes it easy to debug
the fuzzer itself). How it generates the testcase given a number is arbitrary,
but we want to do well on the goals below.

The fuzzer's overall goals are:

* The outputs should be diverse, that is, many different testcases can be
  generated by this fuzzer, with many different aspects to them. Some testcases
  might be small, others large; some might have more of one element and others
  more of another, or a mixture; and so forth. We want to generate many varied
  testcases so we have the best chance to generate ones that reveal bugs.
* The outputs should be valid, in the sense explained below.

The specific testcases the fuzzer generates are WebAssembly and JavaScript
combination programs. Each testcase contains a JavaScript file that loads a
corresponding WebAssembly file, then runs and interacts with it.

We are fuzzing the WebAssembly JavaScript Interop proposal, so the interaction
between wasm and JS is especially important to cover. This proposal allows
creating objects that are usable from JS but are implement by wasm (as you will
see in the examples below). Using such objects in JS, and passing them back and
forth to and from wasm, are areas that we suspeect might have bugs the fuzzer
can find.

For simplicity, the fuzzer should emit the wasm files in the wat text format, as
you will see in the examples below.

Our specific focus is on security and correctness. We are not interested in
simple parsing bugs, that is, if the WebAssembly or the JavaScript does not even
parse, that is not useful. We want the programs to run and do useful, realistic
work. While running them we will use tools like Address Sanitizer to look for
memory problems, and we will compare the outputs in different modes (JIT vs no
JIT, for example) to look for correctness issues.

It is ok if some testcases throw an exception or trap. Such errors do not need
to be avoided entirely, but we do want most of the code to mostly run, so that
we actually test lots of executing code.

Some tips for achieving these goals:

* Decide on rates of certain things. For example, perhaps one program has lots
  of calls while the other has few. Either at the program or function level, you
  can pick a rate of things like calls, and use it in that scope. Different
  programs and functions will therefore vary when you pick a different rate of
  calls in them.

The output of the fuzzer is a pair of js and wasm files, in a single response.
The JS should begin immediately at the start of the fuzzer's output, without any
prefix or annotation. To separate the wat from it, use this separator line:

{JS_WAT_SEP}

That is, the output should like like this (without ```` in the actual output):

```
JavaScript code

{JS_WAT_SEP}

WebAssembly text format
```

'''

# After fixing the fuzzer once, we re-run all checks again. This is the maximum
# number of iterations before we give up entirely - if we hit this, then we seem
# to not be making progress.
MAX_FIX_ITERS = 10


# The fuzzer is updated using diffs. We use a simple format to avoid the LLM
# getting line numbers/counts wrong.
DIFF_PREFIX = '<<<<<<< SEARCH'
DIFF_MIDDLE = '======='
DIFF_POSTFIX = '>>>>>>> REPLACE'
DIFF_FORMAT = f'''\
{DIFF_PREFIX}
[Existing code that needs to change]
{DIFF_MIDDLE}
[Improved code]
{DIFF_POSTFIX}
'''


# Returns an error if we failed to update.
def update_fuzzer(diff):
    diff = diff.strip()
    if not diff.startswith(DIFF_PREFIX):
        return f'Did not find the right prefix ({DIFF_PREFIX})'
    if diff.count(DIFF_MIDDLE) != 1:
        return f'The diff separator ({DIFF_MIDDLE}) must appear exactly once'
    if not diff.endswith(DIFF_POSTFIX):
        return f'Did not find the right post ({DIFF_POSTFIX})'

    diff = diff[len(DIFF_PREFIX):-len(DIFF_POSTFIX)]
    existing, improved = diff.split(f'\n{DIFF_MIDDLE}\n')

    fuzzer = read_fuzzer()
    fuzzer = fuzzer.replace(existing, improved)
    write_fuzzer(fuzzer)


# Functions that check for things, and fix them as needed

FAILURE = 'FAILURE'

FIX_EXISTING_FUZZER_INTRO = '''
We are writing a fuzzer in Python.

''' + FUZZER_GOALS + '''

The fuzzer has a problem that I want you to fix. The fuzzer itself is attached
below, as well the seed that reproduces the bug, and the relevant part of the
output that shows the problem.

Write a diff for the fuzzer that fixes the problem, with no other text. I will
apply that diff and run the fuzzer with the seed, then verify that the output
is correct.

Emit the diff in the following form:

{DIFF_FORMAT}

If you cannot find a fix, emit instead the word "FAILURE" in capital letters,
followed by explanation of the problems you hit.

'''

def ensure_js_parsing(seed, js):
    open(js_temp.name, 'w').write(js)
    proc = check_js_parsing(js_temp.name)
    if not proc.returncode:
        return

    print("❌ JS does not parse")

    open(error_temp.name, 'w').write(proc.stdout)

    prompt = FIX_EXISTING_FUZZER_INTRO
    prompt += f'Problem: the JavaScript for seed {seed} does not parse. '
    prompt += 'The error follows the JavaScript contents.\n\n'
    prompt += bundle_files([js_temp.name, error_temp.name, params.fuzzer_file])

    client = GeminiClient()
    response = client.chat(prompt)

    # Loop on LLM responses.
    for i in range(MAX_FIX_ITERS):
        print("    (fix attempt {i})")

        if response.startswith(FAILURE):
            print("❌ LLM gave up")
            sys.exit(1)

        # Apply the diff and try the testcase again.
        error = update_fuzzer(response)
        if error:
            client.chat('Your diff is not in the proper format:\n{DIFF_FORMAT}\n\n(error: {error})\n')
            continue

        try:
            js, _ = run_fuzzer(seed)
        except subprocess.CalledProcessError:
            print("❌ Fuzzer crashes, fixing...")
            client.chat('After your diff, the fuzzer crashes')
            continue

        open(js_temp.name, 'w').write(js)
        proc = check_js_parsing(js_temp.name)
        if not proc.returncode:
            print("✅ JS parsing fixed")
            return

        open(error_temp.name, 'w').write(proc.stdout)

        prompt = 'The JavaScript still does not parse. '
        prompt += 'Here is the JS and error:\n\n'
        prompt += bundle_files([js_temp.name, error_temp.name])
        client.chat(prompt)


# How many random samples to validate with
NUM_VALIDATIONS = 20 # XXX moar


# Tests various things and fixes the fuzzer. This does one forward iteration,
# i.e., it does not backtrack to previous checks after fixing something. Returns
# True if we fixed something.
def fix_fuzzer_iter():
    fixed = False

    # A map of seeds to the fuzzer's outputs (pairs of js, wat).
    outputs = {}

    print("💼 Validating fuzzer")

    while len(outputs) < NUM_VALIDATIONS:
        seed = random_seed()

        # In rare cases seeds might overlap. Skip them.
        if seed in outputs:
            continue

        # Check we do not crash when generating testcases.
        try:
            output = run_fuzzer(seed)
        except subprocess.CalledProcessError:
            print("❌ Fuzzer crashes, fixing...")
            # TODO LLM fix
            3/0

        outputs[seed] = output

        # The same number should lead to the same output.
        try:
            output2 = run_fuzzer(seed)
        except subprocess.CalledProcessError:
            print("❌ Fuzzer is nondeterministic, now crashes, fixing...")
            # TODO LLM fix
            3/0

        if output2 != output:
            print("❌ Fuzzer is nondeterministic, fixing...")
            # TODO LLM fix
            3/0

    # Different numbers should lead to different outputs.
    for seed, output in outputs.items():
        for seed2, output2 in outputs.items():
            # TODO: Check more carefully, ignoring names/comments/etc?
            if seed2 != seed and output2 == output:
                print("❌ Fuzzer has collision, fixing...")
                # TODO LLM fix
                3/0

    # Check the testcases parse.
    for seed, output in outputs.items():
        js, wat = output

        fixed = ensure_js_parsing(seed, js) or fixed

        open(wat_temp.name, 'w').write(wat)
        try:
            run_wasm_opt('-all', wat_temp.name)
        except subprocess.CalledProcessError:
            print("❌ Fuzzer wat does not parse, fixing...")
            # TODO LLM fix
            3/0

    # Check at least some testcases run without error.
    2/0

    return fixed


def fix_fuzzer():
    fixed = False
    for _ in range(MAX_FIX_ITERS):
        if not fix_fuzzer_iter():
            if fixed:
                print("✅ Fuzzer was successfully fixed")
            return
        fixed = True


# Generate the initial fuzzer

INITIAL_GENERATION_PROMPT = '''
Write a fuzzer in Python that generates things similar to the examples below.

''' + FUZZER_GOALS + '''

The following pairs of wasm+js are examples of useful, working programs in the
style we would like the fuzzer to generate. That is, the fuzzer should be
capable of generating programs generally similar to them, but of course
individual testcases may be very different. The fuzzer should also be able
generate testcases that are generally similar to combinations of them, that is,
combining interesting elements from different examples here.

Emit the Python fuzzer in your output, with no other text. Explanations for
the fuzzer's approach or parts of the fuzzer can be in code comments inside the
Python.

Example testcases:

'''


def generate_initial_fuzzer():
    print("💼 Generating initial fuzzer")

    # Use all our js_wasm testcases as initial examples.
    js_files = list(pathlib.Path(in_binaryen('test', 'js_wasm')).glob('*.mjs'))
    examples = []
    for js_file in js_files:
        examples.append(str(js_file))
        examples.append(str(pathlib.Path(js_file).with_suffix('.wat')))

    prompt = INITIAL_GENERATION_PROMPT + bundle_files(examples)

    client = GeminiClient()
    response = client.one_off(prompt)
    write_fuzzer(response)

    fix_fuzzer()


# Improve the fuzzer in a single iteration of the main loop

def improve_fuzzer():
    fix_fuzzer()

    print("💼 Improving fuzzer by doing ..?")
    0/4

# Main workflow.
def work():
    # Create the initial fuzzer, if there is none.
    if not os.path.exists(params.fuzzer_file):
        generate_initial_fuzzer()
    else:
        print("💼 Improving existing fuzzer")

    # Iterately improve the fuzzer.
    try:
        for i in range(params.max_iters):
            print(f"⏱️  Improving fuzzer, iteration {i}")
            improve_fuzzer()
    except KeyboardInterrupt:
        print("🛑 Stopping by user request.")


def main():
    parser = argparse.ArgumentParser(description="SlopFuzz")
    parser.add_argument("--model", type=str, default="gemini-3-flash-preview", help="Model ID")
    parser.add_argument("--temperature", type=float, default=0.7, help="Creativity temperature")
    parser.add_argument("--fuzzer-file", type=str, required=True, help="File to write the fuzzer in (must be inside a git repo, as each successful update is committed)")
    parser.add_argument("--max-iters", type=int, default=1000, help="Maximum number of iterations to run")
    parser.add_argument("--save-history", default=False, action="store_true", help="Save history of prompts and fuzzers as we go, for debugging (uses the fuzzer-file with different suffixes)")
    parser.add_argument("--binaryen-bin", type=str, help="Directory with Binaryen binaries (wasm-opt)")
    parser.add_argument("--vm", type=str, required=True, help="VM to run the testcases in")
    parser.add_argument("--verbose", default=False, action="store_true", help="Log very verbosely")

    global params
    params = parser.parse_args()
    
    print(f"📖 Using model {params.model}")

    work()


if __name__ == "__main__":
    main()
