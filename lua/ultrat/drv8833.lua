local drv8833 = {}

local clk = require "esp32.clock"
local gpio = require "esp32.gpio"

-- led control channels
local LC1 = 0
local LC2 = 1
local RC1 = 2
local RC2 = 3

function drv8833.setup(l1, l2, r1, r2)
    gpio.mode(l1, "pwm", LC1, 25000, 10)
    gpio.mode(l2, "pwm", LC2, 25000, 10)
    gpio.mode(r1, "pwm", RC1, 25000, 10)
    gpio.mode(r2, "pwm", RC2, 25000, 10)
end

function drv8833.move(vl, vr)
    if vl >= 0 then
        gpio.write_pwm(LC1, math.min(vl, 1023))
        gpio.write_pwm(LC2, 0)
    else
        gpio.write_pwm(LC1, 0)
        gpio.write_pwm(LC2, math.min(-vl, 1023))
    end

    if vr >= 0 then
        gpio.write_pwm(RC1, math.min(vr, 1023))
        gpio.write_pwm(RC2, 0)
    else
        gpio.write_pwm(RC1, 0)
        gpio.write_pwm(RC2, math.min(-vr, 1023))
    end
end

function drv8833.stop()
    gpio.write_pwm(LC1, 1023)
    gpio.write_pwm(LC2, 1023)
    gpio.write_pwm(RC1, 1023)
    gpio.write_pwm(RC2, 1023)
end

drv8833.Move_for = task(function(vl, vr, t)
    drv8833.move(vl, vr)
    await(t)
    drv8833.stop()
end)

return drv8833
