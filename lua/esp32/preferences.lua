local _c = _esp.preferences

local M = {}
local active_instance = nil

function M.open(namespace, readonly, partition)
    if not namespace then
        error("Preferences.open/begin requires a namespace name", 2)
    end
    local inst = _c.open(tostring(namespace), readonly or false, partition)
    active_instance = inst
    return inst
end

M.begin = M.open

local function get_active()
    if not active_instance then
        error("No Preferences namespace is currently open. Call preferences.open('namespace') first.", 3)
    end
    return active_instance
end

function M.put_int(key, val, commit) return get_active():put_int(key, val, commit) end
function M.get_int(key, default) return get_active():get_int(key, default) end
function M.put_bool(key, val, commit) return get_active():put_bool(key, val, commit) end
function M.get_bool(key, default) return get_active():get_bool(key, default) end
function M.put_string(key, val, commit) return get_active():put_string(key, val, commit) end
function M.get_string(key, default) return get_active():get_string(key, default) end
function M.is_key(key) return get_active():is_key(key) end
function M.remove(key, commit) return get_active():remove(key, commit) end
function M.clear(commit) return get_active():clear(commit) end
function M.commit() return get_active():commit() end
function M.close()
    if active_instance then
        local res = active_instance:close()
        active_instance = nil
        return res
    end
    return true
end

return M
