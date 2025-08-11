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


if __name__ == "__main__":
    cmd = sys.argv[1]
    arg = sys.argv[2]

    if cmd == 'bugfind':
        prompt = '''
'''
        prompt += open(arg).read()
        subprocess.check_call(['python3', 'bundle.py', arg])
        do_prompt(prompt)
    else:
        print('invalid command')
        sys.exit(1)

