local function print_mem(label)
    local lua_mem = collectgarbage("count")
    local sys_mem = free_heap() / 1024.0
    print(string.format("[%s] Memory -> Lua: %.1f KB, System: %.1f KB", label, lua_mem, sys_mem))
end

print_mem("START")

print "===== PROTO ====="
dofile "proto.lua"
collectgarbage()
print_mem("PROTO")

print "===== TASK ====="
dofile "task.lua"
collectgarbage()
print_mem("TASK")

print "===== AWAIT ====="
dofile "await.lua"
collectgarbage()
print_mem("AWAIT")

print "===== X ====="
dofile "x.lua"
collectgarbage()
print_mem("X")

print "===== TOGGLE ====="
dofile "toggle.lua"
collectgarbage()
print_mem("TOGGLE")

print "===== TASKS ====="
dofile "tasks.lua"
collectgarbage()
print_mem("TASKS")

print "===== ABORT ====="
dofile "abort.lua"
collectgarbage()
print_mem("ABORT")

print "===== OTHERS ====="
dofile "others.lua"
collectgarbage()
print_mem("OTHERS")

print "===== PAR ====="
dofile "par.lua"
collectgarbage()
print_mem("PAR")

-- print "===== ERRORS ====="
-- dofile "errors.lua"
-- collectgarbage()
-- print_mem("ERRORS")

print "===== STREAMS ====="
dofile "streams.lua"
collectgarbage()
print_mem("STREAMS")

print "===== ENVS ====="
dofile "envs.lua"
collectgarbage()
print_mem("ENVS")
