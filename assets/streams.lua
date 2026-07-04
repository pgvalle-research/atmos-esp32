local M = {}

M.mt = {
    __call  = function (t) return t:f() end,
    __close = function (t) if t.clo then t:clo() end end,
    __index = M,
}

function M.is (s)
    return (getmetatable(s) == M.mt)
end

local function close_s (t)
    local _ <close> = t.s
end

-------------------------------------------------------------------------------
-- SOURCES
-------------------------------------------------------------------------------

function M.from (v, ...)
    local multi = (select('#',...) > 0)
    if multi then
        assert(type(v)=='number' or M.is(v))
    end

    if v==nil or type(v)=='number' then
        return M.fr_range(v, ...)
    elseif M.is(v) then
        return M.fr_streams(v, ...)
    elseif type(v) == 'table' then
        return M.fr_table(v)
    elseif type(v) == 'function' then
        return M.fr_function(v)
    elseif type(v) == 'coroutine' then
        return M.fr_coroutine(v)
    else
        return M.fr_const(v)
    end
end

-------------------------------------------------------------------------------

local function fr_const (t)
    return t.v
end

function M.fr_const (v)
    local t = {
        v = v,
        f = fr_const,
    }
    return setmetatable(t, M.mt)
end

-------------------------------------------------------------------------------

local function fr_coroutine (t)
    return (function (ok, ...)
        assert(ok)
        if (... == nil) then
            return nil
        end
        return ...
    end)(coroutine.resume(t.co))
end

local function close (t)
    coroutine.close(t.co)
end

function M.fr_coroutine (co)
    local t = {
        co  = co,
        f   = fr_coroutine,
        clo = close,
    }
    return setmetatable(t, M.mt)
end

-------------------------------------------------------------------------------

function M.fr_counter (i)
    return M.fr_range(i)
end

-------------------------------------------------------------------------------

function M.fr_function (f)
    local t = {
        f = f,
    }
    return setmetatable(t, M.mt)
end

-------------------------------------------------------------------------------

local function fr_range (t)
    if t.b and t.a>t.b then
        return nil
    end
    local v = t.a
    t.a = t.a + 1
    return v
end

function M.fr_range (a, b)
    local t = {
        a = a or 1,
        b = b,
        f = fr_range,
    }
    return setmetatable(t, M.mt)
end

-------------------------------------------------------------------------------

function M.fr_streams (...)
    local n = select('#', ...)
    assert(n >= 2)
    local s = select(1, ...)
    for i=2, n do
        s = s:seq(select(i, ...))
    end
    return s
end

-------------------------------------------------------------------------------

local function fr_table (t)
    if t.i > #t.t then
        return nil
    end
    local v = t.t[t.i]
    t.i = t.i + 1
    return v
end

function M.fr_table (t)
    local t = {
        t = t,
        i = 1,
        f = fr_table,
    }
    return setmetatable(t, M.mt)
end

-------------------------------------------------------------------------------
-- COMBINATORS
-------------------------------------------------------------------------------

local function empty (t)
    return nil
end

function M.empty ()
    local t = {
        f = empty,
    }
    return setmetatable(t, M.mt)
end

-------------------------------------------------------------------------------

local function acc0 (t)
    if t.done then
        return nil
    end
    local cur = t.cur
    local v = t.s()
    if v == nil then
        t.done = true
    else
        t.cur = t.g(t.cur, v)
    end
    return cur
end

function M.acc0 (s, z, g)
    local t = {
        s    = s,
        g    = g,
        cur  = z,
        done = false,
        f    = acc0,
        clo  = close_s,
    }
    return setmetatable(t, M.mt)
end

-------------------------------------------------------------------------------

local function acc1 (t)
    local v = t.s()
    if v == nil then
        return nil
    end
    t.cur = t.g(t.cur, v)
    return t.cur
end

function M.acc1 (s, g)
    local t = {
        s   = s,
        g   = g,
        cur = nil,
        f   = acc1,
        clo = close_s,
    }
    return setmetatable(t, M.mt)
end

-------------------------------------------------------------------------------

local function filter (t)
    while true do
        local v = t.s()
        if v == nil then
            return nil
        end
        if t.g(v) then
            return v
        end
    end
end

function M.filter (s, g)
    local t = {
        s   = s,
        g   = g,
        f   = filter,
        clo = close_s,
    }
    return setmetatable(t, M.mt)
end

-------------------------------------------------------------------------------

local function skip (t)
    while t.n > 0 do
        if t.s() == nil then
            return nil
        end
        t.n = t.n - 1
    end
    return t.s()
end

function M.skip (s, n)
    local t = {
        s   = s,
        n   = n or 1,
        f   = skip,
        clo = close_s,
    }
    return setmetatable(t, M.mt)
end

-------------------------------------------------------------------------------

local function seq (t)
    local v = t.cur()
    if v == nil then
        if t.nxt == nil then
            return nil
        end
        t.cur = t.nxt
        t.nxt = nil
        v = t.cur()
    end
    return v
end

local function close (t)
    local _ <close> = t.cur
    local _ <close> = t.nxt
end

function M.seq (s1, s2)
    local t = {
        cur = s1,
        nxt = s2,
        f   = seq,
        clo = close,
    }
    return setmetatable(t, M.mt)
end

-------------------------------------------------------------------------------

local function take (t)
    if t.i >= t.n then
        return nil
    end
    t.i = t.i + 1
    return t.s()
end

function M.take (s, n)
    local t = {
        s   = s,
        i   = 0,
        n   = n,
        f   = take,
        clo = close_s,
    }
    return setmetatable(t, M.mt)
end

-------------------------------------------------------------------------------

local function tee2 (t)
    if #t.q1 == 0 then
        local v = t.s()
        if v ~= nil then
            table.insert(t.q1, v)
            table.insert(t.q2, v)
        end
    end
    if #t.q1 <= 0 then
        return nil
    end
    return table.remove(t.q1, 1)
end

function M.tee2 (s)
    local q1, q2 = {}, {}
    local t1 = {
        s   = s,
        q1  = q1,
        q2  = q2,
        f   = tee2,
        clo = close_s,
    }
    local t2 = {
        s   = s,
        q1  = q2,
        q2  = q1,
        f   = tee2,
        clo = close_s,
    }
    t1 = setmetatable(t1, M.mt)
    t2 = setmetatable(t2, M.mt)
    return t1, t2
end

function M.tee (s, n, ...)
    local fs = { n, ... }
    if n == nil then
        n = 2
        fs = nil
    elseif type(n) == 'number' then
        fs = nil
        assert(select('#',...) == 0)
    else
        n = #fs
        for _,f in ipairs(fs) do
            assert(type(f) == 'function')
        end
    end
    assert(n >= 1)
    local ss = { s }
    for i=2, n do
        local s1,s2 = M.tee2(ss[#ss])
        ss[#ss] = nil
        ss[#ss+1] = s1
        ss[#ss+1] = s2
    end

    if fs then
        for i,f in ipairs(fs) do
            ss[i] = f(ss[i])
        end
    end

    return table.unpack(ss)
end

-------------------------------------------------------------------------------

local function zip (t)
    local vs = {}
    for i,s in ipairs(t.ss) do
        local v = s()
        if v == nil then
            return nil
        end
        vs[i] = v
    end
    return vs
end

local function close (t)
    for _, s in ipairs(t.ss) do
        local _ <close> = s
    end
end

function M.zip (...)
    local t = {
        ss  = {...},
        f   = zip,
        clo = close,
    }
    return setmetatable(t, M.mt)
end

-------------------------------------------------------------------------------

function M.map (s, f)
    return M.acc1(s, function (_, v) return f(v) end)
end

function M.mapi (s, f)
    local i = 0
    return s:map(function (v)
        i = i + 1
        return f(i, v)
    end)
end

function M.max (s)
    return M.acc0(s, -math.huge, function(a,x) return math.max(a,x) end)
end

function M.min (s)
    return M.acc0(s, math.huge, function(a,x) return math.min(a,x) end)
end

function M.sum (s)
    return M.acc0(s, 0, function(a,x) return a+x end)
end

function M.table (s)
    return M.acc0(s, {}, function(a,v) a[#a+1]=v ; return a end)
end

function M.tap (s, f)
    return M.acc1(s, function (_, v)
        f(v)
        return v
    end)
end

-------------------------------------------------------------------------------
-------------------------------------------------------------------------------

local function xseq (t)
    if t.s == false then
        t.s = t.ss()
    end
    if t.s == nil then
        return nil
    end
    local v = t.s()
    while v == nil do
        t.s = t.ss()
        if t.s == nil then
            return nil
        end
        v = t.s()
    end
    return v
end

function M.xseq (ss)
    local t = {
        ss  = ss,
        s   = false,
        f   = xseq,
        clo = close_s,
    }
    return setmetatable(t, M.mt)
end

-------------------------------------------------------------------------------
-- SINKS
-------------------------------------------------------------------------------

function M.to_last (s)
    local s <close> = s
    local v = nil
    local x = s()
    while true do
        if x == nil then
            return v
        end
        v = x
        x = s()
    end
end

M.to = M.to_last

function M.to_first (s)
    return s()
end

-------------------------------------------------------------------------------

do
    function M.to_any (s, f)
        return (s:filter(f):to_first() ~= nil)
    end

    function M.to_some (s, f)
        return (s:filter(f):skip(1):to_first() ~= nil)
    end

    function M.to_all (s, f)
        return not (s:to_any(function(x) return not f(x) end))
    end

    function M.to_none (s, p)
        return not s:to_any(p)
    end
end

-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------

--[===[
function M.fr_const (v)
    return M.fr_consts(v):take(1)
end

local function distinct (t)
    local v = t.s()
    while true do
        if v == nil then
            return nil
        elseif not t.seen[v] then
            t.seen[v] = true
            return v
        end
        v = t.s()
    end
end

function M.distinct (s)
    local t = {
        s = s,
        seen = {},
        f = distinct,
    }
    return setmetatable(t, M.mt)
end

local function loop (t)
    local v = t.s()
    if v == nil then
        t.s = t.fs()
        v = t.s()
    end
    return v
end

function M.loop (fs)
    local t = {
        fs = fs,
        s  = fs(),
        f  = loop,
    }
    return setmetatable(t, M.mt)
end

function M.to_vector (s)
    return M.to_table(s)
end

local function tuple (t)
    return (function (...)
        if ... == nil then
            return nil
        end
        return { tag=t.tag, ... }
    end)(t.s())
end

function M.tuple (s, tag)
    local t = {
        s   = s,
        tag = tag,
        f   = tuple,
    }
    return setmetatable(t, M.mt)
end

function M.mul (s)
    return M.acc0(s, 1, function(a,x) return a*x end)
end

function M.sort (s, f)
    return s:map(function (t)
        table.sort(t, f)    -- TODO: insertion sort
        return t
    end)
end

function M.skip (s, n)
    return s:mapi(function(i, v)
        if i > n then
            return M.fr_consts(v):take(1)
        else
            return M.empty()
        end
    end):xseq()
end
]===]

return M
