#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_system.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "eos.h"
#include "net.h"
#include "wifi.h"

// XXX: WiFi fail due to no DHCP server

#define WIFI_MAX_SCAN_RECORDS       20
#define WIFI_MAX_CONNECT_ATTEMPTS   3

#define WIFI_STATE_STOPPED          0
#define WIFI_STATE_SCANNING         1
#define WIFI_STATE_CONNECTING       2
#define WIFI_STATE_CONNECTED        3
#define WIFI_STATE_DISCONNECTING    4
#define WIFI_STATE_DISCONNECTED     5

#define WIFI_ACTION_NONE            0
#define WIFI_ACTION_SCAN            1
#define WIFI_ACTION_CONNECT         2
#define WIFI_ACTION_DISCONNECT      3

static const char *TAG = "EOS WIFI";

static SemaphoreHandle_t mutex;

static wifi_config_t wifi_sta_config;
static wifi_scan_config_t wifi_scan_config;

static int wifi_connect_cnt = 0;
static uint8_t wifi_action;
static uint8_t wifi_state;
static wifi_ap_record_t scan_r[WIFI_MAX_SCAN_RECORDS];
static uint16_t scan_n;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    esp_err_t ret = ESP_OK;
    char _disconnect;
    uint8_t _action, _state;
    unsigned char *rbuf, *p;
    int i, len;
    ip_event_got_ip_t *got_ip;

    if (event_base == WIFI_EVENT) {
        switch(event_id) {
            case WIFI_EVENT_SCAN_DONE:
                scan_n = WIFI_MAX_SCAN_RECORDS;
                memset(scan_r, 0, sizeof(scan_r));
                esp_wifi_scan_get_ap_records(&scan_n, scan_r);

                ESP_LOGI(TAG, "Scan done: %d", scan_n);
                xSemaphoreTake(mutex, portMAX_DELAY);
                _state = wifi_state;
                if (wifi_state == WIFI_STATE_CONNECTED) wifi_action = WIFI_ACTION_NONE;
                xSemaphoreGive(mutex);

                if (_state != WIFI_STATE_CONNECTED) ret = esp_wifi_stop();

                rbuf = eos_net_alloc();
                rbuf[0] = EOS_WIFI_MTYPE_SCAN;
                p = rbuf + 1;
                for (i=0; i<scan_n; i++) {
                    len = strnlen((char *)scan_r[i].ssid, sizeof(scan_r[i].ssid));
                    if (len == sizeof(scan_r[i].ssid)) continue;
                    if (p - rbuf + len + 1 > EOS_NET_MTU) break;

                    strcpy((char *)p, (char *)scan_r[i].ssid);
                    p += len + 1;
                }
                eos_net_send(EOS_NET_MTYPE_WIFI, rbuf, p - rbuf);
                break;

            case WIFI_EVENT_STA_START:
                xSemaphoreTake(mutex, portMAX_DELAY);
                _action = wifi_action;
                xSemaphoreGive(mutex);

                switch (_action) {
                    case WIFI_ACTION_SCAN:
                        ret = esp_wifi_scan_start(&wifi_scan_config, 0);
                        break;

                    case WIFI_ACTION_CONNECT:
                        ret = esp_wifi_connect();
                        break;

                    default:
                        break;
                }
                break;

            case WIFI_EVENT_STA_STOP:
                xSemaphoreTake(mutex, portMAX_DELAY);
                wifi_state = WIFI_STATE_STOPPED;
                wifi_action = WIFI_ACTION_NONE;
                xSemaphoreGive(mutex);
                break;

            case WIFI_EVENT_STA_CONNECTED:
                xSemaphoreTake(mutex, portMAX_DELAY);
                wifi_state = WIFI_STATE_CONNECTED;
                wifi_action = WIFI_ACTION_NONE;
                wifi_connect_cnt = WIFI_MAX_CONNECT_ATTEMPTS;
                xSemaphoreGive(mutex);
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                xSemaphoreTake(mutex, portMAX_DELAY);
                if (wifi_connect_cnt) wifi_connect_cnt--;
                _action = wifi_action;
                _disconnect = (wifi_connect_cnt == 0);
                if (_disconnect) wifi_state = WIFI_STATE_DISCONNECTED;
                xSemaphoreGive(mutex);

                if (_disconnect) {
                    rbuf = eos_net_alloc();
                    rbuf[0] = EOS_WIFI_MTYPE_DISCONNECT;
                    eos_net_send(EOS_NET_MTYPE_WIFI, rbuf, 1);
                    if (!_action) ret = esp_wifi_stop();
                } else {
                    ret = esp_wifi_connect();
                }
                break;

            default: // Ignore the other event types
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch(event_id) {
            case IP_EVENT_STA_GOT_IP:
                got_ip = (ip_event_got_ip_t *)event_data;
                ESP_LOGI(TAG, "IP address: " IPSTR, IP2STR(&got_ip->ip_info.ip));
                rbuf = eos_net_alloc();
                rbuf[0] = EOS_WIFI_MTYPE_CONNECT;
                rbuf[1] = EOS_OK;
                eos_net_send(EOS_NET_MTYPE_WIFI, rbuf, 2);

                /* ip_changed is set even at normal connect!
                if (got_ip->ip_changed) {
                    ESP_LOGI(TAG, "IP changed");
                    // send wifi reconnect
                } else {
                    rbuf = eos_net_alloc();
                    rbuf[0] = EOS_WIFI_MTYPE_CONNECT;
                    rbuf[1] = EOS_OK;
                    eos_net_send(EOS_NET_MTYPE_WIFI, rbuf, 2);
                }
                */
                break;

            default: // Ignore the other event types
                break;
        }
    }

    if (ret != ESP_OK) ESP_LOGE(TAG, "EVT HANDLER ERR:%d EVT:%d", ret, event_id);
}

static void wifi_handler(unsigned char _mtype, unsigned char *buffer, uint16_t buf_len) {
    uint8_t mtype;
    int rv;
    char *ssid, *pass, *_buf;
    uint16_t _buf_len;

    if (buf_len < 1) return;

    rv = EOS_OK;

    mtype = buffer[0];
    buffer++;
    buf_len--;

    switch (mtype) {
        case EOS_WIFI_MTYPE_SCAN:
            rv = eos_wifi_scan();
            break;

        case EOS_WIFI_MTYPE_AUTH:
            _buf = (char *)buffer;
            _buf_len = 0;

            ssid = _buf;
            _buf_len = strnlen(_buf, buf_len);
            if (_buf_len == buf_len) break;
            _buf += _buf_len + 1;
            buf_len -= _buf_len + 1;

            pass = _buf;
            _buf_len = strnlen(_buf, buf_len);
            if (_buf_len == buf_len) break;
            _buf += _buf_len + 1;
            buf_len -= _buf_len + 1;

            rv = eos_wifi_auth(ssid, pass);
            break;

        case EOS_WIFI_MTYPE_CONNECT:
            rv = eos_wifi_connect();
            break;

        case EOS_WIFI_MTYPE_DISCONNECT:
            rv = eos_wifi_disconnect();
            break;
    }

    if (rv) ESP_LOGE(TAG, "MSG HANDLER ERR:%d MSG:%d", rv, mtype);
}

void eos_wifi_init(void) {
    esp_err_t ret;
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();

    memset(&wifi_sta_config, 0, sizeof(wifi_sta_config));

    esp_netif_create_default_wifi_sta();

    ret = esp_wifi_init(&wifi_config);
    assert(ret == ESP_OK);

    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    assert(ret == ESP_OK);

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);
    assert(ret == ESP_OK);

    ret = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    assert(ret == ESP_OK);

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    assert(ret == ESP_OK);

    ret = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_sta_config);
    assert(ret == ESP_OK);

    ret = esp_wifi_stop();
    assert(ret == ESP_OK);

    mutex = xSemaphoreCreateBinary();
    xSemaphoreGive(mutex);

    eos_net_set_handler(EOS_NET_MTYPE_WIFI, wifi_handler);
    ESP_LOGI(TAG, "INIT");
}

int eos_wifi_scan(void) {
    int rv = EOS_OK;
    esp_err_t ret = ESP_OK;
    uint8_t _wifi_state = 0;

    xSemaphoreTake(mutex, portMAX_DELAY);
    if (!wifi_action) {
        _wifi_state = wifi_state;

        wifi_action = WIFI_ACTION_SCAN;
        if (wifi_state == WIFI_STATE_STOPPED) wifi_state = WIFI_STATE_SCANNING;

        memset(&wifi_scan_config, 0, sizeof(wifi_scan_config));
    } else {
        rv = EOS_ERR_BUSY;
    }
    xSemaphoreGive(mutex);

    if (rv) return rv;

    if (_wifi_state == WIFI_STATE_STOPPED) {
        ret = esp_wifi_start();
    } else {
        ret = esp_wifi_scan_start(&wifi_scan_config, 0);
    }
    if (ret != ESP_OK) rv = EOS_ERR;

    return rv;
}

int eos_wifi_auth(char *ssid, char *pass) {
    int rv = EOS_OK;

    xSemaphoreTake(mutex, portMAX_DELAY);
    if (!wifi_action) {
        if (ssid) strncpy((char *)wifi_sta_config.sta.ssid, ssid, sizeof(wifi_sta_config.sta.ssid) - 1);
        if (pass) strncpy((char *)wifi_sta_config.sta.password, pass, sizeof(wifi_sta_config.sta.password) - 1);
    } else {
        rv = EOS_ERR_BUSY;

    }
    xSemaphoreGive(mutex);

    return rv;
}

int eos_wifi_connect(void) {
    int rv = EOS_OK;
    esp_err_t ret = ESP_OK;
    uint8_t _wifi_state = 0;

    xSemaphoreTake(mutex, portMAX_DELAY);
    if (!wifi_action) {
        _wifi_state = wifi_state;

        wifi_action = WIFI_ACTION_CONNECT;
        wifi_state = WIFI_STATE_CONNECTING;
        wifi_connect_cnt = WIFI_MAX_CONNECT_ATTEMPTS;
    } else {
        rv = EOS_ERR_BUSY;
    }
    xSemaphoreGive(mutex);

    if (rv) return rv;

    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_sta_config);

    if (_wifi_state == WIFI_STATE_STOPPED) {
        ret = esp_wifi_start();
    } else {
        ret = esp_wifi_connect();
    }
    if (ret != ESP_OK) rv = EOS_ERR;

    return rv;
}

int eos_wifi_disconnect(void) {
    int rv = EOS_OK;
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(mutex, portMAX_DELAY);
    if (!wifi_action) {
        wifi_action = WIFI_ACTION_DISCONNECT;
        wifi_state = WIFI_STATE_DISCONNECTING;
        wifi_connect_cnt = 0;

        memset(wifi_sta_config.sta.ssid, 0, sizeof(wifi_sta_config.sta.ssid));
        memset(wifi_sta_config.sta.password, 0, sizeof(wifi_sta_config.sta.password));
    } else {
        rv = EOS_ERR_BUSY;
    }
    xSemaphoreGive(mutex);

    if (rv) return rv;

    ret = esp_wifi_stop();
    if (ret != ESP_OK) rv = EOS_ERR;

    return rv;
}

