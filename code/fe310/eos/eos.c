#include "event.h"
#include "interrupt.h"
#include "timer.h"
#include "net.h"

#include "eos.h"

void eos_init(void) {
    eos_evtq_init();
    eos_intr_init();
    eos_timer_init();
    eos_net_init();
}

void eos_start(void) {
    eos_net_start(9);
}