local modes = {}

local Pid = require "ultrat.pid"
local drv = require "ultrat.drv8833"

modes.i_see_you = task(function()
    spawn(Pid)
    while true do
        await({ tag = "TARGET", val = true })
        local tsk = spawn(task(function()
            while true do
                local ev = await({ tag = "PID" })
                if ev.pid == 0 then
                    drv.move(1023, 1023)
                else
                    drv.move(ev.pid, -ev.pid)
                end
            end
        end))
        await({ tag = "TARGET", val = false })
        abort(tsk)
        drv.stop()
    end
end)

modes.calibration = task(function()
    spawn(Pid)
    while true do
        await({ tag = "TARGET", val = true })
        local tsk = spawn(task(function()
            while true do
                local ev = await({ tag = "PID" })
                if ev.pid == 0 then
                    drv.stop()
                else
                    drv.move(-ev.pid, ev.pid)
                end
            end
        end))
        await({ tag = "TARGET", val = false })
        abort(tsk)
        drv.stop()
    end
end)

modes.backward = task(function()
    await(drv.Move_for(1023, 1023, 500 * _ms_))
    await(drv.Move_for(-1023, 1023, 350 * _ms_))
    await(modes.i_see_you())
end)

modes.whiplash = task(function()
    await(drv.Move_for(1023, 1023, 300 * _ms_))
    await(drv.Move_for(1023, -1023, 50 * _ms_))
    await(modes.i_see_you())
end)

modes.sharingan = task(function()
    local choice = math.random(1, 2)
    if choice == 1 then
        drv.move(-900, 900)
    else
        drv.move(900, -900)
    end

    await(150 * _ms_)
    await(modes.whiplash())
end)

return modes
