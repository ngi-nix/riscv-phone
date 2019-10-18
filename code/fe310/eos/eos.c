#include "event.h"
#include "interrupt.h"
#include "timer.h"
#include "spi.h"
#include "net.h"
#include "i2s.h"

#include "eos.h"

void eos_init(void) {
    eos_evtq_init();
    eos_intr_init();
    eos_timer_init();
    eos_net_init();
    eos_spi_init();
    eos_i2s_init();
    eos_net_start();
}
