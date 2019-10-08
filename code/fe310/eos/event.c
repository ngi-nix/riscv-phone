#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "encoding.h"
#include "platform.h"

#include "net.h"
#include "msgq.h"
#include "event.h"

EOSMsgQ _eos_event_q;
static EOSMsgItem event_q_array[EOS_EVT_SIZE_Q];

static eos_evt_fptr_t evt_handler[EOS_EVT_MAX_EVT];
static uint16_t evt_handler_wrapper_acq = 0;
static uint16_t evt_handler_flags_buf_acq = 0;

void eos_evtq_init(void) {
    int i;
    
    for (i=0; i<EOS_EVT_MAX_EVT; i++) {
        evt_handler[i] = eos_evtq_bad_handler;
    }
    eos_msgq_init(&_eos_event_q, event_q_array, EOS_EVT_SIZE_Q);
}

int eos_evtq_push(unsigned char cmd, unsigned char *buffer, uint16_t len) {
    clear_csr(mstatus, MSTATUS_MIE);
    int ret = eos_msgq_push(&_eos_event_q, cmd, buffer, len);
    set_csr(mstatus, MSTATUS_MIE);
    return ret;
}

void eos_evtq_pop(unsigned char *cmd, unsigned char **buffer, uint16_t *len) {
    clear_csr(mstatus, MSTATUS_MIE);
    eos_msgq_pop(&_eos_event_q, cmd, buffer, len);
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_evtq_bad_handler(unsigned char cmd, unsigned char *buffer, uint16_t len) {
    write(1, "error\n", 6);
}

static void evtq_handler_wrapper(unsigned char cmd, unsigned char *buffer, uint16_t len, uint16_t *flags_acq, uint16_t flag, eos_evt_fptr_t f) {
    int ok = eos_net_acquire(*flags_acq & flag);
    if (ok) {
        f(cmd, buffer, len);
        eos_net_release();
        *flags_acq &= ~flag;
    } else {
        *flags_acq |= flag;
        eos_evtq_push(cmd, buffer, len);
    }
}

static void evtq_handler(unsigned char cmd, unsigned char *buffer, uint16_t len) {
    if (((cmd & EOS_EVT_MASK) >> 4) > EOS_EVT_MAX_EVT) {
        eos_evtq_bad_handler(cmd, buffer, len);
    } else {
        unsigned char idx = ((cmd & EOS_EVT_MASK) >> 4) - 1;
        uint16_t flag = (uint16_t)1 << idx;
        if (flag & evt_handler_flags_buf_acq) {
            evtq_handler_wrapper(cmd, buffer, len, &evt_handler_wrapper_acq, flag, evt_handler[idx]);
        } else {
            evt_handler[idx](cmd, buffer, len);
        }
    }
}

void eos_evtq_set_handler(unsigned char cmd, eos_evt_fptr_t handler, uint8_t flags) {
    if (flags) {
        uint16_t flag = (uint16_t)1 << (((cmd & EOS_EVT_MASK) >> 4) - 1);
        if (flags & EOS_EVT_FLAG_NET_BUF_ACQ) evt_handler_flags_buf_acq |= flag;
    }
    evt_handler[((cmd & EOS_EVT_MASK) >> 4) - 1] = handler;
}

void eos_evtq_loop(void) {
    unsigned char cmd;
    unsigned char *buffer;
    uint16_t len;
    int foo = 1;

    while(foo) {
        clear_csr(mstatus, MSTATUS_MIE);
        eos_msgq_pop(&_eos_event_q, &cmd, &buffer, &len);
        if (cmd) {
            set_csr(mstatus, MSTATUS_MIE);
            evtq_handler(cmd, buffer, len);
            clear_csr(mstatus, MSTATUS_MIE);
        } else {
            // asm volatile ("wfi");
        }
        set_csr(mstatus, MSTATUS_MIE);
    }
}


