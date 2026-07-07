local atmos = require "atmos"

local M = {}

function M.setup(pin, channel, freq, resolution)
    return _esp_pwm_setup(pin, channel, freq, resolution)
end

function M.write(channel, duty)
    return _esp_pwm_write(channel, duty)
end

function M.fade(channel, target_duty, duration_ms)
    return _esp_pwm_fade(channel, target_duty, duration_ms)
end

function M.step()
    while true do
        local ev = _esp_pwm_next_event()
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
