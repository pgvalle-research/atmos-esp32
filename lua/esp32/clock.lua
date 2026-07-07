local atmos = require "atmos"

local M = {
    now = 0,
}

local QUANTUM_US = 10000
local old = _esp_get_time()

function M.step()
    local now = _esp_get_time()
    while now - old >= QUANTUM_US do
        M.now = M.now + QUANTUM_US
        old = old + QUANTUM_US
        emit(QUANTUM_US)
    end
end

M.env = {
    step = M.step,
    mode = {
        primary = true,
    },
}

atmos.env(M.env)

return M
