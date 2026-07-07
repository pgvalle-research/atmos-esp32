#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_littlefs.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "esp_timer.h"
#include "esp_bindings.h"

#define TAG "lua_atmos"

#define LUA_MOUNT_PATH "/lua"
#define LUA_PARTITION_LABEL "lua"
#define LUA_ENTRYPOINT "init.lua"
#define LUA_ENTRYPOINT_PATH LUA_MOUNT_PATH "/" LUA_ENTRYPOINT
#define LUA_PACKAGE_PATH \
    "package.path = package.path .. ';" \
    LUA_MOUNT_PATH "/?.lua;" \
    LUA_MOUNT_PATH "/?/init.lua'"

#define LUA_WATCHDOG_YIELD_INTERVAL_US 500000 /* Yield CPU to FreeRTOS every 500ms during long Lua runs */
#define LUA_WATCHDOG_HOOK_COUNT 10000         /* Call hook every 10,000 Lua instructions */
#define LUA_GC_PAUSE_PCT 100                  /* Start GC cycle as soon as there is garbage (100% of previous size) */
#define LUA_GC_STEPMUL_PCT 500                /* Run GC 5x faster than allocation rate */

static int l_meminfo(lua_State *L) {
    UBaseType_t stack_words = uxTaskGetStackHighWaterMark(NULL);

    lua_createtable(L, 0, 9);

    lua_pushinteger(L, (lua_Integer)heap_caps_get_total_size(MALLOC_CAP_8BIT));
    lua_setfield(L, -2, "heap_total");
    lua_pushinteger(L, (lua_Integer)heap_caps_get_free_size(MALLOC_CAP_8BIT));
    lua_setfield(L, -2, "heap_free");
    lua_pushinteger(L, (lua_Integer)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    lua_setfield(L, -2, "heap_min_free");
    lua_pushinteger(L, (lua_Integer)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    lua_setfield(L, -2, "heap_largest_free");

    lua_pushinteger(L, (lua_Integer)heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
    lua_setfield(L, -2, "internal_total");
    lua_pushinteger(L, (lua_Integer)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    lua_setfield(L, -2, "internal_free");
    lua_pushinteger(L, (lua_Integer)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
    lua_setfield(L, -2, "internal_min_free");
    lua_pushinteger(L, (lua_Integer)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    lua_setfield(L, -2, "internal_largest_free");

    lua_pushinteger(L, (lua_Integer)(stack_words * sizeof(StackType_t)));
    lua_setfield(L, -2, "stack_free");

    return 1;
}

static void register_esp_bindings(lua_State *L) {
    lua_register(L, "meminfo", l_meminfo);
    register_gpio_esp(L);
}

static void init_filesystem() {
    esp_vfs_littlefs_conf_t conf = {
        .base_path = LUA_MOUNT_PATH,
        .partition_label = LUA_PARTITION_LABEL,
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount or format filesystem: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Mounted Lua filesystem at %s", LUA_MOUNT_PATH);
    }
}

static bool set_lua_package_path(lua_State *L) {
    if (luaL_dostring(L, LUA_PACKAGE_PATH) == LUA_OK) return true;

    ESP_LOGE(TAG, "Failed to set package.path: %s", lua_tostring(L, -1));
    lua_pop(L, 1);
    return false;
}


static void lua_hook(lua_State *L, lua_Debug *ar) {
    static uint64_t s_last_yield_time = 0;
    uint64_t now = esp_timer_get_time();
    if (now - s_last_yield_time > LUA_WATCHDOG_YIELD_INTERVAL_US) {
        s_last_yield_time = now;
        vTaskDelay(1);
    }
}

static int run_lua_file() {
    lua_State *L = luaL_newstate();
    if (L == NULL) {
        ESP_LOGE(TAG, "Failed to create Lua state");
        return LUA_ERRMEM;
    }

    int status = LUA_OK;

    lua_State *T = lua_newthread(L);
    if (T == NULL) {
        ESP_LOGE(TAG, "Failed to create Lua app coroutine");
        status = LUA_ERRMEM;
        goto done;
    }

    luaL_openlibs(L);
    register_esp_bindings(L);

    lua_sethook(T, lua_hook, LUA_MASKCOUNT, LUA_WATCHDOG_HOOK_COUNT);

    // Tune Garbage Collector for memory-constrained embedded environments (Lua 5.4/5.5 API)
    lua_gc(L, LUA_GCPARAM, LUA_GCPPAUSE, LUA_GC_PAUSE_PCT);
    lua_gc(L, LUA_GCPARAM, LUA_GCPSTEPMUL, LUA_GC_STEPMUL_PCT);

    if (!set_lua_package_path(L)) {
        status = LUA_ERRRUN;
        goto done;
    }

    if (luaL_loadfile(T, LUA_ENTRYPOINT_PATH) != LUA_OK) {
        ESP_LOGE(TAG, "Error loading Lua script '%s': %s", LUA_ENTRYPOINT_PATH, lua_tostring(T, -1));
        lua_pop(T, 1);
        status = LUA_ERRFILE;
    } else {
        int nresults = 0;
        status = lua_resume(T, L, 0, &nresults);
        if (status == LUA_OK) {
            lua_pop(T, nresults);
        } else {
            size_t len;
            const char *err_msg = luaL_tolstring(T, -1, &len);
            ESP_LOGE(TAG, "Error running Lua script '%s': %s", LUA_ENTRYPOINT_PATH, err_msg);
            lua_pop(T, 2); // Pop both the formatted string and the original error object
        }
    }

done:
    lua_close(L);
    return status;
}

void app_main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    init_filesystem();
    ESP_LOGI(TAG, "Running %s", LUA_ENTRYPOINT_PATH);
    int status = run_lua_file();
    ESP_LOGI(TAG, "Lua app exited with status %d", status);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
