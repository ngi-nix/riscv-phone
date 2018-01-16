#include <stddef.h>
#include <stdio.h>

#include "encoding.h"
#include "platform.h"
#include "event.h"
#include "timer.h"

#define MIN(X, Y)               (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)               (((X) > (Y)) ? (X) : (Y))

static eos_timer_fptr_t timer_ext_handler = NULL;
volatile uint64_t timer_next = 0;
volatile uint64_t timer_next_evt = 0;

void handle_m_time_interrupt(void) {
    volatile uint64_t *mtime = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIME);
    volatile uint64_t *mtimecmp = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);
    uint64_t now = *mtime;

    if (timer_next && (timer_next <= now)) {
        uint32_t next = timer_ext_handler();
        if (next) {
            timer_next = now + next;
        } else {
            timer_next = 0;
        }
    }
    if (timer_next_evt && (timer_next_evt <= now)) {
        eos_evtq_push(EOS_EVT_TIMER, NULL, 0);
        timer_next_evt = 0;
    }
    *mtimecmp = (timer_next && timer_next_evt) ? MIN(timer_next, timer_next_evt) : (timer_next ? timer_next : timer_next_evt);
    if (*mtimecmp == 0) clear_csr(mie, MIP_MTIP);
}

void eos_timer_init(void) {
    volatile uint64_t *mtimecmp = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);
    *mtimecmp = 0;
    clear_csr(mie, MIP_MTIP);
}

void eos_timer_set(uint32_t tick, unsigned char is_evt) {
    volatile uint64_t *mtime = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIME);
    volatile uint64_t *mtimecmp = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);

    clear_csr(mstatus, MSTATUS_MIE);

    uint64_t now = *mtime;
    uint64_t then = *mtimecmp;
    uint64_t next = now + tick;
    if (is_evt) {
        if ((timer_next_evt == 0) || (next < timer_next_evt)) timer_next_evt = next;
        next = timer_next ? MIN(timer_next, timer_next_evt) : timer_next_evt;
    } else if (timer_ext_handler) {
        if ((timer_next == 0) || (next < timer_next)) timer_next = next;
        next = timer_next_evt ? MIN(timer_next, timer_next_evt) : timer_next;
    }
    if ((then == 0) || (next < then)) *mtimecmp = next;
    if (then == 0) set_csr(mie, MIP_MTIP);

    set_csr(mstatus, MSTATUS_MIE);
}

void eos_timer_clear(unsigned char is_evt) {
    volatile uint64_t *mtimecmp = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);

    clear_csr(mstatus, MSTATUS_MIE);

    if (is_evt) {
        timer_next_evt = 0;
        *mtimecmp = timer_next;
    } else {
        timer_next = 0;
        *mtimecmp = timer_next_evt;
    }
    if (*mtimecmp == 0) clear_csr(mie, MIP_MTIP);

    set_csr(mstatus, MSTATUS_MIE);
}

void eos_timer_set_handler(eos_timer_fptr_t handler) {
    clear_csr(mstatus, MSTATUS_MIE);
    timer_ext_handler = handler;
    set_csr(mstatus, MSTATUS_MIE);
}
