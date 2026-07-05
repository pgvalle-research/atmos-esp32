local clock = require "atmos.env.clock_esp"

local function say(msg)
    print(string.format("[%0.3f] %s", clock.now / _s_, msg))
end

loop(function()
    say("start")

    say("await 250ms")
    await(250 * _ms_)
    say("after await")

    say("emit/await custom event")
    spawn(task(function()
        await(200 * _ms_)
        emit_in("parent", { tag = "button", pin = 0 })
    end))
    local button = await("button")
    say("got button pin=" .. button.pin)

    say("loop_on three ticks")
    local n = 0
    local ticker = spawn(task(function()
        loop_on(300 * _ms_, function(late)
            n = n + 1
            say("tick " .. n .. " late=" .. tostring(late))
            if n == 3 then
                _break_()
            end
        end)
    end))
    await(ticker)
    say("loop_on done")

    say("par_any race")
    local winner = par_any(
        function()
            await(200 * _ms_)
            return "fast"
        end,
        function()
            await(500 * _ms_)
            return "slow"
        end
    )
    say("par_any winner=" .. winner)

    say("par_all join")
    local a, b = par_all(
        function()
            await(120 * _ms_)
            return "A"
        end,
        function()
            await(180 * _ms_)
            return "B"
        end
    )
    say("par_all results=" .. a .. "," .. b)

    say("watching cancels body")
    watching(250 * _ms_,
        function()
            say("watching body start")
            await(1 * _s_)
            say("watching body should not print")
        end
    )
    say("watching done")

    say("nested task result")
    local ret = await(task(function()
        await(100 * _ms_)
        return "child-ret"
    end))
    say("nested task returned=" .. ret)

    say("done")
end)
