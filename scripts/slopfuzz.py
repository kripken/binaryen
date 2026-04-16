#!/usr/bin/python3

import os
import argparse
import time
import logging

from google import genai
from google.genai import types


# --- Setup Logging ---
logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')
logger = logging.getLogger(__name__)


# Generic LLM client handling
class GeminiClient:
    def __init__(self, model, temperature, system_instruction):
        # Looks for GEMINI_API_KEY env var automatically.
        self.client = genai.Client()
        self.model = model
        self.temperature = temperature
        self.system_instruction = system_instruction
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
        
        response = self._execute_with_retry(
            self.client.models.generate_content,
            model=self.model,
            contents=prompt,
            config=config
        )
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
                config=config
            )

        response = self._execute_with_retry(
            self.chat_session.send_message,
            message=message
        )
        return response.text


# Global arguments from the user
args = None


# Generate the initial fuzzer using a few examples.
def generate_initial_fuzzer():
    print(f"💼 Generating Fuzzer not found: {args.path}", file=sys.stderr)


# Main workflow
def work():
    # Create the initial fuzzer, if there is none.
    if not os.path.exists(args.fuzzer_file):
        generate_initial_fuzzer()

    # Iterately improve the fuzzer.
    try:
        while True:
            improve_fuzzer()
    except KeyboardInterrupt:
        print('🛑 Stopping by user request.')
        break


def main():
    parser = argparse.ArgumentParser(description="SlopFuzz")
    parser.add_argument("--model", type=str, default="gemini-3-flash-preview", help="Model ID")
    parser.add_argument("--temperature", type=float, default=0.7, help="Creativity temperature")
    parser.add_argument("--fuzzer-file", type=str, help="File to write the fuzzer in (must be inside a git repo, as each successful update is committed)")

    global args
    args = parser.parse_args()

    work()


if __name__ == "__main__":
    main()
