local esp32 = require "esp32"
local clock = esp32.clock
local Fps = require "ultrat.fps"

loop(function()
    print("======================================================")
    print("ESP32 LUA ATMOS RUNNING")
    print("======================================================")

    -- Spawn FPS counter
    spawn(Fps)

    -- Keep main loop alive
    await(false)
end)
