#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "eos.h"
#include "event.h"
#include "net.h"

#include "cell.h"

static eos_evt_fptr_t evt_handler[EOS_CELL_MAX_MTYPE];
static uint16_t evt_handler_flags_buf_free = 0;
static uint16_t evt_handler_flags_buf_acq = 0;

static void cell_handler_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    if ((buffer == NULL) || (len < 1)) {
        eos_evtq_bad_handler(type, buffer, len);
        eos_net_free(buffer, 0);
        return;
    }

    uint8_t mtype = buffer[0];
    if (mtype >= EOS_CELL_MAX_MTYPE) {
        eos_evtq_bad_handler(type, buffer, len);
        eos_net_free(buffer, 0);
        return;
    }

    _eos_net_handle(type, buffer, len, mtype, evt_handler, &evt_handler_flags_buf_free, &evt_handler_flags_buf_acq);
}

void eos_cell_init(void) {
    int i;

    for (i=0; i<EOS_CELL_MAX_MTYPE; i++) {
        evt_handler[i] = eos_evtq_bad_handler;
    }
    eos_net_set_handler(EOS_NET_MTYPE_CELL, cell_handler_evt, 0);
}

void eos_cell_set_handler(int mtype, eos_evt_fptr_t handler, uint8_t flags) {
    if (mtype >= EOS_CELL_MAX_MTYPE) {
        return;
    }
    _eos_net_set_handler(mtype, handler, evt_handler, flags, &evt_handler_flags_buf_free, &evt_handler_flags_buf_acq);
}
