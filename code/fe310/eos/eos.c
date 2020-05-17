#include "event.h"
#include "interrupt.h"
#include "timer.h"
#include "power.h"
#include "i2s.h"
#include "uart.h"
#include "spi.h"
#include "net.h"
#include "wifi.h"
#include "cell.h"
#include "sock.h"
#include "eve/eve.h"

#include "eos.h"

uint32_t eve_touch[6] = {0xfa46,0xfffffcf6,0x422fe,0xffffff38,0x10002,0xf3cb0};

#include <stdio.h>

void eos_init(void) {
    uint8_t wake_src = eos_power_cause_wake();
    printf("WAKE SRC:%d\n", wake_src);

    eos_evtq_init();
    eos_intr_init();
    eos_timer_init();
    eos_i2s_init();
    eos_uart_init();
    eos_spi_init();
    eos_net_init();
    eos_power_init();
    eos_wifi_init();
    eos_cell_init();
    eos_sock_init();
    eos_spi_dev_init();

    int rv = eos_net_wake(wake_src);
    printf("NET WAKE:%d\n", wake_src);

    eve_set_touch_calibration(eve_touch);
    eos_spi_dev_start(EOS_DEV_DISP);
    eve_init(wake_src == EOS_PWR_WAKE_RESET);
    eos_spi_dev_stop();
}
