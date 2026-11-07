import os
import shutil
import sys
import subprocess

def parse_module_root(spec):
    if "=" in spec:
        prefix, path = spec.split("=", 1)
    else:
        prefix, path = "", spec
    return prefix, os.path.abspath(path)

def iter_source_files(root, exts):
    for dirpath, dirs, files in os.walk(root):
        dirs[:] = [d for d in dirs if d not in {".git", "old", "tst", "test", "tests", "exs"}]
        for f in files:
            if any(f.endswith(ext) for ext in exts):
                abs_p = os.path.join(dirpath, f)
                rel_p = os.path.relpath(abs_p, root)
                yield rel_p, abs_p

def main():
    if len(sys.argv) < 4:
        print("Usage: python compile_lua.py <lua_bin> <dest_dir> <app_src_dir> [<module_prefix=module_src_dir> ...]")
        sys.exit(1)

    lua_bin = os.path.abspath(sys.argv[1])
    dest_dir = os.path.abspath(sys.argv[2])
    app_src_dir = os.path.abspath(sys.argv[3])
    module_roots = [parse_module_root(spec) for spec in sys.argv[4:] if spec.strip()]

    atmos_src_dir = None
    lua_atmos_dir = None
    f_streams_dir = None
    module_dirs = []

    for prefix, path in module_roots:
        if prefix == "atmos-src":
            atmos_src_dir = path
        else:
            if prefix == "atmos":
                lua_atmos_dir = path
            elif prefix == "streams":
                f_streams_dir = path
            module_dirs.append((prefix, path))

    if os.path.exists(dest_dir):
        shutil.rmtree(dest_dir)
    os.makedirs(dest_dir, exist_ok=True)

    # Collect all compilation jobs: (kind, src_path, dest_path, chunk_name)
    jobs = []
    seen_dst = set()

    def add_job(kind, src, dst, name):
        if dst not in seen_dst:
            seen_dst.add(dst)
            jobs.append((kind, src, dst, name))

    # 1. Application sources (.atm and .lua)
    has_init = False
    for rel_p, abs_p in iter_source_files(app_src_dir, (".atm", ".lua")):
        is_init = (rel_p in ("init.atm", "init.lua"))
        if is_init:
            has_init = True
        if rel_p.endswith(".atm"):
            dst_p = os.path.join(dest_dir, rel_p[:-4] + ".lua")
            kind = "atm_init" if is_init else "atm"
        else:
            dst_p = os.path.join(dest_dir, rel_p)
            kind = "lua_init" if is_init else "lua"
        add_job(kind, abs_p, dst_p, "@" + rel_p)

    if not has_init:
        print("Error: init.atm or init.lua not found in app source directory", file=sys.stderr)
        sys.exit(1)

    # 2. Atmos language runtime (atmos.lang.run)
    if atmos_src_dir:
        run_lua = os.path.join(atmos_src_dir, "run.lua")
        if os.path.exists(run_lua):
            add_job("lua", run_lua, os.path.join(dest_dir, "atmos", "lang", "run.lua"), "@atmos/lang/run.lua")

    # 3. External modules (atmos, streams, esp32)
    for prefix, root in module_dirs:
        for rel_p, abs_p in iter_source_files(root, (".lua",)):
            dst_p = os.path.join(dest_dir, prefix, rel_p)
            add_job("lua", abs_p, dst_p, f"@{prefix}/{rel_p}")

    # Pre-create all destination directories
    for _, _, dst_p, _ in jobs:
        os.makedirs(os.path.dirname(dst_p), exist_ok=True)

    # Build Lua batch compiler script
    lua_script = []
    if atmos_src_dir:
        lua_atmos_parent = os.path.dirname(lua_atmos_dir) if lua_atmos_dir else ""
        f_streams_parent = os.path.dirname(f_streams_dir) if f_streams_dir else ""
        lua_script.append(f"""
package.path = package.path .. ';{atmos_src_dir}/?.lua;{atmos_src_dir}/?/init.lua;{lua_atmos_parent}/?.lua;{lua_atmos_parent}/?/init.lua;{f_streams_parent}/?.lua;{f_streams_parent}/?/init.lua'
table.insert(package.searchers or package.loaders, 1, function(name)
  local prefix = 'atmos.lang.'
  if name:sub(1, #prefix) == prefix then
    local sub = name:sub(#prefix + 1)
    local path = '{atmos_src_dir}/' .. sub:gsub('%.', '/') .. '.lua'
    local f = loadfile(path)
    if f then return f end
  end
end)
require 'atmos.lang.exec'
""")

    lua_script.append("local jobs = {")
    for kind, src, dst, name in jobs:
        lua_script.append(f"  {{ [==[{kind}]==], [==[{src}]==], [==[{dst}]==], [==[{name}]==] }},")
    lua_script.append("}")

    lua_script.append("""
for _, job in ipairs(jobs) do
  local kind, src, dst, name = job[1], job[2], job[3], job[4]
  local f = assert(io.open(src, "rb"), "Cannot open " .. src)
  local content = f:read("*a")
  f:close()

  local chunk
  if kind == "atm" or kind == "atm_init" then
    local src_code = '(func (...) { ' .. content .. '\\n})(...)'
    local lua_code = assert(atm_to_lua(src, src_code))
    if kind == "atm_init" then
      lua_code = 'require "atmos.lang.run"\\nrequire "atmos"\\nloop(function(...)\\n' .. lua_code .. '\\nend)\\n'
    end
    chunk = assert(load(lua_code, name))
  elseif kind == "lua_init" and atm_to_lua then
    local lua_code = 'require "atmos.lang.run"\\nrequire "atmos"\\nloop(function(...)\\n' .. content .. '\\nend)\\n'
    chunk = assert(load(lua_code, name))
  else
    chunk = assert(load(content, name))
  end

  local dump = assert(string.dump(chunk))
  local out = assert(io.open(dst, "wb"), "Cannot write " .. dst)
  out:write(dump)
  out:close()
  print("Compiled: " .. name)
end
""")

    script_str = "\n".join(lua_script)
    try:
        subprocess.run([lua_bin, "-"], input=script_str.encode("utf-8"), check=True)
    except subprocess.CalledProcessError as e:
        print(f"Error compiling Lua/Atmos scripts: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
