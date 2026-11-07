local atmos = require "atmos"
local _c = _esp.espnow

local M = {}

function M.init()
    return _c.init()
end

function M.add_peer(mac)
    return _c.add_peer(mac)
end

function M.send(mac, data)
    _c.send(mac, data)
end

function M.get_mac()
    return _c.get_mac()
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
