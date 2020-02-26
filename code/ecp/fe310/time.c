#include <core.h>
#include <tr.h>

#include <eos/eos.h>
#include <eos/net.h>
#include <eos/event.h>
#include <eos/timer.h>

#include "encoding.h"
#include "platform.h"

extern ECPSocket *_ecp_tr_sock;

static void timer_handler(unsigned char type) {
    ecp_cts_t next = ecp_timer_exe(_ecp_tr_sock);
    if (next) {
        eos_timer_set(next, EOS_TIMER_ETYPE_ECP, 0);
    }
}

int ecp_tm_init(ECPContext *ctx) {
    eos_timer_set_handler(EOS_TIMER_ETYPE_ECP, timer_handler);
    eos_net_acquire_for_evt(EOS_EVT_TIMER | EOS_TIMER_ETYPE_ECP, 1);
    return ECP_OK;
}

ecp_cts_t ecp_tm_abstime_ms(ecp_cts_t msec) {
    volatile uint64_t *mtime = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIME);

    uint64_t now_ms = *mtime * 1000 / RTC_FREQ;
    return now_ms + msec;
}

void ecp_tm_sleep_ms(ecp_cts_t msec) {
    eos_timer_sleep(msec);
}

void ecp_tm_timer_set(ecp_cts_t next) {
    eos_timer_set(next, EOS_TIMER_ETYPE_ECP, 1);
}
