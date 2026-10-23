#include "bindings.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_err.h"
#include "lua.h"
#include "lauxlib.h"

#define PREFS_METATABLE "esp32.preferences_handle"

typedef struct {
    nvs_handle_t handle;
    bool is_open;
} lua_prefs_t;

static lua_prefs_t *check_prefs(lua_State *L, int idx) {
    lua_prefs_t *prefs = (lua_prefs_t *)luaL_checkudata(L, idx, PREFS_METATABLE);
    if (!prefs->is_open) {
        luaL_error(L, "Preferences handle is closed");
    }
    return prefs;
}

static const char *check_key(lua_State *L, int idx) {
    const char *key = luaL_checkstring(L, idx);
    size_t len = strlen(key);
    luaL_argcheck(L, len > 0 && len < 16, idx, "Key length must be between 1 and 15 characters");
    return key;
}

static int l_prefs_open(lua_State *L) {
    const char *ns = luaL_checkstring(L, 1);
    size_t len = strlen(ns);
    luaL_argcheck(L, len > 0 && len < 16, 1, "Namespace length must be between 1 and 15 characters");

    bool readonly = lua_toboolean(L, 2);
    const char *part = luaL_optstring(L, 3, NULL);

    nvs_handle_t handle;
    esp_err_t err;
    nvs_open_mode_t open_mode = readonly ? NVS_READONLY : NVS_READWRITE;

    if (part != NULL) {
        err = nvs_open_from_partition(part, ns, open_mode, &handle);
    } else {
        err = nvs_open(ns, open_mode, &handle);
    }

    if (err != ESP_OK) {
        return luaL_error(L, "Failed to open Preferences namespace '%s': %s", ns, esp_err_to_name(err));
    }

    lua_prefs_t *prefs = (lua_prefs_t *)lua_newuserdatauv(L, sizeof(lua_prefs_t), 0);
    prefs->handle = handle;
    prefs->is_open = true;

    luaL_getmetatable(L, PREFS_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

static int l_prefs_close(lua_State *L) {
    lua_prefs_t *prefs = (lua_prefs_t *)luaL_checkudata(L, 1, PREFS_METATABLE);
    if (prefs && prefs->is_open) {
        nvs_close(prefs->handle);
        prefs->is_open = false;
    }
    return 0;
}

static int l_prefs_gc(lua_State *L) {
    return l_prefs_close(L);
}

static int l_prefs_tostring(lua_State *L) {
    lua_prefs_t *prefs = (lua_prefs_t *)luaL_checkudata(L, 1, PREFS_METATABLE);
    if (prefs && prefs->is_open) {
        lua_pushliteral(L, "preferences (open)");
    } else {
        lua_pushliteral(L, "preferences (closed)");
    }
    return 1;
}

static int l_prefs_set_i32(lua_State *L) {
    lua_prefs_t *prefs = check_prefs(L, 1);
    const char *key = check_key(L, 2);
    int32_t val = (int32_t)luaL_checkinteger(L, 3);

    esp_err_t err = nvs_set_i32(prefs->handle, key, val);
    if (err != ESP_OK) {
        return luaL_error(L, "Failed to write int '%s': %s", key, esp_err_to_name(err));
    }
    err = nvs_commit(prefs->handle);
    if (err != ESP_OK) {
        return luaL_error(L, "Failed to commit '%s': %s", key, esp_err_to_name(err));
    }

    lua_pushboolean(L, true);
    return 1;
}

static int l_prefs_get_i32(lua_State *L) {
    lua_prefs_t *prefs = check_prefs(L, 1);
    const char *key = check_key(L, 2);

    int32_t val = 0;
    esp_err_t err = nvs_get_i32(prefs->handle, key, &val);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
            lua_pushvalue(L, 3);
        } else {
            lua_pushnil(L);
        }
        return 1;
    }
    if (err != ESP_OK) {
        return luaL_error(L, "Failed to read int '%s': %s", key, esp_err_to_name(err));
    }

    lua_pushinteger(L, val);
    return 1;
}

static int l_prefs_set_bool(lua_State *L) {
    lua_prefs_t *prefs = check_prefs(L, 1);
    const char *key = check_key(L, 2);
    uint8_t val = lua_toboolean(L, 3) ? 1 : 0;

    esp_err_t err = nvs_set_u8(prefs->handle, key, val);
    if (err != ESP_OK) {
        return luaL_error(L, "Failed to write bool '%s': %s", key, esp_err_to_name(err));
    }
    err = nvs_commit(prefs->handle);
    if (err != ESP_OK) {
        return luaL_error(L, "Failed to commit '%s': %s", key, esp_err_to_name(err));
    }

    lua_pushboolean(L, true);
    return 1;
}

static int l_prefs_get_bool(lua_State *L) {
    lua_prefs_t *prefs = check_prefs(L, 1);
    const char *key = check_key(L, 2);

    uint8_t val = 0;
    esp_err_t err = nvs_get_u8(prefs->handle, key, &val);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
            lua_pushvalue(L, 3);
        } else {
            lua_pushnil(L);
        }
        return 1;
    }
    if (err != ESP_OK) {
        return luaL_error(L, "Failed to read bool '%s': %s", key, esp_err_to_name(err));
    }

    lua_pushboolean(L, val != 0);
    return 1;
}

static int l_prefs_set_str(lua_State *L) {
    lua_prefs_t *prefs = check_prefs(L, 1);
    const char *key = check_key(L, 2);
    const char *val = luaL_checkstring(L, 3);

    esp_err_t err = nvs_set_str(prefs->handle, key, val);
    if (err != ESP_OK) {
        return luaL_error(L, "Failed to write string '%s': %s", key, esp_err_to_name(err));
    }
    err = nvs_commit(prefs->handle);
    if (err != ESP_OK) {
        return luaL_error(L, "Failed to commit '%s': %s", key, esp_err_to_name(err));
    }

    lua_pushboolean(L, true);
    return 1;
}

static int l_prefs_get_str(lua_State *L) {
    lua_prefs_t *prefs = check_prefs(L, 1);
    const char *key = check_key(L, 2);

    size_t required_size = 0;
    esp_err_t err = nvs_get_str(prefs->handle, key, NULL, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
            lua_pushvalue(L, 3);
        } else {
            lua_pushnil(L);
        }
        return 1;
    }
    if (err != ESP_OK) {
        return luaL_error(L, "Failed to read string '%s': %s", key, esp_err_to_name(err));
    }

    char *buf = (char *)malloc(required_size);
    if (!buf) {
        return luaL_error(L, "Out of memory reading string '%s'", key);
    }
    err = nvs_get_str(prefs->handle, key, buf, &required_size);
    if (err != ESP_OK) {
        free(buf);
        return luaL_error(L, "Failed to read string '%s': %s", key, esp_err_to_name(err));
    }

    lua_pushlstring(L, buf, required_size > 0 ? required_size - 1 : 0);
    free(buf);
    return 1;
}

static int l_prefs_is_key(lua_State *L) {
    lua_prefs_t *prefs = check_prefs(L, 1);
    const char *key = check_key(L, 2);

    nvs_type_t type;
    esp_err_t err = nvs_find_key(prefs->handle, key, &type);
    lua_pushboolean(L, err == ESP_OK);
    return 1;
}

static int l_prefs_remove(lua_State *L) {
    lua_prefs_t *prefs = check_prefs(L, 1);
    const char *key = check_key(L, 2);

    esp_err_t err = nvs_erase_key(prefs->handle, key);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        return luaL_error(L, "Failed to erase key '%s': %s", key, esp_err_to_name(err));
    }
    err = nvs_commit(prefs->handle);
    if (err != ESP_OK) {
        return luaL_error(L, "Failed to commit erase '%s': %s", key, esp_err_to_name(err));
    }

    lua_pushboolean(L, true);
    return 1;
}

static int l_prefs_clear(lua_State *L) {
    lua_prefs_t *prefs = check_prefs(L, 1);

    esp_err_t err = nvs_erase_all(prefs->handle);
    if (err != ESP_OK) {
        return luaL_error(L, "Failed to clear namespace: %s", esp_err_to_name(err));
    }
    err = nvs_commit(prefs->handle);
    if (err != ESP_OK) {
        return luaL_error(L, "Failed to commit clear: %s", esp_err_to_name(err));
    }

    lua_pushboolean(L, true);
    return 1;
}

static int l_prefs_commit(lua_State *L) {
    lua_prefs_t *prefs = check_prefs(L, 1);

    esp_err_t err = nvs_commit(prefs->handle);
    if (err != ESP_OK) {
        return luaL_error(L, "Failed to commit: %s", esp_err_to_name(err));
    }

    lua_pushboolean(L, true);
    return 1;
}

void register_preferences(lua_State *L) {
    if (luaL_newmetatable(L, PREFS_METATABLE)) {
        lua_pushcfunction(L, l_prefs_gc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, l_prefs_tostring);
        lua_setfield(L, -2, "__tostring");
    }
    lua_pop(L, 1);

    lua_register(L, "_esp_prefs_open", l_prefs_open);
    lua_register(L, "_esp_prefs_close", l_prefs_close);
    lua_register(L, "_esp_prefs_set_i32", l_prefs_set_i32);
    lua_register(L, "_esp_prefs_get_i32", l_prefs_get_i32);
    lua_register(L, "_esp_prefs_set_bool", l_prefs_set_bool);
    lua_register(L, "_esp_prefs_get_bool", l_prefs_get_bool);
    lua_register(L, "_esp_prefs_set_str", l_prefs_set_str);
    lua_register(L, "_esp_prefs_get_str", l_prefs_get_str);
    lua_register(L, "_esp_prefs_is_key", l_prefs_is_key);
    lua_register(L, "_esp_prefs_remove", l_prefs_remove);
    lua_register(L, "_esp_prefs_clear", l_prefs_clear);
    lua_register(L, "_esp_prefs_commit", l_prefs_commit);
}
