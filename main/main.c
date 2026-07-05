#define LUA_32BITS 1

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_littlefs.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "esp_timer.h"

#define TAG "lua_atmos"

#define LUA_MOUNT_PATH "/lua"
#define LUA_PARTITION_LABEL "lua"
#define LUA_CLOCK_PERIOD_US 1000
#define LUA_ENTRYPOINT "init.lua"
#define LUA_ENTRYPOINT_PATH LUA_MOUNT_PATH "/" LUA_ENTRYPOINT
#define LUA_PACKAGE_PATH \
    "package.path = package.path .. ';" \
    LUA_MOUNT_PATH "/?.lua;" \
    LUA_MOUNT_PATH "/?/init.lua'"

static TaskHandle_t s_lua_task;
static esp_timer_handle_t s_clock_timer;

static void clock_timer_cb(void *arg)
{
    if (s_lua_task != NULL) {
        xTaskNotifyGive(s_lua_task);
    }
}

static void start_clock_timer(void)
{
    if (s_clock_timer != NULL) {
        return;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = clock_timer_cb,
        .name = "atmos_clock",
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_clock_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_clock_timer, LUA_CLOCK_PERIOD_US));
}

static void stop_clock_timer(void)
{
    if (s_clock_timer == NULL) {
        return;
    }

    ESP_ERROR_CHECK(esp_timer_stop(s_clock_timer));
    ESP_ERROR_CHECK(esp_timer_delete(s_clock_timer));
    s_clock_timer = NULL;
}

static int l_wait_clock(lua_State *L)
{
    return lua_yield(L, 0);
}

static void register_esp_bindings(lua_State *L)
{
    lua_register(L, "wait_clock", l_wait_clock);
}

static void init_filesystem(void)
{
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

static bool set_lua_package_path(lua_State *L)
{
    if (luaL_dostring(L, LUA_PACKAGE_PATH) == LUA_OK) {
        return true;
    }

    ESP_LOGE(TAG, "Failed to set package.path: %s", lua_tostring(L, -1));
    lua_pop(L, 1);
    return false;
}

static void wait_for_clock_tick(void)
{
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

static int resume_lua_app(lua_State *L, lua_State *T)
{
    int nresults = 0;
    int status = lua_resume(T, L, 0, &nresults);

    while (status == LUA_YIELD) {
        wait_for_clock_tick();
        status = lua_resume(T, L, 0, &nresults);
    }

    if (status == LUA_OK) {
        lua_pop(T, nresults);
    } else {
        size_t len;
        const char *err_msg = luaL_tolstring(T, -1, &len);
        ESP_LOGE(TAG, "Error running Lua script '%s': %s", LUA_ENTRYPOINT_PATH, err_msg);
        lua_pop(T, 2); // Pop both the formatted string and the original error object
    }

    return status;
}

static uint64_t s_last_yield_time = 0;

static void lua_hook(lua_State *L, lua_Debug *ar)
{
    uint64_t now = esp_timer_get_time();
    if (now - s_last_yield_time > 500000) { // 500ms
        s_last_yield_time = now;
        vTaskDelay(1);
    }
}

static int run_lua_file(void)
{
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

    s_last_yield_time = esp_timer_get_time();
    lua_sethook(T, lua_hook, LUA_MASKCOUNT, 10000);

    // Tune Garbage Collector for memory-constrained embedded environments (Lua 5.4/5.5 API)
    lua_gc(L, LUA_GCPARAM, LUA_GCPPAUSE, 100);
    lua_gc(L, LUA_GCPARAM, LUA_GCPSTEPMUL, 500);

    if (!set_lua_package_path(L)) {
        status = LUA_ERRRUN;
        goto done;
    }

    if (luaL_loadfile(T, LUA_ENTRYPOINT_PATH) != LUA_OK) {
        ESP_LOGE(TAG, "Error loading Lua script '%s': %s", LUA_ENTRYPOINT_PATH, lua_tostring(T, -1));
        lua_pop(T, 1);
        status = LUA_ERRFILE;
    } else {
        s_lua_task = xTaskGetCurrentTaskHandle();
        start_clock_timer();
        status = resume_lua_app(L, T);
        stop_clock_timer();
        s_lua_task = NULL;
    }

done:
    lua_close(L);
    return status;
}

void app_main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    init_filesystem();
    ESP_LOGI(TAG, "Running %s", LUA_ENTRYPOINT_PATH);
    int status = run_lua_file();
    ESP_LOGI(TAG, "Lua app exited with status %d", status);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
