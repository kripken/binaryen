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
    return subprocess.run(list(args),
                          stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT,
                          text=True)


def run_wasm_opt(*args):
    return run(in_bin('wasm-opt'), *args)


def run_vm(*args):
    args = [
        params.vm,
        '--wasm-staging',
        '--experimental-wasm-compilation-hints',
        '--experimental-wasm-stringref',
        '--experimental-wasm-fp16',
        '--experimental-wasm-custom-descriptors',
        '--experimental-wasm-js-interop',
    ] + list(args)
    return run(*args)


def run_node(*args):
    return run('node', *args)



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
                    wait = (2 ** attempt) + 10
                    logger.warning(f"Rate limit hit. Retrying in {wait}s...")
                    time.sleep(wait)
                elif "500" in err_msg or "503" in err_msg or "deadline" in err_msg:
                    wait = (2 ** attempt)
                    logger.warning(f"Server error/Timeout ({err_msg}). Retrying in {wait}s...")
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


# Bundle text files into a prompt, with a header for each. Each item can be a
# tuple of a filename and a comment, or just a filename
def bundle_files(files):
    chunks = []
    for item in files:
        if type(item) is tuple:
            filename, comment = item
        else:
            filename = item
            comment = None

        # Header
        chunk = f">>>> {os.path.basename(filename)}"
        if comment:
            chunk += f" ({comment})"
        chunk += "\n"

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


# Run the fuzzer on a seed, returning the process.
def run_fuzzer_proc(seed):
    return run(sys.executable, params.fuzzer_file, str(seed))


# Run the fuzzer on a seed. Returns the raw js and wat output.
def run_fuzzer(seed):
    proc = run_fuzzer_proc(seed)
    assert proc.returncode == 0
    output = proc.stdout
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

Some specific things to focus on:

* Avoid unbounded loops on the JS side, as we do not want the testcase
  to hang. Loops on the wasm side are ok, as we have special wasm
  instrumentation that avoids infinite loops and recursion.
* Sending objects over the wasm/JS boundary is important, as this is a common
  source of bugs in VMs. For example, you can send an object from JS to wasm and
  use it there, and vice versa.

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
DIFF_START = '<< SEARCH'
DIFF_MIDDLE = '======='
DIFF_END = '>> REPLACE'
DIFF_FORMAT = f'''\
{DIFF_START}
[Existing code that needs to change]
{DIFF_MIDDLE}
[Improved code]
{DIFF_END}

(Multiple diff chunks like this can appear. Make sure to format them all properly.)
'''

BAD_DIFF_PROMPT = f"""
Your diff is not in the proper format:

{DIFF_FORMAT}

"""

# Returns a prompt with the error if we failed to update.
def update_fuzzer(diff):
    if DIFF_START not in diff:
        return BAD_DIFF_PROMPT + "Diff should start with `{DIFF_START}`"

    fuzzer = read_fuzzer()
    start = 0
    while True:
        start = diff.find(DIFF_START, start)
        if start < 0:
            break
        middle = diff.find(DIFF_MIDDLE, start)
        if middle < 0:
            return BAD_DIFF_PROMPT + "Diff should have {DIFF_MIDDLE}"
        end = diff.find(DIFF_END, middle)
        if end < 0:
            return BAD_DIFF_PROMPT + "Diff should end with `{DIFF_END}`"
        
        existing = diff[start + len(DIFF_START) + 1:middle]
        improved = diff[middle + len(DIFF_MIDDLE) + 1:end]

        print(f"replacing\n{existing}\nwith\n{improved}\n") # XXX debuggingg

        if existing not in fuzzer:
             prompt = "Your diff asks us to replace something that does not exist:\n"
             prompt += f"\n```\n{existing}\n```\n"
             prompt += "\nThe fuzzer we are trying to update is currently this:\n"
             prompt += bundle_files([params.fuzzer_file])
             return prompt

        fuzzer = fuzzer.replace(existing, improved)
        
        start = end + len(DIFF_END)

    write_fuzzer(fuzzer)


# Functions that check for things, and fix them as needed

FAILURE = 'FAILURE'

FIX_EXISTING_FUZZER_INTRO = '''
We are writing a fuzzer in Python.

''' + FUZZER_GOALS + f'''

The fuzzer has a problem that I want you to fix. The fuzzer itself is attached
below, as well the seed that reproduces the bug, and the relevant part of the
output that shows the problem.

Write a diff for the fuzzer that fixes the problem, with no other text. I will
apply that diff and run the fuzzer with the seed, then verify that the output
is correct.

Emit the diff in the following form:

{DIFF_FORMAT}

If you cannot find a fix (because my instructions are not clear enough, or you
think something is going wrong in the tools we am using, or some other problem
that you can't get around), emit instead the word "FAILURE" in capital letters,
followed by explanation.

'''
# TODO: Add the examples again, as a reminder?


# A process-like object with a returncode and an error (in stdout; we assume
# stdout and stderr were merged).
class ProcError:
    def __init__(self, error):
        self.returncode = 1
        self.stdout = error


class Fixer:
    what = 'Problem name'

    # An extra explanation to provide.
    extra_explanation = None

    # Checks for a problem, returning a subprocess execution result.
    def test(self):
        raise Exception("unimplemented")

    # Returns a list of the files to bundle for the repro.
    def get_files(self):
        raise Exception("unimplemented")

    # Generic loop to fix a problem. Returns true if we fixed something.
    def fix(self):
        proc = self.test()
        if not proc.returncode:
            return False

        problem = f"{self.what} is failing"
        print(f"❌ {problem}")

        prompt = FIX_EXISTING_FUZZER_INTRO
        prompt += f"The problem to fix: {self.what} is broken.\n"
        prompt += "The error follows the contents.\n\n"
        if self.extra_explanation:
            prompt += f"\n{self.extra_explanation}\n\n"
        open(error_temp.name, 'w').write(proc.stdout)
        prompt += bundle_files(self.get_files() + [
            (error_temp.name, 'error output'),
            (params.fuzzer_file, 'fuzzer program'),
        ])

        client = GeminiClient()
        response = client.chat(prompt)

        # Loop on LLM responses.
        for i in range(MAX_FIX_ITERS):
            print(f"    (fix attempt {i})")

            if response.startswith(FAILURE):
                print("❌ LLM gave up")
                sys.exit(1)

            # Apply the diff and try the testcase again.
            prompt = update_fuzzer(response)
            if prompt:
                response = client.chat(prompt)
                continue

            proc = self.test()
            if not proc.returncode:
                print(f"✅ {self.what} fixed")
                return True

            open(error_temp.name, 'w').write(proc.stdout)

            prompt = f'{self.what} is still not fixed. Here are the details:\n\n'
            prompt += bundle_files(self.get_files() + [
                (error_temp.name, 'error output'),
            ])
            response = client.chat(prompt)


class SeededFixer(Fixer):
    def __init__(self, seed):
        self.seed = seed


class CrashFixer(SeededFixer):
    what = "fuzzer execution"

    extra_explanation = "The fuzzer itself is crashing\n"

    def test(self):
        return run_fuzzer_proc(self.seed)

    def get_files(self):
        return []  # No addiitonal files needed


class ParsingFixer(SeededFixer):
    what = "JavaScript parsing"

    def test(self):
        proc = run_fuzzer_proc(self.seed)
        if proc.returncode:  # XXX get_files should be different here!
            4/0
            return proc

        output = proc.stdout
        if output.count(JS_WAT_SEP) != 1:
            4/0
            return ProcError("Separator between JS and wasm ({JS_WAT_SEP}) not found")
        js, wat = output.split(JS_WAT_SEP)

        return self.parse(js, wat)


class JSParsingFixer(ParsingFixer):
    what = "JavaScript parsing"

    def parse(self, js, wat):
        open(js_temp.name, 'w').write(js)
        return run_node('--check', js_temp.name)

    def get_files(self):
        return [(js_temp.name, "emitted JavaScript that does not parse")]


class WatParsingFixer(ParsingFixer):
    what = "WebAssembly text parsing"

    def parse(self, js, wat):
        open(wat_temp.name, 'w').write(wat)
        return run_wasm_opt(wat_temp.name, '-all')

    def get_files(self):
        return [(wat_temp.name, "emitted wat that does not parse")]


class ExecutionFixer(SeededFixer):
    what = "JS+wasm testcase execution"

    extra_explanation = "The JS+wasm testcase should execute without error.\n" + \
        "Attached is an example of a working testcase, which might help.\n"

    def test(self):
        proc = run_fuzzer_proc(self.seed)
        if proc.returncode:  # XXX get_files should be different here!
            4/0
            return proc

        output = proc.stdout
        if output.count(JS_WAT_SEP) != 1:
            4/0
            return ProcError("Separator between JS and wasm ({JS_WAT_SEP}) not found")
        js, wat = output.split(JS_WAT_SEP)

        open(js_temp.name, 'w').write(js)
        open(wat_temp.name, 'w').write(wat)
        wat_to_wasm()

        return run_vm(js_temp.name, '--', wasm_temp.name)

    def get_files(self):
        # Provide a working example after the failing testcase, to be helpful.
        working_pair = get_examples()[:2]
        return [
            (js_temp.name, "JavaScript part of the erroring testcase"),
            (wat_temp.name, "Wasm part of the erroring testcase"),
            (working_pair[0], "An example of a working testcase, JavaScript part"),
            (working_pair[1], "The Wasm part of the working example"),
        ]


# How many random samples to validate with
NUM_VALIDATIONS = 100


# Tests various things and fixes the fuzzer. This does one forward iteration,
# i.e., it does not backtrack to previous checks after fixing something. Returns
# True if we fixed something.
def validate_fuzzer_iter():
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
        fixed = CrashFixer(seed).fix() or fixed

        # Get the output after we no longer crash.
        try:
            output = run_fuzzer(seed)
        except subprocess.CalledProcessError:
            print("❌ Fuzzer crashes after fix")
            sys.exit(1)

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

    # Check testcases parse.
    for _ in range(NUM_VALIDATIONS):
        seed = random_seed()
        fixed = JSParsingFixer(seed).fix() or fixed
        fixed = WatParsingFixer(seed).fix() or fixed

    # We can't expect all testcases to execute without error - and we do want to
    # test some error handling - but a significant amount should avoid erroring.
    errored = 0
    erroring_seed = None
    for _ in range(NUM_VALIDATIONS):
        seed = random_seed()
        if ExecutionFixer(seed).test().returncode:
            errored += 1
            erroring_seed = seed

    if errored / NUM_VALIDATIONS > 0.25:
        print(f"❌ Too many execution errors: {int(100 * errored / NUM_VALIDATIONS)}%")
        # Too many errored. Fix up one of them.
        fixed = ExecutionFixer(erroring_seed).fix() or fixed

    return fixed


def validate_fuzzer():
    fixed = False
    for _ in range(MAX_FIX_ITERS):
        if not validate_fuzzer_iter():
            if fixed:
                print("✅ Fuzzer was successfully fixed")
            return
        fixed = True

    print("❌ Failed to fix fizzer")
    sys.exit(1)

    
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


def get_examples():
    # Use all our js_wasm testcases as initial examples.
    js_files = list(pathlib.Path(in_binaryen('test', 'js_wasm')).glob('*.mjs'))
    examples = []
    for js_file in js_files:
        examples.append(str(js_file))
        examples.append(str(pathlib.Path(js_file).with_suffix('.wat')))
    return examples


def generate_initial_fuzzer():
    prompt = INITIAL_GENERATION_PROMPT + bundle_files(get_examples())

    client = GeminiClient()
    response = client.one_off(prompt)
    write_fuzzer(response)

    validate_fuzzer()


# Improve the fuzzer in a single iteration of the main loop

def improve_fuzzer():
    print("💼 Improving fuzzer by doing ..?")
    4/0


# Main workflow.
def build_fuzzer():
    # Create the initial fuzzer, if there is none.
    if not os.path.exists(params.fuzzer_file):
        print("💼 Generating initial fuzzer")
        generate_initial_fuzzer()
        validate_fuzzer()
    else:
        print("💼 Improving existing fuzzer")

    # Iterately improve the fuzzer.
    try:
        for i in range(params.max_iters):
            print(f"⏱️  Improving fuzzer, iteration {i}")
            improve_fuzzer()
            validate_fuzzer()
    except KeyboardInterrupt:
        print("🛑 Stopping by user request.")


def do_fuzzing():
    print("💼 Fuzzing with the current fuzzer")
    total = 0
    errored = 0
    while 1:
        seed = random_seed()
        print(f"💼   seed: {seed}  erroring: {int(100 * errored / max(total, 1))}%")
        if ExecutionFixer(seed).test().returncode:
            errored += 1
        total += 1



def main():
    parser = argparse.ArgumentParser(description="SlopFuzz")
    parser.add_argument("--model", type=str, default="gemini-3-flash-preview", help="Model ID")
    parser.add_argument("--temperature", type=float, default=0.7, help="Creativity temperature")
    parser.add_argument("--fuzzer-file", type=str, required=True, help="File to write the fuzzer in (must be inside a git repo, as each successful update is committed)")
    parser.add_argument("--max-iters", type=int, default=1000, help="Maximum number of iterations to run")
    parser.add_argument("--save-history", default=False, action="store_true", help="Save history of prompts and fuzzers as we go, for debugging (uses the fuzzer-file with different suffixes)")
    parser.add_argument("--binaryen-bin", type=str, help="Directory with Binaryen binaries (wasm-opt)")
    parser.add_argument("--vm", type=str, required=True, help="VM to run the testcases in")
    parser.add_argument("--fuzz", default=False, action="store_true", help="Fuzz using the existing fuzzer, instead of building and improving a fuzzer")
    parser.add_argument("--verbose", default=False, action="store_true", help="Log very verbosely")

    global params
    params = parser.parse_args()
    
    print(f"📖 Using model {params.model}")

    if not params.fuzz:
        build_fuzzer()
    else:
        do_fuzzing()


if __name__ == "__main__":
    main()
