#include <core.h>

#include "encoding.h"
#include "platform.h"

static ecp_cts_t t_abstime_ms(ecp_cts_t msec) {
    volatile uint64_t *mtime = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIME);
    
    uint64_t now_ms = *mtime * 1000 / RTC_FREQ;
    return now_ms + msec;
}

int ecp_time_init(ECPTimeIface *t) {
    t->init = 1;
    t->abstime_ms = t_abstime_ms;
    t->sleep_ms = NULL;
    return 0;
}
