local atmos = require "atmos"
local _c = _esp.ir

local M = {}

function M.start(pin, options)
    return _c.start(pin, options)
end

function M.stop()
    return _c.stop()
end

function M.stats()
    return _c.stats()
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
