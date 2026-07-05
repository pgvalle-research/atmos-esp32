import os
import shutil
import sys
import subprocess

def compile_file(lua_bin, src_dir, rel_file_path, dest_path):
    # Compile rel_file_path into dest_path via string.dump in the custom 32-bit interpreter
    lua_code = (
        f'local f = assert(loadfile([[{rel_file_path}]])); '
        f'local dump = assert(string.dump(f)); '
        f'local out = assert(io.open([[{dest_path}]], "wb")); '
        f'out:write(dump); '
        f'out:close()'
    )
    subprocess.run([lua_bin, "-e", lua_code], cwd=src_dir, check=True)

def main():
    if len(sys.argv) < 4:
        print("Usage: python compile_lua.py <lua_bin> <src_dir> <dest_dir>")
        sys.exit(1)

    lua_bin = os.path.abspath(sys.argv[1])
    src_dir = os.path.abspath(sys.argv[2])
    dest_dir = os.path.abspath(sys.argv[3])

    if os.path.exists(dest_dir):
        shutil.rmtree(dest_dir)
    os.makedirs(dest_dir)

    for root, dirs, files in os.walk(src_dir):
        rel_path = os.path.relpath(root, src_dir)
        if rel_path == ".":
            target_dir = dest_dir
        else:
            target_dir = os.path.join(dest_dir, rel_path)
            os.makedirs(target_dir, exist_ok=True)

        for file in files:
            src_file = os.path.join(root, file)
            dest_file = os.path.join(target_dir, file)

            if file.endswith(".lua"):
                try:
                    rel_file_path = os.path.relpath(src_file, src_dir)
                    compile_file(lua_bin, src_dir, rel_file_path, dest_file)
                    print(f"Compiled to bytecode: {rel_file_path}")
                except Exception as e:
                    print(f"Failed to compile {src_file}: {e}")
                    sys.exit(1)
            else:
                shutil.copy2(src_file, dest_file)

if __name__ == "__main__":
    main()
