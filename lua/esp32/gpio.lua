local atmos = require "atmos"
local _c = _esp.gpio

local M = {}

function M.mode(pin, mode, ...)
    if mode == "pwm" then
        local channel, freq, resolution = ...
        if not channel or not freq or not resolution then
            error("PWM mode requires: channel, frequency, resolution", 2)
        end
        return _c.setup_pwm(pin, channel, freq, resolution)
    else
        return _c.mode(pin, mode)
    end
end

function M.read(pin)
    return _c.read(pin)
end

function M.write(pin, value)
    return _c.write(pin, value)
end

function M.watch(pin, edge)
    return _c.watch(pin, edge or "change")
end

function M.unwatch(pin)
    return _c.unwatch(pin)
end

function M.read_adc(pin, atten)
    return _c.read_adc(pin, atten)
end

function M.write_pwm(channel, duty)
    return _c.write_pwm(channel, duty)
end

function M.fade_pwm(channel, target_duty, duration_ms)
    return _c.fade_pwm(channel, target_duty, duration_ms)
end

function M.step()
    while true do
        local ev = _c.next_event()
        if ev == nil then
            break
        end
        emit(ev)
    end
end

M.env = {
    step = M.step,
    mode = {
        secondary = true,
    },
}

atmos.env(M.env)

return M
