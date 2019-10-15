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
static uint16_t evt_handler_wrapper_acq[EOS_EVT_MAX_EVT];
static uint16_t evt_handler_flags_buf_acq[EOS_EVT_MAX_EVT];

static volatile char evt_busy = 0;

void eos_evtq_init(void) {
    int i;
    
    for (i=0; i<EOS_EVT_MAX_EVT; i++) {
        evt_handler[i] = eos_evtq_bad_handler;
    }
    eos_msgq_init(&_eos_event_q, event_q_array, EOS_EVT_SIZE_Q);
}

int eos_evtq_push(unsigned char type, unsigned char *buffer, uint16_t len) {
    clear_csr(mstatus, MSTATUS_MIE);
    int ret = eos_msgq_push(&_eos_event_q, type, buffer, len);
    set_csr(mstatus, MSTATUS_MIE);
    return ret;
}

void eos_evtq_pop(unsigned char *type, unsigned char **buffer, uint16_t *len) {
    clear_csr(mstatus, MSTATUS_MIE);
    eos_msgq_pop(&_eos_event_q, type, buffer, len);
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_evtq_bad_handler(unsigned char type, unsigned char *buffer, uint16_t len) {
    write(1, "error\n", 6);
}

static void evtq_handler_wrapper(unsigned char type, unsigned char *buffer, uint16_t len) {
    unsigned char idx = ((type & EOS_EVT_MASK) >> 4) - 1;
    uint16_t flag = (uint16_t)1 << ((type & ~EOS_EVT_MASK) - 1);
    int ok;
    
    ok = eos_net_acquire(evt_handler_wrapper_acq[idx] & flag);
    if (ok) {
        evt_handler[idx](type, buffer, len);
        eos_net_release();
        evt_handler_wrapper_acq[idx] &= ~flag;
    } else {
        evt_handler_wrapper_acq[idx] |= flag;
        eos_evtq_push(type, buffer, len);
    }
}

static void evtq_handler(unsigned char type, unsigned char *buffer, uint16_t len) {
    unsigned char idx = ((type & EOS_EVT_MASK) >> 4) - 1;
    uint16_t flag = (uint16_t)1 << ((type & ~EOS_EVT_MASK) - 1);
    
    if (idx >= EOS_EVT_MAX_EVT) {
        eos_evtq_bad_handler(type, buffer, len);
        return;
    }
    if (flag & evt_handler_flags_buf_acq[idx]) {
        evtq_handler_wrapper(type, buffer, len);
    } else {
        evt_handler[idx](type, buffer, len);
    }
}

void eos_evtq_set_handler(unsigned char type, eos_evt_fptr_t handler) {
    unsigned char idx = ((type & EOS_EVT_MASK) >> 4) - 1;
    
    if (idx < EOS_EVT_MAX_EVT) evt_handler[idx] = handler;
}

void eos_evtq_set_flags(unsigned char type, uint8_t flags) {
    unsigned char idx = ((type & EOS_EVT_MASK) >> 4) - 1;
    uint16_t flag = type & ~EOS_EVT_MASK ? (uint16_t)1 << ((type & ~EOS_EVT_MASK) - 1) : 0xFFFF;

    if ((idx < EOS_EVT_MAX_EVT) && (flags & EOS_EVT_FLAG_NET_BUF_ACQ)) evt_handler_flags_buf_acq[idx] |= flag;
}

void eos_evtq_set_busy(char busy) {
    evt_busy = busy;
}

void eos_evtq_wait(unsigned char type, unsigned char *selector, uint16_t sel_len, unsigned char **buffer, uint16_t *len) {
    int rv = 0;

    while(!rv) {
        clear_csr(mstatus, MSTATUS_MIE);
        rv = eos_msgq_get(&_eos_event_q, type, selector, sel_len, buffer, len);
        if (!rv) {
            asm volatile ("wfi");
        }
        set_csr(mstatus, MSTATUS_MIE);
    }
}

void eos_evtq_loop(void) {
    unsigned char type;
    unsigned char *buffer;
    uint16_t len;
    int foo = 1;

    while(foo) {
        clear_csr(mstatus, MSTATUS_MIE);
        eos_msgq_pop(&_eos_event_q, &type, &buffer, &len);
        if (type) {
            set_csr(mstatus, MSTATUS_MIE);
            evtq_handler(type, buffer, len);
            clear_csr(mstatus, MSTATUS_MIE);
        } else {
            if (!evt_busy) asm volatile ("wfi");
        }
        set_csr(mstatus, MSTATUS_MIE);
    }
}


