#define LUA_32BITS 1

#include "esp_bindings.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "lua.h"
#include "lauxlib.h"

static adc_oneshot_unit_handle_t s_adc1_handle = NULL;
static adc_oneshot_unit_handle_t s_adc2_handle = NULL;

static adc_atten_t parse_atten(lua_State *L, int index) {
    if (lua_isnoneornil(L, index)) {
        return ADC_ATTEN_DB_12; // Default (covers 0 to ~3.3V)
    }
    if (lua_isinteger(L, index)) {
        int val = lua_tointeger(L, index);
        if (val == 0) return ADC_ATTEN_DB_0;
        if (val == 2) return ADC_ATTEN_DB_2_5;
        if (val == 6) return ADC_ATTEN_DB_6;
        if (val == 11 || val == 12) return ADC_ATTEN_DB_12;
    } else if (lua_isstring(L, index)) {
        const char *s = lua_tostring(L, index);
        if (strcmp(s, "0db") == 0) return ADC_ATTEN_DB_0;
        if (strcmp(s, "2.5db") == 0) return ADC_ATTEN_DB_2_5;
        if (strcmp(s, "6db") == 0) return ADC_ATTEN_DB_6;
        if (strcmp(s, "11db") == 0 || strcmp(s, "12db") == 0) return ADC_ATTEN_DB_12;
    }
    return (adc_atten_t)luaL_error(L, "invalid attenuation (expected 0db, 2.5db, 6db, 11db, 12db)");
}

static int l_adc_read(lua_State *L) {
    int pin = (int)luaL_checkinteger(L, 1);
    adc_atten_t atten = parse_atten(L, 2);

    adc_unit_t unit;
    adc_channel_t channel;
    esp_err_t err = adc_oneshot_io_to_channel(pin, &unit, &channel);
    if (err != ESP_OK) {
        return luaL_error(L, "GPIO pin %d is not supported by ADC oneshot driver: %s", pin, esp_err_to_name(err));
    }

    adc_oneshot_unit_handle_t handle = NULL;
    if (unit == ADC_UNIT_1) {
        if (s_adc1_handle == NULL) {
            adc_oneshot_unit_init_cfg_t init_config = {
                .unit_id = ADC_UNIT_1,
                .clk_src = 0,
                .ulp_mode = ADC_ULP_MODE_DISABLE,
            };
            err = adc_oneshot_new_unit(&init_config, &s_adc1_handle);
            if (err != ESP_OK) {
                return luaL_error(L, "failed to initialize ADC unit 1: %s", esp_err_to_name(err));
            }
        }
        handle = s_adc1_handle;
    } else if (unit == ADC_UNIT_2) {
        if (s_adc2_handle == NULL) {
            adc_oneshot_unit_init_cfg_t init_config = {
                .unit_id = ADC_UNIT_2,
                .clk_src = 0,
                .ulp_mode = ADC_ULP_MODE_DISABLE,
            };
            err = adc_oneshot_new_unit(&init_config, &s_adc2_handle);
            if (err != ESP_OK) {
                return luaL_error(L, "failed to initialize ADC unit 2: %s", esp_err_to_name(err));
            }
        }
        handle = s_adc2_handle;
    } else {
        return luaL_error(L, "unknown ADC unit %d", unit);
    }

    // Configure the ADC channel for the requested attenuation and maximum bitwidth
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = atten,
    };
    err = adc_oneshot_config_channel(handle, channel, &config);
    if (err != ESP_OK) {
        return luaL_error(L, "adc_oneshot_config_channel failed: %s", esp_err_to_name(err));
    }

    int raw_val = 0;
    err = adc_oneshot_read(handle, channel, &raw_val);
    if (err != ESP_OK) {
        return luaL_error(L, "adc_oneshot_read failed: %s", esp_err_to_name(err));
    }

    lua_pushinteger(L, raw_val);
    return 1;
}

void register_adc_esp(lua_State *L) {
    lua_register(L, "_esp_adc_read", l_adc_read);
}
