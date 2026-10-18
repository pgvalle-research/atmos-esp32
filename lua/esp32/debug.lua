local atmos = require "atmos"

local M = {}

M.Fps = task(function()
    while true do
        local n = 0
        watching(1 * _s_, function()
            loop_on("clock", function()
                n = n + 1
            end)
        end)
        print("FPS:", n)
    end
end)

M.Mem = task(function()
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
end