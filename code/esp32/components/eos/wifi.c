#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_system.h>
#include <esp_event.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "eos.h"
#include "net.h"
#include "wifi.h"

static const char *TAG = "EOS";

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event) {
    switch(event->event_id) {
        case SYSTEM_EVENT_SCAN_DONE:
            break;

        case SYSTEM_EVENT_STA_START:
            break;

        case SYSTEM_EVENT_STA_STOP:
            break;

        case SYSTEM_EVENT_STA_CONNECTED:
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "DISCONNECTED");
            break;

        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "********************************************");
            ESP_LOGI(TAG, "* We are now connected to AP");
            ESP_LOGI(TAG, "* - Our IP address is: " IPSTR, IP2STR(&event->event_info.got_ip.ip_info.ip));
            ESP_LOGI(TAG, "********************************************");
            unsigned char *buf = malloc(32);
            buf[0] = EOS_WIFI_MTYPE_CONNECT;
            eos_net_send(EOS_NET_MTYPE_WIFI, buf, 1, EOS_NET_FLAG_BUF_FREE);
            break;

        default: // Ignore the other event types
            break;
    }

    return ESP_OK;
}

static void wifi_handler(unsigned char _mtype, unsigned char *buffer, uint16_t size) {
    uint8_t mtype = buffer[0];
    char *ssid, *pass;

    switch (mtype) {
        case EOS_WIFI_MTYPE_SCAN:
            break;
        case EOS_WIFI_MTYPE_CONNECT:
            ssid = (char *)buffer+1;
            pass = ssid+strlen(ssid)+1;
            eos_wifi_connect(ssid, pass);
            break;
        case EOS_WIFI_MTYPE_DISCONNECT:
            eos_wifi_disconnect();
            break;
    }
    // eos_wifi_connect((char *)buffer, (char *)(buffer+strlen((char *)buffer)+1));
}

void eos_wifi_init(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi_config;

    memset(&wifi_config, 0, sizeof(wifi_config));
    tcpip_adapter_init();
    // ESP_ERROR_CHECK( nvs_flash_init() );
    ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL) );
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    eos_net_set_handler(EOS_NET_MTYPE_WIFI, wifi_handler);
}

void eos_wifi_connect(char *ssid, char *pass) {
    wifi_config_t wifi_config;

    memset(&wifi_config, 0, sizeof(wifi_config));
    strncpy((char *)wifi_config.sta.ssid, ssid, 31);
    strncpy((char *)wifi_config.sta.password, pass, 63);
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_connect() );
}

void eos_wifi_disconnect(void) {
    ESP_ERROR_CHECK( esp_wifi_disconnect() );
}

