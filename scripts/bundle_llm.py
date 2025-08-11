import os
import pathlib
import sys

# A list of directories and files to scan.
#
# Additional ones can be passed on the commandline, e.g.
#
#   python3 bundle_llm.py "test/gtest/*"
#
# Each entry is a dictionary specifying the path and the glob pattern to match files.
# - "path": The directory to search in.
# - "pattern": The pattern for files to include. '**/*' matches all files recursively.
DIRECTORIES_TO_BUNDLE = [
    {"path": "src", "pattern": "**/*.h"},
]

# --- Main Script ---

def bundle_files():
    """
    Gathers files from specified directories, combines them into a single
    text file, and formats them for use as context for an LLM.
    """
    bundled_file_count = 0
    print("🚀 Starting to bundle files into stdout...", file=sys.stderr)

    for config in DIRECTORIES_TO_BUNDLE:
        dir_path = pathlib.Path(config["path"])
        pattern = config["pattern"]

        # Check if the directory exists before trying to scan it
        if not dir_path.is_dir():
            print(f"⚠️  Warning: Directory '{dir_path}' not found. Skipping.", file=sys.stderr)
            continue

        print(f"\nScanning '{dir_path}' for files matching '{pattern}'...", file=sys.stderr)
        
        # Find all paths matching the pattern, filter for files, and sort for consistency
        found_files = sorted(p for p in dir_path.glob(pattern) if p.is_file())

        if not found_files:
            print("   -> ❌ No matching files found.", file=sys.stderr)
            sys.exit(1)

        for file_path in found_files:
            # Use as_posix() to ensure file paths use forward slashes for consistency
            path = file_path.as_posix()
            print(f"   -> Adding {path}", file=sys.stderr)
            
            try:
                content = file_path.read_text(encoding="utf-8")
                
                # Write the file path and content to the bundle
                print(f">>>> {path}\n")
                print(content)
                
                # Ensure there's a newline at the end of the content
                if not content.endswith('\n'):
                    print('\n')
                
                # Add an extra newline for clear separation between files
                print('\n')
                
                bundled_file_count += 1
                
            except Exception as e:
                print(f"   -> ❌ Error reading file {path}: {e}", file=sys.stderr)
                raise

    print(f"\n✅ Success! Bundled {bundled_file_count} files.", file=sys.stderr)

if __name__ == "__main__":
    for arg in sys.argv[1:]:
        parts = os.path.split(arg)
        entry = {"path": parts[0], "pattern": "**/" + parts[1]}
        print(f"🚀 Adding directory entry {entry}", file=sys.stderr)
        DIRECTORIES_TO_BUNDLE.append(entry)

    bundle_files()
