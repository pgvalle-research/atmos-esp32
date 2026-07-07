#define LUA_32BITS 1

#include "bindings.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lua.h"
#include "lauxlib.h"

#define ESPNOW_QUEUE_LEN 32
#define TAG "espnow_c"

typedef enum {
    ESPNOW_EVENT_RECV,
    ESPNOW_EVENT_SEND
} espnow_event_type_t;

typedef struct {
    espnow_event_type_t type;
    uint8_t mac[6];
    uint8_t *data; // NULL for SEND events
    int len;       // payload length for RECV, status (0=success, 1=fail) for SEND
    int64_t time_us;
} espnow_event_t;

static QueueHandle_t s_espnow_queue = NULL;
static bool s_espnow_initialized = false;

static bool parse_mac(const char *str, uint8_t *mac) {
    int values[6];
    int parsed = sscanf(str, "%x:%x:%x:%x:%x:%x",
                        &values[0], &values[1], &values[2],
                        &values[3], &values[4], &values[5]);
    if (parsed == 6) {
        for (int i = 0; i < 6; i++) {
            mac[i] = (uint8_t)values[i];
        }
        return true;
    }
    return false;
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (s_espnow_queue == NULL) {
        return;
    }

    uint8_t *buf = malloc(len);
    if (buf == NULL) {
        ESP_LOGE(TAG, "failed to allocate memory for recv packet");
        return;
    }
    memcpy(buf, data, len);

    espnow_event_t ev;
    ev.type = ESPNOW_EVENT_RECV;
    memcpy(ev.mac, recv_info->src_addr, 6);
    ev.data = buf;
    ev.len = len;
    ev.time_us = esp_timer_get_time();

    BaseType_t task_woken = pdFALSE;
    if (xQueueSendFromISR(s_espnow_queue, &ev, &task_woken) != pdTRUE) {
        free(buf); // Queue full, drop packet
    }
    if (task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
    if (s_espnow_queue == NULL) {
        return;
    }

    espnow_event_t ev = {
        .type = ESPNOW_EVENT_SEND,
        .len = (status == ESP_NOW_SEND_SUCCESS) ? 0 : 1, // 0 = success, 1 = fail
        .data = NULL,
        .time_us = esp_timer_get_time()
    };
    memcpy(ev.mac, tx_info->des_addr, 6);

    BaseType_t task_woken = pdFALSE;
    (void)xQueueSendFromISR(s_espnow_queue, &ev, &task_woken);
    if (task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static esp_err_t init_wifi(void) {
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif == NULL) {
        sta_netif = esp_netif_create_default_wifi_sta();
        if (sta_netif == NULL) {
            return ESP_FAIL;
        }
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) return err;

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) return err;

    err = esp_wifi_start();
    if (err != ESP_OK) return err;

    return ESP_OK;
}

static int l_espnow_init(lua_State *L) {
    if (s_espnow_initialized) {
        return 0;
    }

    esp_err_t err = init_wifi();
    if (err != ESP_OK) {
        return luaL_error(L, "failed to initialize wifi: %s", esp_err_to_name(err));
    }

    err = esp_now_init();
    if (err != ESP_OK) {
        return luaL_error(L, "failed to initialize esp-now: %s", esp_err_to_name(err));
    }

    if (s_espnow_queue == NULL) {
        s_espnow_queue = xQueueCreate(ESPNOW_QUEUE_LEN, sizeof(espnow_event_t));
        if (s_espnow_queue == NULL) {
            return luaL_error(L, "failed to create esp-now event queue");
        }
    }

    err = esp_now_register_recv_cb(espnow_recv_cb);
    if (err != ESP_OK) {
        return luaL_error(L, "failed to register recv callback: %s", esp_err_to_name(err));
    }

    err = esp_now_register_send_cb(espnow_send_cb);
    if (err != ESP_OK) {
        return luaL_error(L, "failed to register send callback: %s", esp_err_to_name(err));
    }

    s_espnow_initialized = true;
    return 0;
}

static int l_espnow_add_peer(lua_State *L) {
    if (!s_espnow_initialized) {
        return luaL_error(L, "esp-now is not initialized");
    }

    const char *mac_str = luaL_checkstring(L, 1);
    uint8_t mac[6];
    if (!parse_mac(mac_str, mac)) {
        return luaL_error(L, "invalid MAC address format '%s'", mac_str);
    }

    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0; // default
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    if (esp_now_is_peer_exist(mac)) {
        return 0;
    }

    esp_err_t err = esp_now_add_peer(&peer);
    if (err != ESP_OK) {
        return luaL_error(L, "failed to add esp-now peer: %s", esp_err_to_name(err));
    }

    return 0;
}

static int l_espnow_send(lua_State *L) {
    if (!s_espnow_initialized) {
        return luaL_error(L, "esp-now is not initialized");
    }

    const char *mac_str = luaL_checkstring(L, 1);
    uint8_t mac[6];
    if (!parse_mac(mac_str, mac)) {
        return luaL_error(L, "invalid MAC address format '%s'", mac_str);
    }

    size_t len;
    const char *data = luaL_checklstring(L, 2, &len);
    if (len > ESP_NOW_MAX_DATA_LEN) {
        return luaL_error(L, "data length %d exceeds max ESP-NOW payload of %d bytes", len, ESP_NOW_MAX_DATA_LEN);
    }

    esp_err_t err = esp_now_send(mac, (const uint8_t *)data, len);
    if (err != ESP_OK) {
        return luaL_error(L, "esp_now_send failed: %s", esp_err_to_name(err));
    }

    return 0;
}

static int l_espnow_get_mac(lua_State *L) {
    uint8_t mac[6];
    esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (err != ESP_OK) {
        err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
        if (err != ESP_OK) {
            return luaL_error(L, "failed to get local MAC address: %s", esp_err_to_name(err));
        }
    }

    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    lua_pushstring(L, mac_str);
    return 1;
}

static int l_espnow_next_event(lua_State *L) {
    if (s_espnow_queue == NULL) {
        lua_pushnil(L);
        return 1;
    }

    espnow_event_t ev;
    if (xQueueReceive(s_espnow_queue, &ev, 0) != pdTRUE) {
        lua_pushnil(L);
        return 1;
    }

    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             ev.mac[0], ev.mac[1], ev.mac[2], ev.mac[3], ev.mac[4], ev.mac[5]);

    if (ev.type == ESPNOW_EVENT_RECV) {
        lua_createtable(L, 0, 4);
        lua_pushliteral(L, "espnow_recv");
        lua_setfield(L, -2, "tag");
        lua_pushstring(L, mac_str);
        lua_setfield(L, -2, "mac");
        lua_pushlstring(L, (const char *)ev.data, ev.len);
        lua_setfield(L, -2, "data");
        lua_pushinteger(L, (lua_Integer)ev.time_us);
        lua_setfield(L, -2, "time");

        free(ev.data);
    } else if (ev.type == ESPNOW_EVENT_SEND) {
        lua_createtable(L, 0, 4);
        lua_pushliteral(L, "espnow_send");
        lua_setfield(L, -2, "tag");
        lua_pushstring(L, mac_str);
        lua_setfield(L, -2, "mac");
        lua_pushstring(L, ev.len == 0 ? "success" : "fail");
        lua_setfield(L, -2, "status");
        lua_pushinteger(L, (lua_Integer)ev.time_us);
        lua_setfield(L, -2, "time");
    } else {
        lua_pushnil(L);
    }

    return 1;
}

void register_espnow(lua_State *L) {
    lua_register(L, "_esp_espnow_init", l_espnow_init);
    lua_register(L, "_esp_espnow_add_peer", l_espnow_add_peer);
    lua_register(L, "_esp_espnow_send", l_espnow_send);
    lua_register(L, "_esp_espnow_get_mac", l_espnow_get_mac);
    lua_register(L, "_esp_espnow_next_event", l_espnow_next_event);
}
