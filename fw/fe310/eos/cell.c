#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "eos.h"
#include "event.h"
#include "net.h"

#include "cell.h"

static eos_evt_handler_t evt_handler[EOS_CELL_MAX_MTYPE];

static void cell_handle_msg(unsigned char type, unsigned char *buffer, uint16_t len) {
    unsigned char mtype;
    unsigned char idx;

    if ((buffer == NULL) || (len < 1)) {
        eos_net_bad_handler(type, buffer, len);
        return;
    }

    mtype = buffer[0];
    idx = (mtype & EOS_CELL_MTYPE_MASK) >> 4;
    if ((idx < EOS_CELL_MAX_MTYPE) && evt_handler[idx]) {
        evt_handler[idx](mtype & ~EOS_CELL_MTYPE_MASK, buffer, len);
    } else {
        eos_net_bad_handler(type, buffer, len);
    }
}

void eos_cell_init(void) {
    int i;

    for (i=0; i<EOS_CELL_MAX_MTYPE; i++) {
        evt_handler[i] = NULL;
    }
    eos_net_set_handler(EOS_NET_MTYPE_CELL, cell_handle_msg);
}

void eos_cell_set_handler(unsigned char mtype, eos_evt_handler_t handler) {
    unsigned char idx = (mtype & EOS_CELL_MTYPE_MASK) >> 4;

    if (idx < EOS_CELL_MAX_MTYPE) evt_handler[idx] = handler;
}

eos_evt_handler_t eos_cell_get_handler(unsigned char mtype) {
    unsigned char idx = (mtype & EOS_CELL_MTYPE_MASK) >> 4;

    if (idx < EOS_CELL_MAX_MTYPE) return evt_handler[idx];
    return NULL;
}
