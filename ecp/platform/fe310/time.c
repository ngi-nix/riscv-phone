#include <core.h>
#include <tr.h>

#include <eos/eos.h>
#include <eos/net.h>
#include <eos/event.h>
#include <eos/timer.h>

extern ECPSocket *_ecp_tr_sock;

static void timer_handler(unsigned char type) {
    ecp_cts_t next = ecp_timer_exe(_ecp_tr_sock);
    if (next) {
        eos_timer_set(next, EOS_TIMER_ETYPE_ECP);
    }
}

int ecp_tm_init(ECPContext *ctx) {
    eos_timer_set_handler(EOS_TIMER_ETYPE_ECP, timer_handler);
    eos_net_acquire_for_evt(EOS_EVT_TIMER | EOS_TIMER_ETYPE_ECP, 1);
    return ECP_OK;
}

ecp_cts_t ecp_tm_abstime_ms(ecp_cts_t msec) {
    return eos_time_get_tick() * 1000 / EOS_TIMER_RTC_FREQ + msec;
}

void ecp_tm_sleep_ms(ecp_cts_t msec) {
    eos_time_sleep(msec);
}

void ecp_tm_timer_set(ecp_cts_t next) {
    uint32_t _next = eos_timer_get(EOS_TIMER_ETYPE_ECP);
    if ((_next == EOS_TIMER_NONE) || (next < _next)) eos_timer_set(next, EOS_TIMER_ETYPE_ECP);
}
