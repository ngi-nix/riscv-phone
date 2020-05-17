#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "eos.h"
#include "event.h"
#include "net.h"

#include "cell.h"

static eos_evt_handler_t evt_handler[EOS_CELL_MAX_MTYPE];

static void cell_handle_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    if ((buffer == NULL) || (len < 1)) {
        eos_net_bad_handler(type, buffer, len);
        return;
    }

    unsigned char mtype = buffer[0];
    if (mtype < EOS_CELL_MAX_MTYPE) {
        evt_handler[mtype](type, buffer, len);
    } else {
        eos_net_bad_handler(type, buffer, len);
    }
}

static void cell_handle_rdy(unsigned char type, unsigned char *buffer, uint16_t len) {
    // Do nothing
    eos_net_free(buffer, 0);
}

void eos_cell_init(void) {
    int i;

    for (i=0; i<EOS_CELL_MAX_MTYPE; i++) {
        evt_handler[i] = eos_net_bad_handler;
    }
    eos_net_set_handler(EOS_NET_MTYPE_CELL, cell_handle_evt);
    eos_cell_set_handler(EOS_CELL_MTYPE_READY, cell_handle_rdy);
}

void eos_cell_set_handler(unsigned char mtype, eos_evt_handler_t handler) {
    if (handler == NULL) handler = eos_net_bad_handler;
    if (mtype < EOS_CELL_MAX_MTYPE) evt_handler[mtype] = handler;
}
