#include <unistd.h>
#include <sys/time.h>

#include <core.h>
#include <tm.h>

int ecp_tm_init(ECPContext *ctx) {
    return ECP_OK;
}

ecp_cts_t ecp_tm_abstime_ms(ecp_cts_t msec) {
    struct timeval tv;
    ecp_cts_t ms_now;

    gettimeofday(&tv, NULL);
    ms_now = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return ms_now + msec;
}

void ecp_tm_sleep_ms(ecp_cts_t msec) {
    usleep(msec*1000);
}

void ecp_tm_timer_set(ecp_cts_t next) {}