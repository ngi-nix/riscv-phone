#include <core.h>
#include <tr.h>

#include <eos/timer.h>

#include "encoding.h"
#include "platform.h"

ecp_cts_t ecp_tm_abstime_ms(ecp_cts_t msec) {
    volatile uint64_t *mtime = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIME);
    
    uint64_t now_ms = *mtime * 1000 / RTC_FREQ;
    return now_ms + msec;
}

void ecp_tm_sleep_ms(ecp_cts_t msec) {
    volatile uint64_t *mtime = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIME);
    
    uint64_t now_ms = *mtime * 1000 / RTC_FREQ;
    while (*mtime * 1000 / RTC_FREQ < now_ms + msec);
}

void ecp_tm_timer_set(ecp_cts_t next) {
    uint32_t tick = next * (uint64_t)RTC_FREQ / 1000;
    eos_timer_set(tick, EOS_TIMER_ETYPE_ECP);
}
