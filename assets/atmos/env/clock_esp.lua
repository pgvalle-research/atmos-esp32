local atmos = require "atmos"

local M = {
    now = 0,
}

local old = micros()

function M.step ()
    local now = micros()
    if now > old then
        emit(now - old)
        M.now = now
        old = now
    end
    delay(1) -- Yield to FreeRTOS for 1 millisecond to prevent watchdog timeout
end

M.env = {
    step = M.step,
}

atmos.env(M.env)

return M
