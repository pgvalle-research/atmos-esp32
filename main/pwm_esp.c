#define LUA_32BITS 1

#include "esp_bindings.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/ledc.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lua.h"
#include "lauxlib.h"

typedef struct {
    int channel;
    int64_t time_us;
} pwm_event_t;

static QueueHandle_t s_pwm_event_queue = NULL;
static bool s_fade_service_installed = false;

static IRAM_ATTR bool pwm_fade_end_callback(const ledc_cb_param_t *param, void *user_arg) {
    int channel = (int)(intptr_t)user_arg;
    pwm_event_t ev = {
        .channel = channel,
        .time_us = esp_timer_get_time(),
    };
    BaseType_t task_woken = pdFALSE;
    if (s_pwm_event_queue != NULL) {
        xQueueSendFromISR(s_pwm_event_queue, &ev, &task_woken);
    }
    return task_woken == pdTRUE;
}

static int l_pwm_setup(lua_State *L) {
    int pin = (int)luaL_checkinteger(L, 1);
    luaL_argcheck(L, GPIO_IS_VALID_OUTPUT_GPIO(pin), 1, "GPIO pin cannot be used as output");

    int channel = (int)luaL_checkinteger(L, 2);
    luaL_argcheck(L, channel >= 0 && channel < LEDC_CHANNEL_MAX, 2, "invalid LEDC channel");

    int freq = (int)luaL_checkinteger(L, 3);
    luaL_argcheck(L, freq > 0, 3, "frequency must be positive");

    int resolution = (int)luaL_checkinteger(L, 4);
    luaL_argcheck(L, resolution >= 1 && resolution <= 20, 4, "resolution must be between 1 and 20");

    int timer_num = channel % 4;

    // 1. Configure LEDC Timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = (ledc_timer_t)timer_num,
        .duty_resolution  = (ledc_timer_bit_t)resolution,
        .freq_hz          = freq,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    esp_err_t err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) {
        return luaL_error(L, "ledc_timer_config failed: %s", esp_err_to_name(err));
    }

    // 2. Configure LEDC Channel
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = (ledc_channel_t)channel,
        .timer_sel      = (ledc_timer_t)timer_num,
        .intr_type      = LEDC_INTR_DISABLE, // callback registration will handle interrupts
        .gpio_num       = pin,
        .duty           = 0,
        .hpoint         = 0
    };
    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK) {
        return luaL_error(L, "ledc_channel_config failed: %s", esp_err_to_name(err));
    }

    // 3. Setup Fade Service and Event Queue
    if (!s_fade_service_installed) {
        err = ledc_fade_func_install(0);
        if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
            s_fade_service_installed = true;
        } else {
            return luaL_error(L, "ledc_fade_func_install failed: %s", esp_err_to_name(err));
        }
    }

    if (s_pwm_event_queue == NULL) {
        s_pwm_event_queue = xQueueCreate(16, sizeof(pwm_event_t));
        if (s_pwm_event_queue == NULL) {
            return luaL_error(L, "failed to create PWM event queue");
        }
    }

    // 4. Register Callback for Fade End
    ledc_cbs_t callbacks = {
        .fade_cb = pwm_fade_end_callback
    };
    err = ledc_cb_register(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, &callbacks, (void*)(intptr_t)channel);
    if (err != ESP_OK) {
        return luaL_error(L, "ledc_cb_register failed: %s", esp_err_to_name(err));
    }

    return 0;
}

static int l_pwm_write(lua_State *L) {
    int channel = (int)luaL_checkinteger(L, 1);
    luaL_argcheck(L, channel >= 0 && channel < LEDC_CHANNEL_MAX, 1, "invalid LEDC channel");

    int duty = (int)luaL_checkinteger(L, 2);

    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
    if (err != ESP_OK) {
        return luaL_error(L, "ledc_set_duty failed: %s", esp_err_to_name(err));
    }

    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
    if (err != ESP_OK) {
        return luaL_error(L, "ledc_update_duty failed: %s", esp_err_to_name(err));
    }

    return 0;
}

static int l_pwm_fade(lua_State *L) {
    int channel = (int)luaL_checkinteger(L, 1);
    luaL_argcheck(L, channel >= 0 && channel < LEDC_CHANNEL_MAX, 1, "invalid LEDC channel");

    int target_duty = (int)luaL_checkinteger(L, 2);
    int duration_ms = (int)luaL_checkinteger(L, 3);
    luaL_argcheck(L, duration_ms >= 0, 3, "duration must be non-negative");

    esp_err_t err = ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, target_duty, duration_ms);
    if (err != ESP_OK) {
        return luaL_error(L, "ledc_set_fade_with_time failed: %s", esp_err_to_name(err));
    }

    err = ledc_fade_start(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, LEDC_FADE_NO_WAIT);
    if (err != ESP_OK) {
        return luaL_error(L, "ledc_fade_start failed: %s", esp_err_to_name(err));
    }

    return 0;
}

static int l_pwm_next_event(lua_State *L) {
    pwm_event_t ev;
    if (s_pwm_event_queue == NULL || xQueueReceive(s_pwm_event_queue, &ev, 0) != pdTRUE) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, 0, 3);
    lua_pushliteral(L, "pwm");
    lua_setfield(L, -2, "tag");
    lua_pushinteger(L, ev.channel);
    lua_setfield(L, -2, "channel");
    lua_pushinteger(L, (lua_Integer)ev.time_us);
    lua_setfield(L, -2, "time");

    return 1;
}

void register_pwm_esp(lua_State *L) {
    lua_register(L, "_esp_pwm_setup", l_pwm_setup);
    lua_register(L, "_esp_pwm_write", l_pwm_write);
    lua_register(L, "_esp_pwm_fade", l_pwm_fade);
    lua_register(L, "_esp_pwm_next_event", l_pwm_next_event);
}
