#include "esp_bindings.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lauxlib.h"

#define PIN_EVENT_QUEUE_LEN 32
#define PIN_MAX_TRACKED 64

typedef struct {
    int pin;
    int value;
    int64_t time_us;
} pin_event_t;

static QueueHandle_t s_pin_event_queue = NULL;
static bool s_pin_isr_attached[PIN_MAX_TRACKED] = {0};

static bool valid_pin(int pin) {
    return GPIO_IS_VALID_GPIO(pin) && pin < PIN_MAX_TRACKED;
}

static bool valid_output_pin(int pin) {
    return GPIO_IS_VALID_OUTPUT_GPIO(pin) && pin < PIN_MAX_TRACKED;
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

static esp_err_t ensure_pin_event_queue(void) {
    if (s_pin_event_queue == NULL) {
        s_pin_event_queue = xQueueCreate(PIN_EVENT_QUEUE_LEN, sizeof(pin_event_t));
        if (s_pin_event_queue == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
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

static void IRAM_ATTR pin_isr_handler(void *arg) {
    int pin = (int)(intptr_t)arg;
    if (s_pin_event_queue == NULL) {
        return;
    }

    pin_event_t ev = {
        .pin = pin,
        .value = gpio_get_level((gpio_num_t)pin),
        .time_us = esp_timer_get_time(),
    };

    BaseType_t task_woken = pdFALSE;
    (void)xQueueSendFromISR(s_pin_event_queue, &ev, &task_woken);
    if (task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static int l_pin_mode(lua_State *L) {
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
        return luaL_error(L, "invalid pin mode '%s'", mode);
    }

    esp_err_t err = gpio_config(&conf);
    if (err != ESP_OK) {
        return luaL_error(L, "gpio_config failed: %s", esp_err_to_name(err));
    }

    return 0;
}

static int l_pin_read(lua_State *L) {
    int pin = check_pin(L, 1);
    lua_pushinteger(L, gpio_get_level((gpio_num_t)pin));
    return 1;
}

static int l_pin_write(lua_State *L) {
    int pin = check_output_pin(L, 1);
    int value = check_pin_value(L, 2);

    esp_err_t err = gpio_set_level((gpio_num_t)pin, value);
    if (err != ESP_OK) {
        return luaL_error(L, "gpio_set_level failed: %s", esp_err_to_name(err));
    }

    return 0;
}

static int l_pin_watch(lua_State *L) {
    int pin = check_pin(L, 1);
    gpio_int_type_t edge = parse_edge(L, 2);

    esp_err_t err = ensure_pin_event_queue();
    if (err != ESP_OK) {
        return luaL_error(L, "failed to create pin event queue: %s", esp_err_to_name(err));
    }

    err = ensure_gpio_isr_service();
    if (err != ESP_OK) {
        return luaL_error(L, "failed to install GPIO ISR service: %s", esp_err_to_name(err));
    }

    err = gpio_set_intr_type((gpio_num_t)pin, edge);
    if (err != ESP_OK) {
        return luaL_error(L, "gpio_set_intr_type failed: %s", esp_err_to_name(err));
    }

    if (!s_pin_isr_attached[pin]) {
        err = gpio_isr_handler_add((gpio_num_t)pin, pin_isr_handler, (void *)(intptr_t)pin);
        if (err != ESP_OK) {
            return luaL_error(L, "gpio_isr_handler_add failed: %s", esp_err_to_name(err));
        }
        s_pin_isr_attached[pin] = true;
    }

    err = gpio_intr_enable((gpio_num_t)pin);
    if (err != ESP_OK) {
        return luaL_error(L, "gpio_intr_enable failed: %s", esp_err_to_name(err));
    }

    return 0;
}

static int l_pin_unwatch(lua_State *L) {
    int pin = check_pin(L, 1);

    esp_err_t err = gpio_intr_disable((gpio_num_t)pin);
    if (err != ESP_OK) {
        return luaL_error(L, "gpio_intr_disable failed: %s", esp_err_to_name(err));
    }

    if (s_pin_isr_attached[pin]) {
        err = gpio_isr_handler_remove((gpio_num_t)pin);
        if (err != ESP_OK) {
            return luaL_error(L, "gpio_isr_handler_remove failed: %s", esp_err_to_name(err));
        }
        s_pin_isr_attached[pin] = false;
    }

    err = gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_DISABLE);
    if (err != ESP_OK) {
        return luaL_error(L, "gpio_set_intr_type failed: %s", esp_err_to_name(err));
    }

    return 0;
}

static int l_pin_next_event(lua_State *L) {
    pin_event_t ev;
    if (s_pin_event_queue == NULL || xQueueReceive(s_pin_event_queue, &ev, 0) != pdTRUE) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, 0, 5);

    lua_pushliteral(L, "pin");
    lua_setfield(L, -2, "tag");
    lua_pushinteger(L, ev.pin);
    lua_setfield(L, -2, "pin");
    lua_pushinteger(L, ev.value);
    lua_setfield(L, -2, "value");
    lua_pushinteger(L, (lua_Integer)ev.time_us);
    lua_setfield(L, -2, "time");
    lua_pushstring(L, ev.value ? "rising" : "falling");
    lua_setfield(L, -2, "edge");

    return 1;
}

void register_pins_esp(lua_State *L) {
    lua_register(L, "_esp_pin_mode", l_pin_mode);
    lua_register(L, "_esp_pin_read", l_pin_read);
    lua_register(L, "_esp_pin_write", l_pin_write);
    lua_register(L, "_esp_pin_watch", l_pin_watch);
    lua_register(L, "_esp_pin_unwatch", l_pin_unwatch);
    lua_register(L, "_esp_pin_next_event", l_pin_next_event);
}
