#include <stdio.h>

#include "event.h"
#include "interrupt.h"
#include "timer.h"
#include "power.h"
#include "i2s.h"
#include "uart.h"
#include "spi.h"
#include "spi_dev.h"
#include "sdcard.h"
#include "net.h"
#include "wifi.h"
#include "cell.h"
#include "sock.h"
#include "i2c/bq25895.h"
#include "eve/eve.h"

#include "eos.h"

static uint32_t eve_touch[6] = {0xfa46,0xfffffcf6,0x422fe,0xffffff38,0x10002,0xf3cb0};

void eos_init(void) {
    uint8_t wakeup_cause = eos_power_wakeup_cause();
    printf("WAKE:%d\n", wakeup_cause);

    eos_evtq_init();
    eos_intr_init();
    eos_timer_init();
    eos_i2s_init();
    eos_uart_init();
    eos_spi_init();
    eos_spi_dev_init();
    eos_sdc_init();
    eos_net_init();
    eos_power_init();
    eos_wifi_init();
    eos_cell_init();
    eos_sock_init();
    eos_bq25895_init();

    eos_net_wake(wakeup_cause);

    eve_set_touch_calibration(eve_touch);
    eve_init(wakeup_cause == EOS_PWR_WAKE_RST);
}
