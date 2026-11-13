local atmos = require "atmos"
local _c = _esp.gpio

local M = {}

function M.setup(pin, mode)
    return _c.setup(pin, mode)
end
M.mode = M.setup

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

function M.setup_pwm(channel, freq, resolution)
    return _c.setup_pwm(channel, freq, resolution)
end

function M.attach_pwm(pin, channel, ...)
    return _c.attach_pwm(pin, channel, ...)
end

function M.detach_pwm(pin)
    return _c.detach_pwm(pin)
end

function M.stop_pwm(channel)
    return _c.stop_pwm(channel)
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
