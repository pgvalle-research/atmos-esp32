import os
import shutil
import sys
import subprocess
import tempfile

def to_hex_escaped(binary_data):
    return "".join(f"\\x{b:02x}" for b in binary_data)

def compile_to_bytecode(lua_bin, src_dir, rel_file_path):
    with tempfile.NamedTemporaryFile(delete=False) as tmp:
        tmp_name = tmp.name
    try:
        lua_code = (
            f'local f = assert(loadfile([[{rel_file_path}]])); '
            f'local dump = assert(string.dump(f)); '
            f'local out = assert(io.open([[{tmp_name}]], "wb")); '
            f'out:write(dump); '
            f'out:close()'
        )
        subprocess.run([lua_bin, "-e", lua_code], cwd=src_dir, check=True)
        with open(tmp_name, "rb") as f:
            bytecode = f.read()
        return bytecode
    finally:
        if os.path.exists(tmp_name):
            os.remove(tmp_name)

def get_module_name(rel_path):
    # Map relative path (e.g. "atmos/run.lua") to module name (e.g. "atmos.run")
    parts = rel_path.replace("\\", "/").split("/")
    if parts[-1].endswith(".lua"):
        parts[-1] = parts[-1][:-4]
    if parts[-1] == "init" and len(parts) > 1:
        parts.pop()
    return ".".join(parts)

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

    preloads = []
    main_init_bytecode = None

    for root, dirs, files in os.walk(src_dir):
        rel_path = os.path.relpath(root, src_dir)
        for file in files:
            if not file.endswith(".lua"):
                continue
            src_file = os.path.join(root, file)
            rel_file_path = os.path.relpath(src_file, src_dir)

            if rel_file_path == "init.lua":
                # Compile main entrypoint bytecode
                main_init_bytecode = compile_to_bytecode(lua_bin, src_dir, rel_file_path)
            else:
                module_name = get_module_name(rel_file_path)
                bytecode = compile_to_bytecode(lua_bin, src_dir, rel_file_path)
                preloads.append((module_name, rel_file_path, bytecode))

    if main_init_bytecode is None:
        print("Error: init.lua not found in source directory")
        sys.exit(1)

    # Build the bundle Lua script preloading modules as precompiled chunks
    bundle_lines = []
    for mod_name, rel_path, bytecode in preloads:
        escaped_bc = to_hex_escaped(bytecode)
        # Use load() with binary chunk and chunkname starting with '@' to preserve filenames in debug
        bundle_lines.append(f'package.preload["{mod_name}"] = assert(load("{escaped_bc}", "@{rel_path}"))')

    # Add the main entry point code running the compiled main script
    escaped_main = to_hex_escaped(main_init_bytecode)
    bundle_lines.append(f'assert(load("{escaped_main}", "@init.lua"))()')

    bundle_code = "\n".join(bundle_lines)

    # Write the bundle code to a temporary file on the host
    bundle_tmp_path = os.path.join(dest_dir, "init_bundle.tmp")
    with open(bundle_tmp_path, "w") as f:
        f.write(bundle_code)

    dest_init_lua = os.path.join(dest_dir, "init.lua")

    # Compile the bundle code into final single init.lua bytecode in the destination directory
    try:
        lua_compile_code = (
            f'local f = assert(io.open([[{bundle_tmp_path}]], "r")); '
            f'local code = f:read("*a"); '
            f'f:close(); '
            f'local chunk = assert(load(code, "@init.lua")); '
            f'local dump = assert(string.dump(chunk)); '
            f'local out = assert(io.open([[{dest_init_lua}]], "wb")); '
            f'out:write(dump); '
            f'out:close()'
        )
        subprocess.run([lua_bin, "-e", lua_compile_code], cwd=src_dir, check=True)
        print(f"Amalgamated bundle successfully created at: {dest_init_lua}")
    except Exception as e:
        print(f"Failed to compile bundled init.lua: {e}")
        sys.exit(1)
    finally:
        if os.path.exists(bundle_tmp_path):
            os.remove(bundle_tmp_path)

if __name__ == "__main__":
    main()
