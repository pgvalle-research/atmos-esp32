local esp32 = require "esp32"
local clock = esp32.clock
local Fps = require "ultrat.fps"
local modes = require "ultrat.modes"

loop(function()
    print("======================================================")
    print("ESP32 LUA ATMOS RUNNING")
    print("======================================================")

    -- Spawn tasks
    spawn(Fps)
    spawn(Mem)
    spawn(modes.i_see_you)

    -- Keep main loop alive
    await(false)
end)
