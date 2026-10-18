local esp32 = require "esp32"
local clock = esp32.clock
local debug = esp32.debug
local modes = require "ultrat.modes"

loop(function()
    print("======================================================")
    print("ESP32 LUA ATMOS RUNNING")
    print("======================================================")

    -- Spawn tasks
    spawn(debug.Fps)
    spawn(debug.Mem)
    spawn(modes.i_see_you)

    -- Keep main loop alive
    await(false)
end)
