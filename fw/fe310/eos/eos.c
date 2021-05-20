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

#include "board.h"

#include "eos.h"

void eos_init(void) {
    uint8_t wakeup_cause = eos_power_wakeup_cause();
    uint32_t touch_matrix[6] = {0xfa46,0xfffffcf6,0x422fe,0xffffff38,0x10002,0xf3cb0};
    int touch_calibrate = 0;

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

    eos_net_start(wakeup_cause);

    eve_init(wakeup_cause == EOS_PWR_WAKE_RST, touch_calibrate, touch_matrix, EVE_GPIO_DIR);
    if (touch_calibrate) {
        printf("TOUCH MATRIX:\n");
        printf("uint32_t touch_matrix[6] = {0x%x,0x%x,0x%x,0x%x,0x%x,0x%x}\n", touch_matrix[0], touch_matrix[1], touch_matrix[2], touch_matrix[3], touch_matrix[4], touch_matrix[5]);
    }
}
