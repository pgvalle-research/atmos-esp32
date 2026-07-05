local old_dofile = dofile
function dofile(filename)
    if not filename:match("^/") then
        filename = "/lua/tst/" .. filename
    end
    return old_dofile(filename)
end

package.path = package.path .. ";/lua/tst/?.lua"

local a = require "atmos.env.clock_esp"
dofile("all.lua")
