local gpio = require "esp32.gpio"
local drv = require "ultrat.drv8833"
local pins = require "ultrat.pins"

local KP, KI, KD = 450, 0, 0
local MS = 10

local WEIGHTS = {-4, -2, 2, 4}
local sensor_readings = {0, 0, 0, 0, 0, 0}
local pid_event = { tag = "PID", pid = 0, readings = sensor_readings }
local target_event = { tag = "TARGET", val = false }

local initialized = false
local function init_sensors()
    if not initialized then
        gpio.mode(pins.S5, "input")
        gpio.mode(pins.S3, "input")
        gpio.mode(pins.S2, "input")
        gpio.mode(pins.S4, "input")
        gpio.mode(pins.S6, "input")
        gpio.mode(pins.S8, "input")
        initialized = true
    end
end

local function read_sensors()
    init_sensors()
    sensor_readings[1] = gpio.read(pins.S5)
    sensor_readings[2] = gpio.read(pins.S3)
    sensor_readings[3] = gpio.read(pins.S2)
    sensor_readings[4] = gpio.read(pins.S4)
    sensor_readings[5] = gpio.read(pins.S6)
    sensor_readings[6] = gpio.read(pins.S8)
    return sensor_readings
end

local function calculate_error(readings)
    local w_sum = 0
    local active = 0

    for i = 1, 4 do
        if readings[i] ~= 0 then
            w_sum = w_sum + WEIGHTS[i]
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
            target_event.val = tgt
            emit(target_event)
        end

        local p = err
        local i = (err * ms) + i_before
        local d = (err - err_before) / ms
        local pid = (KP * p) + (KI * i) + (KD * d)

        pid_event.pid = pid
        emit(pid_event)

        err_before = err
        i_before = i
        tgt_before = tgt
    end
end)

return Pid
