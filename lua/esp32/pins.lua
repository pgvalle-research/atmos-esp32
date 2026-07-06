local atmos = require "atmos"

local M = {}

function M.mode(pin, mode)
    return _esp_pin_mode(pin, mode)
end

function M.read(pin)
    return _esp_pin_read(pin)
end

function M.write(pin, value)
    return _esp_pin_write(pin, value)
end

function M.toggle(pin)
    local value = M.read(pin) == 0 and 1 or 0
    M.write(pin, value)
    return value
end

function M.watch(pin, edge)
    return _esp_pin_watch(pin, edge or "change")
end

function M.unwatch(pin)
    return _esp_pin_unwatch(pin)
end

function M.step()
    while true do
        local ev = _esp_pin_next_event()
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
