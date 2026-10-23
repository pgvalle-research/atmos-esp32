local M = {}

local active_instance = nil

local function wrap_instance(ud)
    local obj = {
        _ud = ud,
        _closed = false,
    }

    local function check_open()
        if obj._closed then
            error("Preferences instance is closed", 3)
        end
    end

    local function norm(first, ...)
        if first == obj then
            return ...
        else
            return first, ...
        end
    end

    function obj:put_int(...)
        check_open()
        local key, val = norm(...)
        if key == nil or val == nil then
            error("put_int requires (key, value)", 2)
        end
        return _esp_prefs_set_i32(obj._ud, tostring(key), math.floor(tonumber(val)))
    end

    function obj:get_int(...)
        check_open()
        local key, default = norm(...)
        if key == nil then
            error("get_int requires (key, [default])", 2)
        end
        local val = _esp_prefs_get_i32(obj._ud, tostring(key))
        if val == nil then
            return default ~= nil and default or 0
        end
        return val
    end

    function obj:put_bool(...)
        check_open()
        local key, val = norm(...)
        if key == nil or val == nil then
            error("put_bool requires (key, value)", 2)
        end
        return _esp_prefs_set_bool(obj._ud, tostring(key), not not val)
    end

    function obj:get_bool(...)
        check_open()
        local key, default = norm(...)
        if key == nil then
            error("get_bool requires (key, [default])", 2)
        end
        local val = _esp_prefs_get_bool(obj._ud, tostring(key))
        if val == nil then
            return default ~= nil and default or false
        end
        return val
    end

    function obj:put_string(...)
        check_open()
        local key, val = norm(...)
        if key == nil or val == nil then
            error("put_string requires (key, value)", 2)
        end
        return _esp_prefs_set_str(obj._ud, tostring(key), tostring(val))
    end

    function obj:get_string(...)
        check_open()
        local key, default = norm(...)
        if key == nil then
            error("get_string requires (key, [default])", 2)
        end
        local val = _esp_prefs_get_str(obj._ud, tostring(key))
        if val == nil then
            return default ~= nil and default or ""
        end
        return val
    end

    function obj:is_key(...)
        check_open()
        local key = norm(...)
        if key == nil then
            error("is_key requires (key)", 2)
        end
        return _esp_prefs_is_key(obj._ud, tostring(key))
    end

    function obj:remove(...)
        check_open()
        local key = norm(...)
        if key == nil then
            error("remove requires (key)", 2)
        end
        return _esp_prefs_remove(obj._ud, tostring(key))
    end

    function obj:clear(...)
        check_open()
        return _esp_prefs_clear(obj._ud)
    end

    function obj:commit(...)
        check_open()
        return _esp_prefs_commit(obj._ud)
    end

    function obj:close(...)
        if not obj._closed then
            _esp_prefs_close(obj._ud)
            obj._closed = true
            if active_instance == obj then
                active_instance = nil
            end
        end
        return true
    end

    -- Arduino Preferences aliases
    obj.putInt    = obj.put_int
    obj.getInt    = obj.get_int
    obj.putBool   = obj.put_bool
    obj.getBool   = obj.get_bool
    obj.putString = obj.put_string
    obj.getString = obj.get_string
    obj.isKey     = obj.is_key
    obj["end"]    = obj.close

    return obj
end

function M.open(namespace, readonly, partition)
    if not namespace then
        error("Preferences.open/begin requires a namespace name", 2)
    end
    local ud = _esp_prefs_open(tostring(namespace), readonly or false, partition)
    local inst = wrap_instance(ud)
    active_instance = inst
    return inst
end

M.begin = M.open

local function get_active()
    if not active_instance or active_instance._closed then
        error("No Preferences namespace is currently open. Call preferences.begin('namespace') first.", 3)
    end
    return active_instance
end

-- Module-level functions delegating to active_instance
function M.put_int(key, val) return get_active():put_int(key, val) end
function M.get_int(key, default) return get_active():get_int(key, default) end
function M.put_bool(key, val) return get_active():put_bool(key, val) end
function M.get_bool(key, default) return get_active():get_bool(key, default) end
function M.put_string(key, val) return get_active():put_string(key, val) end
function M.get_string(key, default) return get_active():get_string(key, default) end
function M.is_key(key) return get_active():is_key(key) end
function M.remove(key) return get_active():remove(key) end
function M.clear() return get_active():clear() end
function M.commit() return get_active():commit() end
function M.close()
    if active_instance then
        return active_instance:close()
    end
    return true
end

-- Arduino Preferences aliases on module
M.putInt    = M.put_int
M.getInt    = M.get_int
M.putBool   = M.put_bool
M.getBool   = M.get_bool
M.putString = M.put_string
M.getString = M.get_string
M.isKey     = M.is_key
M["end"]    = M.close

return M
