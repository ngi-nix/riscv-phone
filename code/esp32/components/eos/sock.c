#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

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
#include "net.h"
#include "sock.h"

static const char *TAG = "EOS SOCK";
static SemaphoreHandle_t mutex;
static int _socks[EOS_SOCK_MAX_SOCK];

static int t_open_dgram(void) {
    struct sockaddr_in _myaddr;
    int sock;

    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return sock;

    memset((char *)&_myaddr, 0, sizeof(_myaddr));
    _myaddr.sin_family = AF_INET;
    _myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    _myaddr.sin_port = htons(3000);

    int rv = bind(sock, (struct sockaddr *)&_myaddr, sizeof(_myaddr));
    if (rv < 0) {
        close(sock);
        return rv;
    }
    return sock;
}

static void t_close(int sock) {
    close(sock);
}

static ssize_t t_sendto(int sock, void *msg, size_t msg_size, EOSNetAddr *addr) {
    struct sockaddr_in servaddr;

    memset((void *)&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = addr->port;
    memcpy((void *)&servaddr.sin_addr, addr->host, sizeof(addr->host));
    return sendto(sock, msg, msg_size, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
}

static ssize_t t_recvfrom(int sock, void *msg, size_t msg_size, EOSNetAddr *addr) {
    struct sockaddr_in servaddr;
    socklen_t addrlen = sizeof(servaddr);
    memset((void *)&servaddr, 0, sizeof(servaddr));

    ssize_t recvlen = recvfrom(sock, msg, msg_size, 0, (struct sockaddr *)&servaddr, &addrlen);
    if (recvlen < 0) return recvlen;

    if (addr) {
        addr->port = servaddr.sin_port;
        memcpy(addr->host, (void *)&servaddr.sin_addr, sizeof(addr->host));
    }
    return recvlen;
}

static void udp_rcvr_task(void *pvParameters) {
    EOSNetAddr addr;
    uint8_t esock = (uint8_t)pvParameters;
    int sock = _socks[esock-1];
    unsigned char *buf;

    do {
        ssize_t rv;

        buf = eos_net_alloc();
        rv = t_recvfrom(sock, buf+EOS_SOCK_SIZE_UDP_HDR, EOS_NET_SIZE_BUF-EOS_SOCK_SIZE_UDP_HDR, &addr);
        if (rv < 0) {
            sock = 0;
            ESP_LOGE(TAG, "UDP RECV ERR:%d", rv);
            continue;
        }
        buf[0] = EOS_SOCK_MTYPE_PKT;
        buf[1] = esock;
        memcpy(buf+2, addr.host, sizeof(addr.host));
        memcpy(buf+2+sizeof(addr.host), &addr.port, sizeof(addr.port));
        eos_net_send(EOS_NET_MTYPE_SOCK, buf, rv+EOS_SOCK_SIZE_UDP_HDR, 0);
    } while(sock);
    xSemaphoreTake(mutex, portMAX_DELAY);
    _socks[esock-1] = 0;
    xSemaphoreGive(mutex);
    vTaskDelete(NULL);
}

static void sock_handler(unsigned char type, unsigned char *buffer, uint16_t size) {
    EOSNetAddr addr;
    uint8_t esock;
    int sock, i;
    unsigned char *rbuf;

    if (size < 1) return;

    switch(buffer[0]) {
        case EOS_SOCK_MTYPE_PKT:
            if (size < EOS_SOCK_SIZE_UDP_HDR) return;
            sock = _socks[buffer[1]-1];
            memcpy(addr.host, buffer+2, sizeof(addr.host));
            memcpy(&addr.port, buffer+2+sizeof(addr.host), sizeof(addr.port));
            t_sendto(sock, buffer+EOS_SOCK_SIZE_UDP_HDR, size-EOS_SOCK_SIZE_UDP_HDR, &addr);
            break;

        case EOS_SOCK_MTYPE_OPEN_DGRAM:
            sock = t_open_dgram();
            esock = 0;
            if (sock > 0) {
                xSemaphoreTake(mutex, portMAX_DELAY);
                for (i=0; i<EOS_SOCK_MAX_SOCK; i++) {
                    if (_socks[i] == 0) {
                        esock = i+1;
                        _socks[i] = sock;
                    }
                }
                xSemaphoreGive(mutex);
            }
            // xTaskCreatePinnedToCore(&sock_receiver, "sock_receiver", EOS_TASK_SSIZE_UDP_RCVR, (void *)esock, EOS_TASK_PRIORITY_UDP_RCVR, NULL, 1);
            xTaskCreate(&udp_rcvr_task, "udp_rcvr", EOS_TASK_SSIZE_UDP_RCVR, (void *)esock, EOS_TASK_PRIORITY_UDP_RCVR, NULL);
            rbuf = eos_net_alloc();
            rbuf[0] = EOS_SOCK_MTYPE_OPEN_DGRAM;
            rbuf[1] = esock;
            eos_net_send(EOS_NET_MTYPE_SOCK, rbuf, 2, 0);
            break;

        case EOS_SOCK_MTYPE_CLOSE:
            if (size < 2) return;
            sock = _socks[buffer[1]-1];
            t_close(sock);
            break;

        default:
            break;
    }
}

void eos_sock_init(void) {
    mutex = xSemaphoreCreateBinary();
    xSemaphoreGive(mutex);
    eos_net_set_handler(EOS_NET_MTYPE_SOCK, sock_handler);
    ESP_LOGI(TAG, "INIT");
}