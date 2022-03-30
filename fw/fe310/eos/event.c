#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"
#include "msgq.h"
#include "event.h"

EOSMsgQ _eos_event_q;
static EOSMsgItem event_q_array[EOS_EVT_SIZE_Q];

static eos_evt_handler_t evt_handler[EOS_EVT_MAX_EVT + 1];

static void evtq_handler(unsigned char type, unsigned char *buffer, uint16_t len) {
    unsigned char idx = (type & EOS_EVT_MASK) >> 4;

    if (idx && (idx <= EOS_EVT_MAX_EVT)) {
        evt_handler[idx](type, buffer, len);
    } else {
        eos_evtq_bad_handler(type, buffer, len);
    }
}

int eos_evtq_init(uint8_t wakeup_cause) {
    int i;

    evt_handler[0] = evtq_handler;
    for (i=0; i<EOS_EVT_MAX_EVT; i++) {
        evt_handler[i + 1] = eos_evtq_bad_handler;
    }
    eos_msgq_init(&_eos_event_q, event_q_array, EOS_EVT_SIZE_Q);

    return EOS_OK;
}

int eos_evtq_push(unsigned char type, unsigned char *buffer, uint16_t len) {
    clear_csr(mstatus, MSTATUS_MIE);
    int ret = eos_msgq_push(&_eos_event_q, type, buffer, len);
    set_csr(mstatus, MSTATUS_MIE);
    return ret;
}

int eos_evtq_push_isr(unsigned char type, unsigned char *buffer, uint16_t len) {
    return eos_msgq_push(&_eos_event_q, type, buffer, len);
}

void eos_evtq_pop(unsigned char *type, unsigned char **buffer, uint16_t *len) {
    clear_csr(mstatus, MSTATUS_MIE);
    eos_msgq_pop(&_eos_event_q, type, buffer, len);
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_evtq_pop_isr(unsigned char *type, unsigned char **buffer, uint16_t *len) {
    eos_msgq_pop(&_eos_event_q, type, buffer, len);
}

int eos_evtq_get(unsigned char type, unsigned char **buffer, uint16_t *len) {
    int rv = 0;

    clear_csr(mstatus, MSTATUS_MIE);
    rv = eos_msgq_find(&_eos_event_q, type, NULL, 0, buffer, len);
    set_csr(mstatus, MSTATUS_MIE);
}

int eos_evtq_find(unsigned char type, unsigned char *selector, uint16_t sel_len, unsigned char **buffer, uint16_t *len) {
    int rv = 0;

    clear_csr(mstatus, MSTATUS_MIE);
    rv = eos_msgq_find(&_eos_event_q, type, selector, sel_len, buffer, len);
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_evtq_wait(unsigned char type, unsigned char *selector, uint16_t sel_len, unsigned char **buffer, uint16_t *len) {
    int rv = 0;

    while(!rv) {
        clear_csr(mstatus, MSTATUS_MIE);
        rv = eos_msgq_find(&_eos_event_q, type, selector, sel_len, buffer, len);
        if (!rv) {
            unsigned char _type;
            unsigned char *_buffer;
            uint16_t _len;

            eos_msgq_pop(&_eos_event_q, &_type, &_buffer, &_len);
            if (_type) {
                set_csr(mstatus, MSTATUS_MIE);
                evt_handler[0](_type, _buffer, _len);
            } else {
                asm volatile ("wfi");
                set_csr(mstatus, MSTATUS_MIE);
            }
        } else {
            set_csr(mstatus, MSTATUS_MIE);
        }
    }
}

void eos_evtq_flush(void) {
    clear_csr(mstatus, MSTATUS_MIE);
    eos_evtq_flush_isr();
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_evtq_flush_isr(void) {
    unsigned char type;
    unsigned char *buffer;
    uint16_t len;

    do {
        eos_msgq_pop(&_eos_event_q, &type, &buffer, &len);
        if (type) {
            set_csr(mstatus, MSTATUS_MIE);
            evt_handler[0](type, buffer, len);
            clear_csr(mstatus, MSTATUS_MIE);
        }
    } while (type);
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
            evt_handler[0](type, buffer, len);
        } else {
            asm volatile ("wfi");
            set_csr(mstatus, MSTATUS_MIE);
        }
    }
}

void eos_evtq_bad_handler(unsigned char type, unsigned char *buffer, uint16_t len) {
    printf("EVT BAD HANDLER:0x%x\n", type);
}

void eos_evtq_set_handler(unsigned char type, eos_evt_handler_t handler) {
    unsigned char idx = (type & EOS_EVT_MASK) >> 4;

    if (handler == NULL) handler = eos_evtq_bad_handler;
    if (idx <= EOS_EVT_MAX_EVT) evt_handler[idx] = handler;
}

eos_evt_handler_t eos_evtq_get_handler(unsigned char type) {
    unsigned char idx = (type & EOS_EVT_MASK) >> 4;

    if (idx <= EOS_EVT_MAX_EVT) return evt_handler[idx];
    return NULL;
}
