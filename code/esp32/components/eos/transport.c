/*
MIT License

Copyright (c) 2017 Olof Astrand (Ebiroll)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <string.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_system.h>
#include <esp_event.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include <lwip/sockets.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>

#include "eos.h"
#include "fe310.h"
#include "transport.h"

static const char *TAG = "EOS";
static int udp_sock = -1;
static TaskHandle_t receiver_task;

static int t_open(void) {
    struct sockaddr_in _myaddr;

    udp_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) return udp_sock;
    
    memset((char *)&_myaddr, 0, sizeof(_myaddr));
    _myaddr.sin_family = AF_INET;
    _myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    _myaddr.sin_port = htons(3000);

    int rv = bind(udp_sock, (struct sockaddr *)&_myaddr, sizeof(_myaddr));
    if (rv < 0) {
        close(udp_sock);
        return rv;
    }
    return EOS_OK;
}

static void t_close(void) {
    close(udp_sock);
}

static ssize_t t_send(void *msg, size_t msg_size, EOSNetAddr *addr) {
    struct sockaddr_in servaddr;

    memset((void *)&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = addr->port;
    memcpy((void *)&servaddr.sin_addr, addr->host, sizeof(addr->host));
    return sendto(udp_sock, msg, msg_size, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
}

static ssize_t t_recv(void *msg, size_t msg_size, EOSNetAddr *addr) {
    struct sockaddr_in servaddr;
    socklen_t addrlen = sizeof(servaddr);
    memset((void *)&servaddr, 0, sizeof(servaddr));

    ssize_t recvlen = recvfrom(udp_sock, msg, msg_size, 0, (struct sockaddr *)&servaddr, &addrlen);
    if (recvlen < 0) return recvlen;

    if (addr) {
        addr->port = servaddr.sin_port;
        memcpy(addr->host, (void *)&servaddr.sin_addr, sizeof(addr->host));
    }
    return recvlen;
}

static void receiver(void *pvParameters) {
    EOSNetAddr addr;
    size_t addr_len = sizeof(addr.host) + sizeof(addr.port);
    unsigned char buffer[2048];
    
    for (;;) {
        ssize_t rv = t_recv(buffer+addr_len, sizeof(buffer)-addr_len, &addr);
        if (rv < 0) {
            ESP_LOGI(TAG, "UDP RECV ERR:%d", rv);
            continue;
        }
        memcpy(buffer, addr.host, sizeof(addr.host));
        memcpy(buffer+sizeof(addr.host), &addr.port, sizeof(addr.port));
        eos_fe310_send(EOS_FE310_CMD_PKT, buffer, rv+addr_len);
    }
}

static void fe310_connect_cmd_handler(unsigned char cmd, unsigned char *buffer, uint16_t size) {
    eos_net_connect((char *)buffer, (char *)(buffer+strlen((char *)buffer)+1));
}

static void fe310_packet_cmd_handler(unsigned char cmd, unsigned char *buffer, uint16_t size) {
    EOSNetAddr addr;
    size_t addr_len = sizeof(addr.host) + sizeof(addr.port);
    
    memcpy(addr.host, buffer, sizeof(addr.host));
    memcpy(&addr.port, buffer+sizeof(addr.host), sizeof(addr.port));
    eos_net_send(buffer+addr_len, size-addr_len, &addr);
}

static esp_err_t esp32_wifi_event_handler(void *ctx, system_event_t *event) {
    switch(event->event_id) {
        case SYSTEM_EVENT_WIFI_READY:
            break;

        case SYSTEM_EVENT_AP_STACONNECTED:
            break;

        case SYSTEM_EVENT_AP_START: 
            break;

        case SYSTEM_EVENT_SCAN_DONE:
            break;

        case SYSTEM_EVENT_STA_CONNECTED: 
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "DISCONNECT");
            vTaskDelete(receiver_task);
            t_close();
            break;

        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "********************************************");
            ESP_LOGI(TAG, "* We are now connected to AP")
            ESP_LOGI(TAG, "* - Our IP address is: " IPSTR, IP2STR(&event->event_info.got_ip.ip_info.ip));
            ESP_LOGI(TAG, "********************************************");
            t_open();
            xTaskCreate(&receiver, "wifi_receiver", 4096, NULL, EOS_PRIORITY_WIFI, &receiver_task);
            // xTaskCreatePinnedToCore(&receiver, "wifi_receiver", 4096, NULL, EOS_PRIORITY_WIFI, &receiver_task, 1);
            eos_fe310_send(EOS_FE310_CMD_CONNECT, NULL, 0);
            break;

        default: // Ignore the other event types
            break;
    }

    return ESP_OK;
}

void eos_net_init(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi_config;

    memset(&wifi_config, 0, sizeof(wifi_config));
    tcpip_adapter_init();
    // ESP_ERROR_CHECK( nvs_flash_init() );
    ESP_ERROR_CHECK( esp_event_loop_init(esp32_wifi_event_handler, NULL) );
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    eos_fe310_set_handler(EOS_FE310_CMD_CONNECT, fe310_connect_cmd_handler);
    eos_fe310_set_handler(EOS_FE310_CMD_PKT, fe310_packet_cmd_handler);
}

void eos_net_connect(char *ssid, char *password) {
    wifi_config_t wifi_config;
    
    memset(&wifi_config, 0, sizeof(wifi_config));
    strncpy((char *)wifi_config.sta.ssid, ssid, 31);
    strncpy((char *)wifi_config.sta.password, password, 63);
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_connect() );
}

void eos_net_disconnect(void) {
    ESP_ERROR_CHECK( esp_wifi_disconnect() );
}

ssize_t eos_net_send(void *msg, size_t msg_size, EOSNetAddr *addr) {
    return t_send(msg, msg_size, addr);
}
