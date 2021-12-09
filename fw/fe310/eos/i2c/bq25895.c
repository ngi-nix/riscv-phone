#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "eos.h"
#include "pwr.h"
#include "i2c.h"
#include "i2c/bq25895.h"

static int reg_read(uint8_t reg, uint8_t *data) {
    return eos_i2c_read8(BQ25895_ADDR, reg, data, 1);
}

static int reg_write(uint8_t reg, uint8_t data) {
    return eos_i2c_write8(BQ25895_ADDR, reg, &data, 1);
}

int eos_bq25895_init(uint8_t wakeup_cause) {
    int rst = (wakeup_cause == EOS_PWR_WAKE_RST);
    int i, rv = EOS_OK;
    uint8_t data = 0;

    if (rst) {
        rv = reg_write(0x14, 0x80);  // reset
        if (rv) printf("I2C ERROR 0x14\n");
        rv = reg_write(0x14, 0x00);  // disable watchdog
        if (rv) printf("I2C ERROR 0x14\n");
        rv = reg_write(0x07, 0x8d);  // disable watchdog
        if (rv) printf("I2C ERROR 0x07\n");
        rv = reg_write(0x00, 0x28);  // 2.1A input current
        if (rv) printf("I2C ERROR 0x00\n");
        rv = reg_write(0x02, 0x30);  // enable ICO, disaable MaxCharge and D+/D-
        if (rv) printf("I2C ERROR 0x02\n");
    }

    printf("BQ25895:\n");
    for (i=0; i<0x15; i++) {
        rv = reg_read(i, &data);
        if (!rv) printf("REG%02x: %02x\n", i, data);
    }

    return EOS_OK;
}
