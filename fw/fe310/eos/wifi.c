#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "eos.h"
#include "event.h"
#include "net.h"

#include "wifi.h"

static eos_evt_handler_t evt_handler[EOS_WIFI_MAX_MTYPE];

static void wifi_handle_msg(unsigned char type, unsigned char *buffer, uint16_t len) {
    unsigned char mtype;

    if ((buffer == NULL) || (len < 1)) {
        eos_net_bad_handler(type, buffer, len);
        return;
    }

    mtype = buffer[0];
    if ((mtype < EOS_WIFI_MAX_MTYPE) && evt_handler[mtype]) {
        evt_handler[mtype](mtype, buffer, len);
    } else {
        eos_net_bad_handler(type, buffer, len);
    }
}

void eos_wifi_netinit(void) {
    int i;

    for (i=0; i<EOS_WIFI_MAX_MTYPE; i++) {
        evt_handler[i] = NULL;
    }
    eos_net_set_handler(EOS_NET_MTYPE_WIFI, wifi_handle_msg);
}

void eos_wifi_set_handler(unsigned char mtype, eos_evt_handler_t handler) {
    if (mtype < EOS_WIFI_MAX_MTYPE) evt_handler[mtype] = handler;
}

eos_evt_handler_t eos_wifi_get_handler(unsigned char mtype) {
    if (mtype < EOS_WIFI_MAX_MTYPE) return evt_handler[mtype];
    return NULL;
}

int eos_wifi_scan(unsigned char *buffer) {
    int async;

    async = 0;
    if (buffer == NULL) {
        buffer = eos_net_alloc();
        async = 1;
    }
    buffer[0] = EOS_WIFI_MTYPE_SCAN;
    return _eos_net_send(EOS_NET_MTYPE_WIFI, buffer, 1, async, 1);
}

int eos_wifi_auth(const char *ssid, const char *pass, unsigned char *buffer) {
    unsigned char *buf;
    size_t ssid_len, pass_len;
    int async;

    async = 0;
    if (buffer == NULL) {
        buffer = eos_net_alloc();
        async = 1;
    }
    ssid_len = strlen(ssid) + 1;
    pass_len = strlen(pass) + 1;
    if ((1 + ssid_len + pass_len) > EOS_NET_MTU) return EOS_ERR_SIZE;

    buf = buffer;
    buf[0] = EOS_WIFI_MTYPE_AUTH;
    buf++;
    strcpy(buf, ssid);
    buf += ssid_len;
    strcpy(buf, pass);
    buf += pass_len;

    return _eos_net_send(EOS_NET_MTYPE_WIFI, buffer, 1 + ssid_len + pass_len, async, 1);
}

int eos_wifi_connect(unsigned char *buffer) {
    int async;

    async = 0;
    if (buffer == NULL) {
        buffer = eos_net_alloc();
        async = 1;
    }
    buffer[0] = EOS_WIFI_MTYPE_CONNECT;
    return _eos_net_send(EOS_NET_MTYPE_WIFI, buffer, 1, async, 1);
}

int eos_wifi_disconnect(unsigned char *buffer) {
    int async;

    async = 0;
    if (buffer == NULL) {
        buffer = eos_net_alloc();
        async = 1;
    }
    buffer[0] = EOS_WIFI_MTYPE_DISCONNECT;
    return _eos_net_send(EOS_NET_MTYPE_WIFI, buffer, 1, async, 1);
}
