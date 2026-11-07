local atmos = require "atmos"
local esp = require "esp32"
local debug = esp.debug

spawn(debug.Mem)
spawn(debug.Fps)

loop(function()
    print("Running in pure Lua mode with lua-atmos runtime!")
    while true do
        await(1 * _s_)
        print("Tick from pure Lua...")
    end
end)
