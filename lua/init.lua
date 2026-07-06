local clock = require "esp32.clock"
-- local pins = require "esp32.pins"

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
    local lua_kb = collectgarbage("count")
    say(string.format(
        "mem heap=%.1f%% min=%.1f%% largest=%u internal=%.1f%% stack=%u lua=%.1fKB",
        pct(m.heap_free, m.heap_total),
        pct(m.heap_min_free, m.heap_total),
        m.heap_largest_free,
        pct(m.internal_free, m.internal_total),
        m.stack_free,
        lua_kb
    ))
end

loop(function()
    -- pins.mode(2, "output")
    -- pins.mode(4, "input_pullup")
    -- pins.watch(4, "falling")

    spawn(task(function()
        loop_on(5 * _s_, function()
            say_mem()
        end)
    end))

    -- spawn(task(function()
    --     loop_on(500 * _ms_, function()
    --         local value = pins.toggle(2)
    --         say("pin 2 = " .. value)
    --     end)
    -- end))

    -- spawn(task(function()
    --     while true do
    --         local ev = await({ tag = "pin", pin = 4, edge = "falling" })
    --         say(string.format("pin %u %s value=%u time=%u", ev.pin, ev.edge, ev.value, ev.time))
    --     end
    -- end))

    spawn(task(function()
        local n = 0
        loop_on(1 * _s_, function()
            say("task 1: " .. n)
            n = n + 1
        end)
    end))

    spawn(task(function()
        local n = 0
        loop_on(2 * _s_, function()
            say("task 2: " .. n)
            n = n + 1
        end)
    end))

    await(10000000 * _s_)
end)
