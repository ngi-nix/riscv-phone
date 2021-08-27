#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "eos.h"
#include "i2c.h"
#include "i2c/bq25895.h"

void eos_bq25895_init(void) {
    uint8_t data = 0;
    int i, ret = EOS_OK;

    eos_i2c_start(100000);
    ret = eos_i2c_write8(BQ25895_ADDR, 0x07, 0x8d);   // disable watchdog
    if (ret) printf("I2C ERROR 0x07\n");
    ret = eos_i2c_write8(BQ25895_ADDR, 0x00, 0x28);   // 2.1A input current
    if (ret) printf("I2C ERROR 0x00\n");
    ret = eos_i2c_write8(BQ25895_ADDR, 0x03, 0x1e);   // sysmin 3.7, disable otg
    if (ret) printf("I2C ERROR 0x03\n");

    printf("BQ25895:\n");
    for (i=0; i<0x15; i++) {
        ret = eos_i2c_read8(BQ25895_ADDR, i, &data);
        if (!ret) printf("REG%02x: %02x\n", i, data);
    }
    eos_i2c_stop();
}
