local atmos = require "atmos"

local M = {}

function M.init()
    return _esp_espnow_init()
end

function M.add_peer(mac)
    return _esp_espnow_add_peer(mac)
end

function M.send(mac, data)
    _esp_espnow_send(mac, data)
end

function M.send_reliable(mac, data)
    _esp_espnow_send(mac, data)
    local ev = await({ tag = "espnow_send", mac = mac })
    return (ev.status == "success")
end

function M.get_mac()
    return _esp_espnow_get_mac()
end

function M.step()
    while true do
        local ev = _esp_espnow_next_event()
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
