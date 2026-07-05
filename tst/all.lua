print "===== PROTO ====="
dofile "proto.lua"
collectgarbage()

print "===== TASK ====="
dofile "task.lua"
collectgarbage()

print "===== AWAIT ====="
dofile "await.lua"
collectgarbage()

print "===== X ====="
dofile "x.lua"
collectgarbage()

print "===== TOGGLE ====="
dofile "toggle.lua"
collectgarbage()

print "===== TASKS ====="
dofile "tasks.lua"
collectgarbage()

print "===== ABORT ====="
dofile "abort.lua"
collectgarbage()

print "===== OTHERS ====="
dofile "others.lua"
collectgarbage()

print "===== PAR ====="
dofile "par.lua"
collectgarbage()

-- print "===== ERRORS ====="
-- dofile "errors.lua"
-- collectgarbage()

print "===== STREAMS ====="
dofile "streams.lua"
collectgarbage()

print "===== ENVS ====="
dofile "envs.lua"
collectgarbage()
