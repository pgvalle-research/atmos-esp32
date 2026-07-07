local atmos = require "atmos"

local M = {
    now = 0,
}

local QUANTUM_US = 10000
local old = math.floor(os.clock() * 1000000)

function M.step()
    local now = math.floor(os.clock() * 1000000)
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
