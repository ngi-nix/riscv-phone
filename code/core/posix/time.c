#include <unistd.h>
#include <sys/time.h>

#include <core.h>

static unsigned int t_abstime_ms(unsigned int msec) {
    struct timeval tv;
    unsigned int ms_now;
    
    gettimeofday(&tv, NULL);
    ms_now = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return ms_now + msec;
}

static void t_sleep_ms(unsigned int msec) {
    usleep(msec*1000);
}

int ecp_time_init(ECPTimeIface *t) {
    t->init = 1;
    t->abstime_ms = t_abstime_ms;
    t->sleep_ms = t_sleep_ms;
    return 0;
}
