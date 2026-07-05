local atmos = require "atmos"

local M = {
    now = 0,
}

function M.step()
    local before = math.floor(os.clock() * 1000000)
    wait_clock()
    local after = math.floor(os.clock() * 1000000)
    local delta = after - before
    if delta > 0 then
        emit(delta)
        M.now = M.now + delta
    end
end

M.env = {
    step = M.step,
}

atmos.env(M.env)

return M
