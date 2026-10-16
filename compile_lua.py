import os
import shutil
import sys
import subprocess
import tempfile

def to_hex_escaped(binary_data):
    return "".join(f"\\x{b:02x}" for b in binary_data)

def compile_to_bytecode(lua_bin, cwd, file_path):
    with tempfile.NamedTemporaryFile(delete=False) as tmp:
        tmp_name = tmp.name
    try:
        lua_code = (
            f'local f = assert(loadfile([[{file_path}]])); '
            f'local dump = assert(string.dump(f)); '
            f'local out = assert(io.open([[{tmp_name}]], "wb")); '
            f'out:write(dump); '
            f'out:close()'
        )
        subprocess.run([lua_bin, "-e", lua_code], cwd=cwd, check=True)
        with open(tmp_name, "rb") as f:
            bytecode = f.read()
        return bytecode
    finally:
        if os.path.exists(tmp_name):
            os.remove(tmp_name)

def write_bytecode(dest_path, bytecode):
    os.makedirs(os.path.dirname(dest_path), exist_ok=True)
    with open(dest_path, "wb") as f:
        f.write(bytecode)

def get_module_name(prefix, rel_path):
    # Map relative path (e.g. "atmos/run.lua") to module name (e.g. "atmos.run")
    parts = rel_path.replace("\\", "/").split("/")
    if parts[-1].endswith(".lua"):
        parts[-1] = parts[-1][:-4]
    if parts == ["init"] and prefix:
        return prefix
    if parts[-1] == "init" and len(parts) > 1:
        parts.pop()
    if prefix:
        parts.insert(0, prefix)
    return ".".join(parts)

def parse_module_root(spec):
    if "=" in spec:
        prefix, path = spec.split("=", 1)
    else:
        prefix, path = "", spec
    return prefix, os.path.abspath(path)

def iter_lua_files(root):
    for dirpath, dirs, files in os.walk(root):
        dirs[:] = [d for d in dirs if d not in {".git", "old", "tst", "test", "tests", "exs"}]
        for file in files:
            if file.endswith(".lua"):
                abs_path = os.path.join(dirpath, file)
                rel_path = os.path.relpath(abs_path, root)
                yield rel_path, abs_path

def main():
    if len(sys.argv) < 4:
        print("Usage: python compile_lua.py <lua_bin> <dest_dir> <app_src_dir> [<module_prefix=module_src_dir> ...]")
        sys.exit(1)

    lua_bin = os.path.abspath(sys.argv[1])
    dest_dir = os.path.abspath(sys.argv[2])
    app_src_dir = os.path.abspath(sys.argv[3])
    module_roots = [parse_module_root(spec) for spec in sys.argv[4:]]

    if os.path.exists(dest_dir):
        shutil.rmtree(dest_dir)
    os.makedirs(dest_dir)

    preloads = []
    main_init_bytecode = None

    for rel_path, abs_path in iter_lua_files(app_src_dir):
        if rel_path == "init.lua":
            main_init_bytecode = compile_to_bytecode(lua_bin, app_src_dir, rel_path)
        else:
            bytecode = compile_to_bytecode(lua_bin, app_src_dir, rel_path)
            # Keep application modules as individual files in LittleFS. This lets
            # Lua's normal file searcher resolve require("foo") as /lua/foo.lua
            write_bytecode(os.path.join(dest_dir, rel_path), bytecode)

    for prefix, root in module_roots:
        for rel_path, abs_path in iter_lua_files(root):
            module_name = get_module_name(prefix, rel_path)
            bytecode = compile_to_bytecode(lua_bin, root, rel_path)
            preloads.append((module_name, os.path.join(prefix.replace(".", "/"), rel_path), bytecode))

    if main_init_bytecode is None:
        print("Error: init.lua not found in app source directory")
        sys.exit(1)

    # Build the bundle Lua script preloading modules as precompiled chunks
    bundle_lines = []
    for mod_name, rel_path, bytecode in preloads:
        escaped_bc = to_hex_escaped(bytecode)
        # Use load() with binary chunk and chunkname starting with '@' to preserve filenames in debug
        bundle_lines.append(f'package.preload["{mod_name}"] = assert(load("{escaped_bc}", "@{rel_path}"))')

    # Add the main entry point code running the compiled main script
    escaped_main = to_hex_escaped(main_init_bytecode)
    bundle_lines.append('require("atmos")')
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
        subprocess.run([lua_bin, "-e", lua_compile_code], cwd=app_src_dir, check=True)
        print(f"Amalgamated bundle successfully created at: {dest_init_lua}")
    except Exception as e:
        print(f"Failed to compile bundled init.lua: {e}")
        sys.exit(1)
    finally:
        if os.path.exists(bundle_tmp_path):
            os.remove(bundle_tmp_path)

if __name__ == "__main__":
    main()
