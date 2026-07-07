local atmos = require "atmos"

local M = {}

function M.mode(pin, mode, ...)
    if mode == "pwm" then
        local channel, freq, resolution = ...
        if not channel or not freq or not resolution then
            error("PWM mode requires: channel, frequency, resolution", 2)
        end
        return _esp_gpio_setup_pwm(pin, channel, freq, resolution)
    else
        return _esp_gpio_mode(pin, mode)
    end
end

function M.read(pin)
    return _esp_gpio_read(pin)
end

function M.write(pin, value)
    return _esp_gpio_write(pin, value)
end

function M.watch(pin, edge)
    return _esp_gpio_watch(pin, edge or "change")
end

function M.unwatch(pin)
    return _esp_gpio_unwatch(pin)
end

function M.read_adc(pin, atten)
    return _esp_gpio_read_adc(pin, atten)
end

function M.write_pwm(channel, duty)
    return _esp_gpio_write_pwm(channel, duty)
end

function M.fade_pwm(channel, target_duty, duration_ms)
    return _esp_gpio_fade_pwm(channel, target_duty, duration_ms)
end

function M.step()
    while true do
        local ev = _esp_gpio_next_event()
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
