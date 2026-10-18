local Mem = task(function()
    loop_on(1 * _s_, function()
        local m = meminfo()
        print(string.format(
            "[MEM] Free Heap: %0.1f%% (%d KB) | Min Free: %0.1f%% | Stack Free: %d B",
            (m.heap_free / m.heap_total) * 100,
            math.floor(m.heap_free / 1024),
            (m.internal_min_free / m.internal_total) * 100,
            m.stack_free
        ))
    end)
end)

return Mem
