#include <core.h>

#include <eos/timer.h>

#include "encoding.h"
#include "platform.h"

static ecp_cts_t t_abstime_ms(ecp_cts_t msec) {
    volatile uint64_t *mtime = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIME);
    
    uint64_t now_ms = *mtime * 1000 / RTC_FREQ;
    return now_ms + msec;
}

static void t_timer_set(ecp_cts_t next) {
    uint32_t tick = next * (uint64_t)RTC_FREQ / 1000;
    eos_timer_set(tick, 1);
}

int ecp_time_init(ECPTimeIface *t) {
    t->init = 1;
    t->abstime_ms = t_abstime_ms;
    t->timer_set = t_timer_set;
    return 0;
}
