#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "eos.h"
#include "i2c.h"
#include "i2c/bq25895.h"

void eos_bq25895_init(void) {
    uint8_t data = 0;
    int i, ret = EOS_OK;

    eos_i2c_start(400000);
    eos_i2c_write8(BQ25895_ADDR, 0, 0x26);  // input current: 2.0 A
    eos_i2c_write8(BQ25895_ADDR, 2, 0x28);
    eos_i2c_write8(BQ25895_ADDR, 7, 0x8d);

    printf("BQ25895:\n");
    for (i=0; i<0x15; i++) {
        ret = eos_i2c_read8(BQ25895_ADDR, i, &data);
        if (!ret) printf("REG%02x: %02x\n", i, data);
    }
    eos_i2c_stop();
}
