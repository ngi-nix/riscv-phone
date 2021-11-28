#include <stdlib.h>
#include <stdint.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"
#include "msgq.h"
#include "event.h"
#include "timer.h"

#define MIN(X, Y)               (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)               (((X) > (Y)) ? (X) : (Y))

static eos_timer_handler_t timer_handler[EOS_TIMER_MAX_ETYPE + 1];
static uint64_t timer_next[EOS_TIMER_MAX_ETYPE + 1];

static void timer_handle_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    unsigned char idx = (type & ~EOS_EVT_MASK);

    if (idx && (idx <= EOS_TIMER_MAX_ETYPE) && timer_handler[idx]) {
        timer_handler[idx](type);
    } else {
        eos_evtq_bad_handler(type, buffer, len);
    }
}

void _eos_timer_handle(void) {
    int i;
    volatile uint64_t *mtime = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIME);
    uint64_t *mtimecmp = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);
    uint64_t now = *mtime;
    uint64_t next = 0;

    for (i = 0; i <= EOS_TIMER_MAX_ETYPE; i++) {
        if (timer_next[i] && (timer_next[i] <= now)) {
            timer_next[i] = 0;
            if (i == 0) {
                timer_handler[0](0);
            } else {
                eos_evtq_push_isr(EOS_EVT_TIMER | i, NULL, 0);
            }
        }
        next = next && timer_next[i] ? MIN(next, timer_next[i]) : (next ? next : timer_next[i]);
    }
    *mtimecmp = next;
    if (*mtimecmp == 0) clear_csr(mie, MIP_MTIP);
}

int eos_timer_init(uint8_t wakeup_cause) {
    int i;
    uint64_t *mtimecmp = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);

    clear_csr(mie, MIP_MTIP);
    *mtimecmp = 0;
    for (i=0; i<=EOS_TIMER_MAX_ETYPE; i++) {
        timer_next[i] = 0;
        timer_handler[i] = NULL;
    }
    eos_evtq_set_handler(EOS_EVT_TIMER, timer_handle_evt);

    return EOS_OK;
}

void eos_timer_set_handler(unsigned char evt, eos_timer_handler_t handler) {
    uint64_t *mtimecmp = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);

    if (!evt && (*mtimecmp != 0)) clear_csr(mie, MIP_MTIP);
    timer_handler[evt] = handler;
    if (!evt && (*mtimecmp != 0)) set_csr(mie, MIP_MTIP);
}

uint32_t eos_timer_get(unsigned char evt) {
    volatile uint64_t *mtime = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIME);
    uint64_t *mtimecmp = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);
    uint64_t now;
    uint32_t ret;

    if (*mtimecmp != 0) clear_csr(mie, MIP_MTIP);
    now = *mtime;
    if (timer_next[evt]) {
        ret = (timer_next[evt] > now) ? (timer_next[evt] - now) * 1000 / EOS_TIMER_RTC_FREQ : 0;
    } else {
        ret = EOS_TIMER_NONE;
    }
    if (*mtimecmp != 0) set_csr(mie, MIP_MTIP);

    return ret;
}

void eos_timer_set(uint32_t msec, unsigned char evt) {
    int i;
    volatile uint64_t *mtime = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIME);
    uint64_t *mtimecmp = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);
    uint64_t tick = *mtime + msec * (uint64_t)EOS_TIMER_RTC_FREQ / 1000;
    uint64_t next = 0;

    if (*mtimecmp != 0) clear_csr(mie, MIP_MTIP);
    timer_next[evt] = tick;
    for (i = 0; i <= EOS_TIMER_MAX_ETYPE; i++) {
        next = next && timer_next[i] ? MIN(next, timer_next[i]) : (next ? next : timer_next[i]);
    }
    *mtimecmp = next;
    if (*mtimecmp != 0) set_csr(mie, MIP_MTIP);
}

void eos_timer_clear(unsigned char evt) {
    int i;
    uint64_t *mtimecmp = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);
    uint64_t next = 0;

    if (*mtimecmp != 0) clear_csr(mie, MIP_MTIP);
    if (timer_next[evt]) {
        timer_next[evt] = 0;
        for (i = 0; i <= EOS_TIMER_MAX_ETYPE; i++) {
            next = next && timer_next[i] ? MIN(next, timer_next[i]) : (next ? next : timer_next[i]);
        }
        *mtimecmp = next;
    }
    if (*mtimecmp != 0) set_csr(mie, MIP_MTIP);
}


void eos_time_sleep(uint32_t msec) {
    volatile uint64_t *mtime = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIME);
    uint64_t now_ms = *mtime * 1000 / EOS_TIMER_RTC_FREQ;

    while (*mtime * 1000 / EOS_TIMER_RTC_FREQ < now_ms + msec);
}

uint64_t eos_time_get_tick(void) {
    volatile uint64_t *mtime = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIME);
    return *mtime;
}

uint32_t eos_time_since(uint32_t start) {
    return (eos_time_get_tick() - start) * 1000 / EOS_TIMER_RTC_FREQ;
 }
