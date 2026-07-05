require "test" -- Preload test framework globals so they are not treated as leaked globals and cleaned up

local function print_mem(label)
    local lua_mem = collectgarbage("count")
    local sys_mem = free_heap() / 1024.0
    print(string.format("[%s] Memory -> Lua: %.1f KB, System: %.1f KB", label, lua_mem, sys_mem))
end

local function run_test(name)
    print("===== " .. name:upper() .. " =====")
    
    -- Save snapshot of global variable keys before running the test file
    local old_globals = {}
    for k in pairs(_G) do
        old_globals[k] = true
    end
    
    dofile(name .. ".lua")
    
    -- Clean up any global variables leaked or defined by the test file
    for k in pairs(_G) do
        if not old_globals[k] then
            _G[k] = nil
        end
    end
    
    collectgarbage()
    collectgarbage() -- Run twice to ensure full collection of cycle dependencies
    
    print_mem(name:upper())
end

print_mem("START")

run_test("proto")
run_test("task")
run_test("await")
run_test("x")
run_test("toggle")
run_test("tasks")
run_test("abort")
run_test("others")
run_test("par")
-- run_test("errors") -- disabled on ESP32
run_test("streams")
run_test("envs")
