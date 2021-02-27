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
