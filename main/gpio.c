#include "bindings.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lua.h"
#include "lauxlib.h"

#define HW_EVENT_QUEUE_LEN 32
#define GPIO_MAX_TRACKED 64

typedef enum {
    HW_EVENT_GPIO,
    HW_EVENT_PWM
} hw_event_type_t;

typedef struct {
    hw_event_type_t type;
    int id;            // pin number or channel number
    int value;         // pin level or target duty
    int64_t time_us;
} hw_event_t;

// =========================================================================
// Structs & Shared State
// =========================================================================

static QueueHandle_t s_hw_event_queue = NULL;
static bool s_gpio_isr_attached[GPIO_MAX_TRACKED] = {0};
static bool s_fade_service_installed = false;

static adc_oneshot_unit_handle_t s_adc1_handle = NULL;
static adc_oneshot_unit_handle_t s_adc2_handle = NULL;

static QueueHandle_t get_hw_event_queue(void) {
    if (s_hw_event_queue == NULL) {
        s_hw_event_queue = xQueueCreate(HW_EVENT_QUEUE_LEN, sizeof(hw_event_t));
    }
    return s_hw_event_queue;
}

// =========================================================================
// GPIO / Interrupts
// =========================================================================

static bool valid_pin(int pin) {
    return GPIO_IS_VALID_GPIO(pin) && pin < GPIO_MAX_TRACKED;
}

static bool valid_output_pin(int pin) {
    return GPIO_IS_VALID_OUTPUT_GPIO(pin) && pin < GPIO_MAX_TRACKED;
}

static int check_pin(lua_State *L, int index) {
    int pin = (int)luaL_checkinteger(L, index);
    luaL_argcheck(L, valid_pin(pin), index, "invalid GPIO pin");
    return pin;
}

static int check_output_pin(lua_State *L, int index) {
    int pin = (int)luaL_checkinteger(L, index);
    luaL_argcheck(L, valid_output_pin(pin), index, "GPIO pin cannot be used as output");
    return pin;
}

static int check_pin_value(lua_State *L, int index) {
    if (lua_isboolean(L, index)) {
        return lua_toboolean(L, index) ? 1 : 0;
    }
    return luaL_checkinteger(L, index) != 0 ? 1 : 0;
}

static esp_err_t ensure_gpio_isr_service(void) {
    esp_err_t err = gpio_install_isr_service(0);
    if (err == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    return err;
}

static gpio_int_type_t parse_edge(lua_State *L, int index) {
    const char *edge = luaL_checkstring(L, index);
    if (strcmp(edge, "rising") == 0) {
        return GPIO_INTR_POSEDGE;
    }
    if (strcmp(edge, "falling") == 0) {
        return GPIO_INTR_NEGEDGE;
    }
    if (strcmp(edge, "both") == 0 || strcmp(edge, "change") == 0) {
        return GPIO_INTR_ANYEDGE;
    }
    return (gpio_int_type_t)luaL_error(L, "invalid edge '%s'", edge);
}

static void IRAM_ATTR gpio_isr_handler(void *arg) {
    int pin = (int)(intptr_t)arg;
    QueueHandle_t queue = get_hw_event_queue();
    if (queue == NULL) {
        return;
    }

    hw_event_t ev = {
        .type = HW_EVENT_GPIO,
        .id = pin,
        .value = gpio_get_level((gpio_num_t)pin),
        .time_us = esp_timer_get_time(),
    };

    BaseType_t task_woken = pdFALSE;
    (void)xQueueSendFromISR(queue, &ev, &task_woken);
    if (task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static int l_gpio_mode(lua_State *L) {
    int pin = check_pin(L, 1);
    const char *mode = luaL_checkstring(L, 2);

    gpio_config_t conf = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    if (strcmp(mode, "input") == 0) {
        conf.mode = GPIO_MODE_INPUT;
    } else if (strcmp(mode, "input_pullup") == 0) {
        conf.mode = GPIO_MODE_INPUT;
        conf.pull_up_en = GPIO_PULLUP_ENABLE;
    } else if (strcmp(mode, "input_pulldown") == 0) {
        conf.mode = GPIO_MODE_INPUT;
        conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    } else if (strcmp(mode, "output") == 0) {
        check_output_pin(L, 1);
        conf.mode = GPIO_MODE_OUTPUT;
    } else if (strcmp(mode, "open_drain") == 0) {
        check_output_pin(L, 1);
        conf.mode = GPIO_MODE_OUTPUT_OD;
    } else {
        return luaL_error(L, "invalid gpio mode '%s'", mode);
    }

    esp_err_t err = gpio_config(&conf);
    if (err != ESP_OK) {
        return luaL_error(L, "gpio_config failed: %s", esp_err_to_name(err));
    }

    return 0;
}

static int l_gpio_read(lua_State *L) {
    int pin = check_pin(L, 1);
    lua_pushinteger(L, gpio_get_level((gpio_num_t)pin));
    return 1;
}

static int l_gpio_write(lua_State *L) {
    int pin = check_output_pin(L, 1);
    int value = check_pin_value(L, 2);

    esp_err_t err = gpio_set_level((gpio_num_t)pin, value);
    if (err != ESP_OK) {
        return luaL_error(L, "gpio_set_level failed: %s", esp_err_to_name(err));
    }

    return 0;
}

static int l_gpio_watch(lua_State *L) {
    int pin = check_pin(L, 1);
    gpio_int_type_t edge = parse_edge(L, 2);

    QueueHandle_t queue = get_hw_event_queue();
    if (queue == NULL) {
        return luaL_error(L, "failed to initialize hardware event queue");
    }

    esp_err_t err = ensure_gpio_isr_service();
    if (err != ESP_OK) {
        return luaL_error(L, "failed to install GPIO ISR service: %s", esp_err_to_name(err));
    }

    err = gpio_set_intr_type((gpio_num_t)pin, edge);
    if (err != ESP_OK) {
        return luaL_error(L, "gpio_set_intr_type failed: %s", esp_err_to_name(err));
    }

    if (!s_gpio_isr_attached[pin]) {
        err = gpio_isr_handler_add((gpio_num_t)pin, gpio_isr_handler, (void *)(intptr_t)pin);
        if (err != ESP_OK) {
            return luaL_error(L, "gpio_isr_handler_add failed: %s", esp_err_to_name(err));
        }
        s_gpio_isr_attached[pin] = true;
    }

    err = gpio_intr_enable((gpio_num_t)pin);
    if (err != ESP_OK) {
        return luaL_error(L, "gpio_intr_enable failed: %s", esp_err_to_name(err));
    }

    return 0;
}

static int l_gpio_unwatch(lua_State *L) {
    int pin = check_pin(L, 1);

    esp_err_t err = gpio_intr_disable((gpio_num_t)pin);
    if (err != ESP_OK) {
        return luaL_error(L, "gpio_intr_disable failed: %s", esp_err_to_name(err));
    }

    if (s_gpio_isr_attached[pin]) {
        err = gpio_isr_handler_remove((gpio_num_t)pin);
        if (err != ESP_OK) {
            return luaL_error(L, "gpio_isr_handler_remove failed: %s", esp_err_to_name(err));
        }
        s_gpio_isr_attached[pin] = false;
    }

    err = gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_DISABLE);
    if (err != ESP_OK) {
        return luaL_error(L, "gpio_set_intr_type failed: %s", esp_err_to_name(err));
    }

    return 0;
}

// =========================================================================
// ADC Oneshot
// =========================================================================

static adc_atten_t parse_atten(lua_State *L, int index) {
    if (lua_isnoneornil(L, index)) {
        return ADC_ATTEN_DB_12; // Default
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

static int l_gpio_read_adc(lua_State *L) {
    int pin = (int)luaL_checkinteger(L, 1);
    adc_atten_t atten = parse_atten(L, 2);

    adc_unit_t unit;
    adc_channel_t channel;
    esp_err_t err = adc_oneshot_io_to_channel(pin, &unit, &channel);
    if (err != ESP_OK) {
        return luaL_error(L, "GPIO pin %d is not supported by ADC: %s", pin, esp_err_to_name(err));
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

// =========================================================================
// PWM (LEDC)
// =========================================================================

static IRAM_ATTR bool pwm_fade_end_callback(const ledc_cb_param_t *param, void *user_arg) {
    int channel = (int)(intptr_t)user_arg;
    QueueHandle_t queue = get_hw_event_queue();
    if (queue == NULL) {
        return false;
    }

    hw_event_t ev = {
        .type = HW_EVENT_PWM,
        .id = channel,
        .value = 0,
        .time_us = esp_timer_get_time(),
    };

    BaseType_t task_woken = pdFALSE;
    (void)xQueueSendFromISR(queue, &ev, &task_woken);
    return task_woken == pdTRUE;
}

static int l_gpio_setup_pwm(lua_State *L) {
    int pin = (int)luaL_checkinteger(L, 1);
    luaL_argcheck(L, GPIO_IS_VALID_OUTPUT_GPIO(pin), 1, "GPIO pin cannot be used as output");

    int channel = (int)luaL_checkinteger(L, 2);
    luaL_argcheck(L, channel >= 0 && channel < LEDC_CHANNEL_MAX, 2, "invalid LEDC channel");

    int freq = (int)luaL_checkinteger(L, 3);
    luaL_argcheck(L, freq > 0, 3, "frequency must be positive");

    int resolution = (int)luaL_checkinteger(L, 4);
    luaL_argcheck(L, resolution >= 1 && resolution <= 20, 4, "resolution must be between 1 and 20");

    int timer_num = channel % 4;

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

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = (ledc_channel_t)channel,
        .timer_sel      = (ledc_timer_t)timer_num,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = pin,
        .duty           = 0,
        .hpoint         = 0
    };
    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK) {
        return luaL_error(L, "ledc_channel_config failed: %s", esp_err_to_name(err));
    }

    if (!s_fade_service_installed) {
        err = ledc_fade_func_install(0);
        if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
            s_fade_service_installed = true;
        } else {
            return luaL_error(L, "ledc_fade_func_install failed: %s", esp_err_to_name(err));
        }
    }

    QueueHandle_t queue = get_hw_event_queue();
    if (queue == NULL) {
        return luaL_error(L, "failed to initialize shared hardware event queue");
    }

    ledc_cbs_t callbacks = {
        .fade_cb = pwm_fade_end_callback
    };
    err = ledc_cb_register(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, &callbacks, (void*)(intptr_t)channel);
    if (err != ESP_OK) {
        return luaL_error(L, "ledc_cb_register failed: %s", esp_err_to_name(err));
    }

    return 0;
}

static int l_gpio_write_pwm(lua_State *L) {
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

static int l_gpio_fade_pwm(lua_State *L) {
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

// =========================================================================
// Unified Event Loop
// =========================================================================

static int l_gpio_next_event(lua_State *L) {
    QueueHandle_t queue = get_hw_event_queue();
    hw_event_t ev;
    if (queue == NULL || xQueueReceive(queue, &ev, 0) != pdTRUE) {
        lua_pushnil(L);
        return 1;
    }

    if (ev.type == HW_EVENT_GPIO) {
        lua_createtable(L, 0, 5);

        lua_pushliteral(L, "gpio");
        lua_setfield(L, -2, "tag");
        lua_pushinteger(L, ev.id);
        lua_setfield(L, -2, "pin");
        lua_pushinteger(L, ev.value);
        lua_setfield(L, -2, "value");
        lua_pushinteger(L, (lua_Integer)ev.time_us);
        lua_setfield(L, -2, "time");
        lua_pushstring(L, ev.value ? "rising" : "falling");
        lua_setfield(L, -2, "edge");
    } else if (ev.type == HW_EVENT_PWM) {
        lua_createtable(L, 0, 3);

        lua_pushliteral(L, "gpio_fade_pwm");
        lua_setfield(L, -2, "tag");
        lua_pushinteger(L, ev.id);
        lua_setfield(L, -2, "channel");
        lua_pushinteger(L, (lua_Integer)ev.time_us);
        lua_setfield(L, -2, "time");
    } else {
        lua_pushnil(L);
    }

    return 1;
}

void register_gpio(lua_State *L) {
    lua_register(L, "_esp_gpio_mode", l_gpio_mode);
    lua_register(L, "_esp_gpio_read", l_gpio_read);
    lua_register(L, "_esp_gpio_write", l_gpio_write);
    lua_register(L, "_esp_gpio_watch", l_gpio_watch);
    lua_register(L, "_esp_gpio_unwatch", l_gpio_unwatch);
    lua_register(L, "_esp_gpio_read_adc", l_gpio_read_adc);
    lua_register(L, "_esp_gpio_setup_pwm", l_gpio_setup_pwm);
    lua_register(L, "_esp_gpio_write_pwm", l_gpio_write_pwm);
    lua_register(L, "_esp_gpio_fade_pwm", l_gpio_fade_pwm);
    lua_register(L, "_esp_gpio_next_event", l_gpio_next_event);
}
