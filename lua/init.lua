local a = require "atmos.env.clock_esp"
loop(function()
    spawn(task(function()
        local t = 1
        loop_on(1 * _s_, function()
            print("T1 counter: " .. t)
            t = t + 1
        end)
    end))
    spawn(task(function()
        local t = 1
        loop_on(2 * _s_, function()
            print("T2 counter: " .. t)
            t = t + 1
        end)
    end))

    await(100 * _s_)
end)
