#include <stdio.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"
#include "msgq.h"
#include "net.h"
#include "timer.h"
#include "event.h"

static EOSMsgQ event_q;
static EOSMsgItem event_q_array[EOS_EVT_SIZE_Q];

static eos_evt_fptr_t evt_net_handler[EOS_NET_MAX_CMD];
static eos_evt_fptr_t evt_timer_handler = NULL;
static eos_evt_fptr_t evt_ui_handler = NULL;

static void bad_handler(unsigned char cmd, unsigned char *buffer, uint16_t len) {
    printf("bad handler: %d\n", cmd);
}

void eos_evtq_init(void) {
    int i;
    
    for (i=0; i<EOS_NET_MAX_CMD; i++) {
        evt_net_handler[i] = bad_handler;
    }
    evt_timer_handler = bad_handler;
    evt_ui_handler = bad_handler;
    eos_msgq_init(&event_q, event_q_array, EOS_EVT_SIZE_Q);
}

int eos_evtq_push(unsigned char cmd, unsigned char *buffer, uint16_t len) {
    return eos_msgq_push(&event_q, cmd, buffer, len);
}

void eos_evtq_pop(unsigned char *cmd, unsigned char **buffer, uint16_t *len) {
    eos_msgq_pop(&event_q, cmd, buffer, len);
}

void eos_evtq_handle(unsigned char cmd, unsigned char *buffer, uint16_t len) {
    if (cmd & EOS_EVT_MASK_NET) {
        cmd &= ~EOS_EVT_MASK_NET;
        if (cmd < EOS_NET_MAX_CMD) {
            evt_net_handler[cmd](cmd, buffer, len);
        } else {
            bad_handler(cmd, buffer, len);
        }
    } else if (cmd == EOS_EVT_TIMER) {
        evt_timer_handler(cmd, buffer, len);
    } else if (cmd == EOS_EVT_UI) {
        evt_ui_handler(cmd, buffer, len);
    }
}

void eos_evtq_loop(void) {
    unsigned char cmd;
    unsigned char *buffer;
    uint16_t len;
    volatile int foo = 1;

    while(foo) {
        clear_csr(mstatus, MSTATUS_MIE);
        eos_evtq_pop(&cmd, &buffer, &len);
        if (cmd) {
            set_csr(mstatus, MSTATUS_MIE);
            eos_evtq_handle(cmd, buffer, len);
            clear_csr(mstatus, MSTATUS_MIE);
        } else {
            // asm volatile ("wfi");
        }
        set_csr(mstatus, MSTATUS_MIE);
    }
}

void eos_evtq_set_handler(unsigned char cmd, eos_evt_fptr_t handler) {
    if (cmd & EOS_EVT_MASK_NET) {
        evt_net_handler[cmd & ~EOS_EVT_MASK_NET] = handler;
    } else if (cmd == EOS_EVT_TIMER) {
        evt_timer_handler = handler;
    } else if (cmd == EOS_EVT_UI) {
        evt_ui_handler = handler;
    }
}