#include <stdio.h>

#include "event.h"
#include "interrupt.h"
#include "timer.h"
#include "pwr.h"
#include "i2s.h"
#include "i2c.h"
#include "uart.h"
#include "spi.h"
#include "spi_dev.h"
#include "lcd.h"
#include "sdcard.h"
#include "net.h"
#include "wifi.h"
#include "cell.h"
#include "sock.h"
#include "i2c/bq25895.h"
#include "eve/eve_eos.h"

#include "board.h"

#include "eos.h"

void eos_init(void) {
    uint8_t wakeup_cause = eos_pwr_wakeup_cause();
    uint32_t touch_matrix[6] = {0xfa46,0xfffffcf6,0x422fe,0xffffff38,0x10002,0xf3cb0};
    int touch_calibrate = 0;
    int rv;

    eos_evtq_init(wakeup_cause);
    eos_intr_init(wakeup_cause);
    eos_timer_init(wakeup_cause);
    eos_uart_init(wakeup_cause);

    printf("INIT:%d\n", wakeup_cause);

    rv = eos_pwr_init(wakeup_cause);
    if (rv) printf("PWR INIT ERR:%d\n", rv);
    rv = eos_i2s_init(wakeup_cause);
    if (rv) printf("I2S INIT ERR:%d\n", rv);
    rv = eos_i2c_init(wakeup_cause);
    if (rv) printf("I2C INIT ERR:%d\n", rv);
    rv = eos_spi_init(wakeup_cause);
    if (rv) printf("SPI INIT ERR:%d\n", rv);
    rv = eos_spi_dev_init(wakeup_cause);
    if (rv) printf("SPI DEV INIT ERR:%d\n", rv);
    rv = eos_sdc_init(wakeup_cause);
    if (rv) printf("SDC INIT ERR:%d\n", rv);
    rv = eos_net_init(wakeup_cause);
    if (rv) printf("NET INIT ERR:%d\n", rv);

    eos_i2c_start();
    rv = eos_bq25895_init(wakeup_cause);
    eos_i2c_stop();
    if (rv) printf("BQ25895 INIT ERR:%d\n", rv);

    rv = eos_eve_init(wakeup_cause, EVE_GPIO_DIR, touch_calibrate, touch_matrix);
    if (rv) printf("EVE INIT ERR:%d\n", rv);

    rv = eos_lcd_init(wakeup_cause);
    if (rv) printf("LCD INIT ERR:%d\n", rv);

    if (touch_calibrate) {
        printf("TOUCH MATRIX:\n");
        printf("uint32_t touch_matrix[6] = {0x%x,0x%x,0x%x,0x%x,0x%x,0x%x}\n", touch_matrix[0], touch_matrix[1], touch_matrix[2], touch_matrix[3], touch_matrix[4], touch_matrix[5]);
    }
    eos_run(wakeup_cause);
}

void eos_run(uint8_t wakeup_cause) {
    int rv;

    rv = eos_eve_run(wakeup_cause);
    if (rv) printf("EVE RUN ERR:%d\n", rv);

    eos_pwr_netinit();
    eos_wifi_netinit();
    eos_cell_netinit();
    eos_sock_netinit();
    rv = eos_net_run(wakeup_cause);
    if (rv) printf("NET RUN ERR:%d\n", rv);
}
