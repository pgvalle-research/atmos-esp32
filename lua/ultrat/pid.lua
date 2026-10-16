local gpio = require "esp32.gpio"
local drv = require "ultrat.drv8833"
local pins = require "ultrat.pins"

local KP, KI, KD = 450, 0, 0
local MS = 10

local function read_sensors()
    return {
        gpio.read(pins.S5),
        gpio.read(pins.S3),
        gpio.read(pins.S2),
        gpio.read(pins.S4),
        gpio.read(pins.S6),
        gpio.read(pins.S8),
    }
end

local function calculate_error(readings)
    local weights = {-4, -2, 2, 4}
    local w_sum = 0
    local active = 0

    for i = 1, 4 do
        if readings[i] ~= 0 then
            w_sum = w_sum + weights[i]
            active = active + 1
        end
    end

    if active == 0 then
        return 0
    else
        return w_sum / active
    end
end

local Pid = task(function()
    local err_before = 0
    local i_before = 0
    local tgt_before = false

    while true do
        local ms = await("clock")
        local readings = read_sensors()
        local err = calculate_error(readings)

        local tgt = (err ~= 0)
        if tgt ~= tgt_before then
            emit({ tag = "TARGET", val = tgt })
        end

        local p = err
        local i = (err * ms) + i_before
        local d = (err - err_before) / ms
        local pid = (KP * p) + (KI * i) + (KD * d)
        emit({ tag = "PID", pid = pid, readings = readings })

        err_before = err
        i_before = i
        tgt_before = tgt
    end
end)

return Pid
