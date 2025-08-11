"""
Prompting tool with Binaryen integration.

Usage:

GOOGLE_API_KEY=... python prompt_llm.py COMMAND FILE

For example:

GOOGLE_API_KEY=... python prompt_llm.py bugfind src/passes/OptimizeInstructions.cpp

Setup: You may need

$ pip install google-generativeai
"""

import os
import shutil
import subprocess
import sys

print('importing...')

import google.generativeai as genai

print('configuring...')

key = os.getenv('GOOGLE_API_KEY')
genai.configure(api_key=key)

generation_config = {
    "temperature": 1,
    # "top_p": 0.95,
    # "top_k": 64,
    # "max_output_tokens": 65536/8192?
}

model_name = 'gemini-2.5-pro'

model = genai.GenerativeModel(model_name=model_name,
                              generation_config=generation_config)


def do_prompt(prompt, outfile, promptfile='b.txt'):
    print(f'Prompting {len(prompt)} bytes, stashed to {promptfile}, writing to {outfile}')
    open(promptfile, 'w').write(prompt)

    print('  generating...')
    response = model.generate_content(prompt, stream=True, request_options={"timeout": 600})

    with open(outfile, 'w') as f:
        f.write('')

    total = 0
    for chunk in response:
        try:
            text = chunk.text
        except:
            print('  warning: blocked or missing chunk')
            raise
            #text = '[ERROR: Blocked or missing chunk]'
        with open(outfile, 'a') as f:
            f.write(text)
        total += len(text)
        print(f'  ({total} bytes so far)')
    print()

    print(f'  wrote {total} bytes to {outfile}')


if __name__ == "__main__":
    cmd = sys.argv[1]
    arg = sys.argv[2]

    if cmd == 'bugfind':
        prompt = '''
'''
        prompt += open(arg).read()
        subprocess.check_call(['python3', 'bundle.py', arg])
        do_prompt(prompt, f'c.txt', promptfile=f'b.txt')
    else:
        print('invalid command')
        sys.exit(1)

