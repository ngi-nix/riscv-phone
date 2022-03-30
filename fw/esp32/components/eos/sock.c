#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <esp_system.h>
#include <esp_log.h>
#include <esp_err.h>

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
    _myaddr.sin_port = htons(0);

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
    servaddr.sin_port = htons(addr->port);
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
        addr->port = ntohs(servaddr.sin_port);
        memcpy(addr->host, (void *)&servaddr.sin_addr, sizeof(addr->host));
    }
    return recvlen;
}

static void udp_rcvr_task(void *pvParameters) {
    uint8_t sock_i = (uint8_t)pvParameters;
    int sock = _socks[sock_i-1];

    do {
        EOSNetAddr addr;
        unsigned char *buf, *_buf;
        ssize_t rv;

        buf = eos_net_alloc();
        rv = t_recvfrom(sock, buf + EOS_SOCK_SIZE_UDP_HDR, EOS_NET_MTU - EOS_SOCK_SIZE_UDP_HDR, &addr);
        if (rv < 0) {
            eos_net_free(buf);
            ESP_LOGE(TAG, "UDP RECV ERR:%d", rv);
            break;
        }
        _buf = buf;
        _buf[0] = EOS_SOCK_MTYPE_PKT;
        _buf[1] = sock_i;
        _buf += 2;
        memcpy(_buf, addr.host, sizeof(addr.host));
        _buf += sizeof(addr.host);
        _buf[0] = addr.port >> 8;
        _buf[1] = addr.port;
        _buf += sizeof(addr.port);
        eos_net_send(EOS_NET_MTYPE_SOCK, buf, rv + EOS_SOCK_SIZE_UDP_HDR);
    } while (1);

    xSemaphoreTake(mutex, portMAX_DELAY);
    _socks[sock_i-1] = 0;
    xSemaphoreGive(mutex);
    vTaskDelete(NULL);
}

static void sock_handler(unsigned char type, unsigned char *buffer, uint16_t buf_len) {
    EOSNetAddr addr;
    uint8_t sock_i;
    int sock, i;

    if (buf_len < 1) return;

    switch(buffer[0]) {
        case EOS_SOCK_MTYPE_PKT:
            if (buf_len < EOS_SOCK_SIZE_UDP_HDR) return;

            sock_i = buffer[1]-1;
            if (sock_i >= EOS_SOCK_MAX_SOCK) return;

            sock = _socks[sock_i];
            buffer += 2;
            memcpy(addr.host, buffer, sizeof(addr.host));
            buffer += sizeof(addr.host);
            addr.port  = (uint16_t)buffer[0] << 8;
            addr.port |= (uint16_t)buffer[1];
            buffer += sizeof(addr.port);
            t_sendto(sock, buffer, buf_len - EOS_SOCK_SIZE_UDP_HDR, &addr);
            break;

        case EOS_SOCK_MTYPE_OPEN_DGRAM:
            sock = t_open_dgram();
            sock_i = 0;
            if (sock > 0) {
                xSemaphoreTake(mutex, portMAX_DELAY);
                for (i=0; i<EOS_SOCK_MAX_SOCK; i++) {
                    if (_socks[i] == 0) {
                        sock_i = i+1;
                        _socks[i] = sock;
                        break;
                    }
                }
                xSemaphoreGive(mutex);
            }
            if (sock_i) xTaskCreate(&udp_rcvr_task, "udp_rcvr", EOS_TASK_SSIZE_UDP_RCVR, (void *)sock_i, EOS_TASK_PRIORITY_UDP_RCVR, NULL);
            buffer[0] = EOS_SOCK_MTYPE_OPEN_DGRAM;
            buffer[1] = sock_i;
            eos_net_reply(EOS_NET_MTYPE_SOCK, buffer, 2);
            break;

        case EOS_SOCK_MTYPE_CLOSE:
            if (buf_len < 2) return;

            sock_i = buffer[1]-1;
            if (sock_i >= EOS_SOCK_MAX_SOCK) return;

            sock = _socks[sock_i];
            t_close(sock);
            break;

        default:
            ESP_LOGE(TAG, "BAD TYPE:%d", buffer[0]);
            break;
    }
}

void eos_sock_init(void) {
    mutex = xSemaphoreCreateBinary();
    xSemaphoreGive(mutex);
    eos_net_set_handler(EOS_NET_MTYPE_SOCK, sock_handler);
    ESP_LOGI(TAG, "INIT");
}
