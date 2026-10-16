local Fps = task(function()
    while true do
        local n = 0
        watching(1 * _s_, function()
            loop_on("clock", function()
                n = n + 1
            end)
        end)
        print("FPS:", n)
    end
end)

return Fps
