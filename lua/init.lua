local clock = require "esp32.clock"
local pins = require "esp32.pins"

local function say(msg)
    print(string.format("[%0.3f] %s", clock.now / _s_, msg))
end

local function pct(part, total)
    if total == nil or total == 0 then
        return 0
    end
    return (part / total) * 100
end

local function say_mem()
    local m = meminfo()
    say(string.format(
        "mem heap=%.1f%% min=%.1f%% largest=%u internal=%.1f%% stack=%u",
        pct(m.heap_free, m.heap_total),
        pct(m.heap_min_free, m.heap_total),
        m.heap_largest_free,
        pct(m.internal_free, m.internal_total),
        m.stack_free
    ))
end

loop(function()
    local toggle = function(pin, ms)
        pins.mode(pin, "output")

        return function()
            while true do
                pins.write(pin, 1)
                await(ms * _ms_)
                pins.write(pin, 0)
                await(ms * _ms_)
            end
        end
    end

    spawn(task(function()
        loop_on(5 * _s_, function()
            say_mem()
        end)
    end))

    spawn(task(toggle(23, 500)))
    spawn(task(toggle(22, 1500)))

    await(10000000 * _s_)
end)
