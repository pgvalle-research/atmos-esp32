local atmos = require "atmos"

local M = {}

function M.start(pin, options)
    return _esp_ir_start(pin, options)
end

function M.stop()
    return _esp_ir_stop()
end

function M.stats()
    return _esp_ir_stats()
end

function M.step()
    while true do
        local ev = _esp_ir_next_event()
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
