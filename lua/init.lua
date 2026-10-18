local esp32 = require "esp32"
local clock = esp32.clock
local Fps = require "ultrat.fps"
local modes = require "ultrat.modes"

local Mem = task(function()
    loop_on(1 * _s_, function()
        local m = meminfo()
        print(string.format(
            "[MEM] Free Heap: %0.1f%% (%d KB) | Min Free: %0.1f%% | Stack Free: %d B",
            (m.heap_free / m.heap_total) * 100,
            math.floor(m.heap_free / 1024),
            (m.internal_min_free / m.internal_total) * 100,
            m.stack_free
        ))
    end)
end)

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
