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

void eos_wifi_init(void) {
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

void eos_wifi_scan(void) {
    unsigned char *buffer = eos_net_alloc();
    buffer[0] = EOS_WIFI_MTYPE_SCAN;
    eos_net_send(EOS_NET_MTYPE_WIFI, buffer, 1, 0);
}

void eos_wifi_connect(const char *ssid, const char *pass) {
    int ssid_len = strlen(ssid);
    int pass_len = strlen(pass);
    unsigned char *buffer = eos_net_alloc();

    buffer[0] = EOS_WIFI_MTYPE_CONNECT;
    strcpy(buffer+1, ssid);
    buffer[ssid_len+1] = 0;
    strcpy(buffer+ssid_len+2, pass);
    buffer[ssid_len+pass_len+2] = 0;
    eos_net_send(EOS_NET_MTYPE_WIFI, buffer, ssid_len+pass_len+3, 0);
}

void eos_wifi_disconnect(void) {
    unsigned char *buffer = eos_net_alloc();
    buffer[0] = EOS_WIFI_MTYPE_DISCONNECT;
    eos_net_send(EOS_NET_MTYPE_WIFI, buffer, 1, 0);
}
