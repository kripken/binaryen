#!/usr/bin/python3

import argparse
import logging
import os
import pathlib
import random
import subprocess
import time

from google import genai
from google.genai import types


# Binaryen paths

my_dir = os.path.dirname(os.path.abspath(__file__))
binaryen_root = os.path.dirname(my_dir)


def in_binaryen(*args):
    return os.path.join(binaryen_root, *args)


def in_bin(tool):
    return os.path.join(args.binaryen_bin or in_binaryen('bin'), tool)


# Global arguments from the user
args = None


# Logging

logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
logger = logging.getLogger(__name__)

prompt_index = 0

def log_prompt_or_response(contents, what):
    if not args.prompt_history_dir:
        return

    global prompt_index
    # New prompts bump the index (responses do not).
    if what == 'prompt':
        prompt_index += 1
    filename = os.path.join(args.prompt_history_dir, f'{what}-{prompt_index}.txt')
    open(filename, 'w').write(contents)


def log_prompt(contents):
    return log_prompt_or_response(contents, 'prompt')


def log_response(contents):
    return log_prompt_or_response(contents, 'response')


# LLM client handling

SYSTEM_INSTRUCTION = '''
You are a software engineer that knows Python, with experience with fuzzers as
a way to find bugs by generating random testcases.
'''


class GeminiClient:
    def __init__(self):
        # Looks for GEMINI_API_KEY env var automatically.
        self.client = genai.Client()
        self.model = args.model
        self.temperature = args.temperature
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

    def generate_single(self, prompt):
        """Standard one-off prompt/response."""
        config = types.GenerateContentConfig(
            system_instruction=self.system_instruction,
            temperature=self.temperature,
        )

        log_prompt(prompt)

        response = self._execute_with_retry(
            self.client.models.generate_content,
            model=self.model,
            contents=prompt,
            config=config,
        )

        log_response(response.text)

        return response.text

    def send_chat(self, message):
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

        log_prompt(message)

        response = self._execute_with_retry(
            self.chat_session.send_message,
            message=message,
        )

        log_response(response.text)

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
    open(args.fuzzer_file, 'w').write(response)


# Generate a random seed for the fuzzer
def random_seed():
    return random.randint(0, 1 << 64)


# The fuzzer's output has both JS and wasm, JS first, then this separator before
# the wasm:
JS_WAT_SEP = '>>>> wat'


# Run the fuzzer on a seed. Returns XXX splits
def run_fuzzer(seed):
    cmd = [args.fuzzer_file, str(seed)]
    output = subprocess.check_output(cmd, text=True)
    XXX


# Fix the fuzzer after changes (which might have broken it)

NUM_VALIDATIONS = 100


def fix_fuzzer():
    # Check we do not crash when generating testcases.
    for _ in range(NUM_VALIDATIONS):
        seed = random_seed()
        try:
            run_fuzzer(seed)
        except subprocess.CalledProcessError:
            
            raise
    # Check different numbers lead to different outputs.
    # Check the testcases parse
    # Check one (1) testcase runs
    2/0


# Generate the initial fuzzer

FUZZER_GOALS = f'''

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

INITIAL_GENERATION_PROMPT = '''
Write a fuzzer in Python that generates things similar to the examples below.

The fuzzer should take a single commandline parameter, and use that number to
deterministically generate a testcase (this determinism makes it easy to debug
the fuzzer itself). How it generates the testcase given a number is arbitrary
and you can do anything you want, but try to do well on the goals below.

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
    response = client.generate_single(prompt)
    write_fuzzer(response)

    fix_fuzzer()


# Improve the fuzzer in a single iteration

def improve_fuzzer():
    print("💼 Improving fuzzer")

    # client = GeminiClient()
    # client.generate_single(prompt)

    fix_fuzzer()


# Main workflow.
def work():
    # Create the initial fuzzer, if there is none.
    if not os.path.exists(args.fuzzer_file):
        generate_initial_fuzzer()

    # Iterately improve the fuzzer.
    try:
        for i in range(args.max_iters):
            print(f"⏱️ Improving fuzzer, iteration {i}")
            improve_fuzzer()
    except KeyboardInterrupt:
        print("🛑 Stopping by user request.")


def main():
    parser = argparse.ArgumentParser(description="SlopFuzz")
    parser.add_argument("--model", type=str, default="gemini-3-flash-preview", help="Model ID")
    parser.add_argument("--temperature", type=float, default=0.7, help="Creativity temperature")
    parser.add_argument("--fuzzer-file", type=str, required=True, help="File to write the fuzzer in (must be inside a git repo, as each successful update is committed)")
    parser.add_argument("--max-iters", type=int, default=1000, help="Maximum number of iterations to run")
    parser.add_argument("--prompt-history-dir", type=str, help="Directory to store the full history of prompts and responses (for debugging)")
    parser.add_argument("--binaryen-bin", type=str, help="Directory with Binaryen binaries (wasm-opt)")

    global args
    args = parser.parse_args()

    work()


if __name__ == "__main__":
    main()
