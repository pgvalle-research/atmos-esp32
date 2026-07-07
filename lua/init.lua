local esp32 = require "esp32"
local clock, gpio = esp32.clock, esp32.gpio

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
    spawn(task(function()
        loop_on(5 * _s_, function()
            say_mem()
        end)
    end))

    -- Setup PWM on Pin 23 (LED pin), Channel 0, 5kHz, 13-bit resolution
    gpio.mode(23, "pwm", 0, 5000, 13)
    gpio.mode(22, "output")

    -- Periodic ADC reading on GPIO 34 (analog input) every 500ms
    spawn(task(function()
        while true do
            local val = gpio.read_adc(34)
            say("ADC Pin 34 raw value: " .. tostring(val))
            await(500 * _ms_)
        end
    end))

    -- Fade loop for LED
    spawn(task(function()
        while true do
            say("Fading LED up...")
            gpio.fade_pwm(0, 8191, 1000) -- Fade to 100% duty (8191) in 1000 ms
            local ev = await({ tag = "gpio_fade_pwm", channel = 0 })
            say(string.format("LED fade up complete (event time: %u)", ev.time))

            await(200 * _ms_)

            say("Fading LED down...")
            gpio.fade_pwm(0, 0, 1000) -- Fade to 0 duty in 1000 ms
            ev = await({ tag = "gpio_fade_pwm", channel = 0 })
            say(string.format("LED fade down complete (event time: %u)", ev.time))

            await(200 * _ms_)
        end
    end))

    await(10000000 * _s_)
end)
