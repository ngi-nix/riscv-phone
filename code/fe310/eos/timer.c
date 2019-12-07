#include <stdlib.h>
#include <stdint.h>

#include "encoding.h"
#include "platform.h"

#include "msgq.h"
#include "event.h"
#include "timer.h"

#define MIN(X, Y)               (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)               (((X) > (Y)) ? (X) : (Y))

static eos_timer_fptr_t timer_handler[EOS_TIMER_MAX_ETYPE + 1];
static uint64_t timer_next[EOS_TIMER_MAX_ETYPE + 1];

static void timer_handler_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    unsigned char idx = (type & ~EOS_EVT_MASK) - 1;

    if ((idx < EOS_TIMER_MAX_ETYPE) && timer_handler[idx]) {
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
            if (i == EOS_TIMER_MAX_ETYPE) {
                timer_handler[EOS_TIMER_MAX_ETYPE](0);
            } else {
                eos_evtq_push_isr(EOS_EVT_TIMER | i + 1, NULL, 0);
            }
        }
        next = next && timer_next[i] ? MIN(next, timer_next[i]) : (next ? next : timer_next[i]);
    }
    *mtimecmp = next;
    if (*mtimecmp == 0) clear_csr(mie, MIP_MTIP);
}

void eos_timer_init(void) {
    int i;
    uint64_t *mtimecmp = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);

    clear_csr(mie, MIP_MTIP);
    *mtimecmp = 0;
    for (i=0; i<=EOS_TIMER_MAX_ETYPE; i++) {
        timer_next[i] = 0;
        timer_handler[i] = NULL;
    }
    eos_evtq_set_handler(EOS_EVT_TIMER, timer_handler_evt);
}

void eos_timer_set_handler(unsigned char evt, eos_timer_fptr_t handler, uint8_t flags) {
    uint64_t *mtimecmp = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);

    if (evt && (evt <= EOS_TIMER_MAX_ETYPE)) {
        evt--;
    } else if (evt == 0) {
        evt = EOS_TIMER_MAX_ETYPE;
    } else {
        return;
    }

    clear_csr(mie, MIP_MTIP);
    timer_handler[evt] = handler;
    if (*mtimecmp != 0) set_csr(mie, MIP_MTIP);

    if ((evt != EOS_TIMER_MAX_ETYPE) && flags) eos_evtq_set_flags(EOS_EVT_TIMER | evt + 1, flags);
}

uint64_t eos_timer_get(unsigned char evt) {
    uint64_t *mtimecmp = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);
    uint64_t ret = 0;

    if (evt && (evt <= EOS_TIMER_MAX_ETYPE)) {
        evt--;
    } else if (evt == 0) {
        evt = EOS_TIMER_MAX_ETYPE;
    } else {
        return 0;
    }

    clear_csr(mie, MIP_MTIP);
    ret = timer_next[evt];
    if (*mtimecmp != 0) set_csr(mie, MIP_MTIP);

    return ret;
}

void eos_timer_set(uint32_t msec, unsigned char evt, unsigned char b) {
    int i;
    volatile uint64_t *mtime = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIME);
    uint64_t *mtimecmp = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);
    uint64_t tick = *mtime + msec * (uint64_t)RTC_FREQ / 1000;
    uint64_t next = 0;

    if (evt && (evt <= EOS_TIMER_MAX_ETYPE)) {
        evt--;
    } else if (evt == 0) {
        evt = EOS_TIMER_MAX_ETYPE;
    } else {
        return;
    }

    clear_csr(mie, MIP_MTIP);
    if (!b || (timer_next[evt] == 0) || (tick < timer_next[evt])) {
        timer_next[evt] = tick;
        for (i = 0; i <= EOS_TIMER_MAX_ETYPE; i++) {
            next = next && timer_next[i] ? MIN(next, timer_next[i]) : (next ? next : timer_next[i]);
        }
        *mtimecmp = next;
    }
    if (*mtimecmp != 0) set_csr(mie, MIP_MTIP);
}

void eos_timer_clear(unsigned char evt) {
    int i;
    uint64_t *mtimecmp = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);
    uint64_t next = 0;

    if (evt && (evt <= EOS_TIMER_MAX_ETYPE)) {
        evt--;
    } else if (evt == 0) {
        evt = EOS_TIMER_MAX_ETYPE;
    } else {
        return;
    }

    clear_csr(mie, MIP_MTIP);
    if (timer_next[evt]) {
        timer_next[evt] = 0;
        for (i = 0; i <= EOS_TIMER_MAX_ETYPE; i++) {
            next = next && timer_next[i] ? MIN(next, timer_next[i]) : (next ? next : timer_next[i]);
        }
        *mtimecmp = next;
    }
    if (*mtimecmp != 0) set_csr(mie, MIP_MTIP);
}

void eos_timer_sleep(uint32_t msec) {
    volatile uint64_t *mtime = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIME);

    uint64_t now_ms = *mtime * 1000 / RTC_FREQ;
    while (*mtime * 1000 / RTC_FREQ < now_ms + msec);
}
