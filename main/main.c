#define LUA_32BITS 1

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_littlefs.h"
#include "esp_vfs.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "esp_timer.h"

#define TAG "lua_atmos"
#define LUA_FILE_PATH "/assets"

static int l_delay(lua_State *L)
{
    int ms = luaL_checkinteger(L, 1);
    TickType_t ticks = pdMS_TO_TICKS(ms);
    if (ticks == 0) {
        ticks = 1; // Guarantee at least 1 tick delay to block the task and allow IDLE task to feed WDT
    }
    vTaskDelay(ticks);
    return 0;
}

static int l_micros(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)esp_timer_get_time());
    return 1;
}

static void register_esp_bindings(lua_State *L)
{
    lua_register(L, "delay", l_delay);
    lua_register(L, "micros", l_micros);
}

// Initialize and mount the LittleFS filesystem
static void init_filesystem(void)
{
    ESP_LOGI(TAG, "Initializing LittleFS filesystem");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = LUA_FILE_PATH,
        .partition_label = "assets",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount or format filesystem: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Filesystem mounted at %s", LUA_FILE_PATH);
    }
}

// Create state and execute the main Lua script
static void run_lua_file(const char *file_name)
{
    ESP_LOGI(TAG, "Starting Lua Atmos application from file: %s", file_name);

    lua_State *L = luaL_newstate();
    if (L == NULL) {
        ESP_LOGE(TAG, "Failed to create new Lua state");
        return;
    }

    luaL_openlibs(L);
    register_esp_bindings(L);

    // Set the Lua module search path to resolve library requires
    if (luaL_dostring(L, "package.path = package.path .. ';./?.lua;/assets/?.lua;/assets/?/init.lua'")) {
        ESP_LOGE(TAG, "Failed to set package.path: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    // Construct the full file path and execute
    char full_path[128];
    snprintf(full_path, sizeof(full_path), LUA_FILE_PATH"/%s", file_name);

    if (luaL_dofile(L, full_path) == LUA_OK) {
        lua_pop(L, lua_gettop(L));
    } else {
        ESP_LOGE(TAG, "Error running Lua script '%s': %s", full_path, lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    lua_close(L);
    ESP_LOGI(TAG, "End of Lua Atmos application.");
}

void app_main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    ESP_LOGI(TAG, "Lua Atmos Starter");

    init_filesystem();
    run_lua_file("test_atmos.lua");

    // Prevent task exit
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
