import os
import pathlib
import sys

def bundle_files(files):
    bundled_file_count = 0
    print("🚀 Bundling files...", file=sys.stderr)

    for filename in files:
        # Use as_posix() to ensure file paths use forward slashes for consistency
        path = pathlib.PurePath(filename).as_posix()
        print(f"   -> Adding {path}", file=sys.stderr)
        
        try:
            content = open(filename).read()
            
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
    bundle_files(sys.argv[1:])

