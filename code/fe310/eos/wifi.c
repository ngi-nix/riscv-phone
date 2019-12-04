#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "eos.h"
#include "event.h"
#include "net.h"

#include "wifi.h"

static eos_wifi_fptr_t wifi_handler[EOS_WIFI_MAX_MTYPE];
static uint16_t wifi_handler_flags_buf_free = 0;
static uint16_t wifi_handler_flags_buf_acq = 0;

static void wifi_handler_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    if ((buffer == NULL) || (len < 1)) {
        eos_evtq_bad_handler(type, buffer, len);
        eos_net_free(buffer, 0);
        return;
    }

    uint8_t mtype = buffer[0];
    if (mtype >= EOS_WIFI_MAX_MTYPE) {
        eos_evtq_bad_handler(type, buffer, len);
        eos_net_free(buffer, 0);
        return;
    }

    uint16_t buf_free = ((uint16_t)1 << mtype) & wifi_handler_flags_buf_free;
    uint16_t buf_acq = ((uint16_t)1 << mtype) & wifi_handler_flags_buf_acq;
    if (buf_free) {
        eos_net_free(buffer, buf_acq);
        buffer = NULL;
        len = 0;
    }

    wifi_handler[mtype](buffer, len);

    if (buf_free && buf_acq) eos_net_release();
}

void eos_wifi_init(void) {
    eos_net_set_handler(EOS_NET_MTYPE_WIFI, wifi_handler_evt, 0);
}

void eos_wifi_connect(const char *ssid, const char *pass) {
    int ssid_len = strlen(ssid);
    int pass_len = strlen(pass);
    unsigned char *buffer = eos_net_alloc(0);

    buffer[0] = EOS_WIFI_MTYPE_CONNECT;
    strcpy(buffer+1, ssid);
    buffer[ssid_len+1] = 0;
    strcpy(buffer+ssid_len+2, pass);
    buffer[ssid_len+pass_len+2] = 0;
    eos_net_send(EOS_NET_MTYPE_WIFI, buffer, ssid_len+pass_len+3, 0);
}

void eos_wifi_disconnect(void) {
    unsigned char *buffer = eos_net_alloc(0);
    buffer[0] = EOS_WIFI_MTYPE_DISCONNECT;
    eos_net_send(EOS_NET_MTYPE_WIFI, buffer, 1, 0);
}

void eos_wifi_set_handler(int mtype, eos_wifi_fptr_t handler, uint8_t flags) {
    if (mtype >= EOS_WIFI_MAX_MTYPE) {
        return;
    }

    if (flags) {
        uint16_t flag = (uint16_t)1 << mtype;
        if (flags & EOS_NET_FLAG_BUF_FREE) wifi_handler_flags_buf_free |= flag;
        if (flags & EOS_NET_FLAG_BUF_ACQ) wifi_handler_flags_buf_acq |= flag;
    }
    wifi_handler[mtype] = handler;
}
