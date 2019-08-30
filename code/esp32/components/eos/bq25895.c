#include <stdio.h>
#include <esp_log.h>

#include "eos.h"
#include "i2c.h"

static const char *TAG = "BQ25895";

#define BQ25895_ADDR                0x6A

/**
 * @brief test function to show buffer
 */
static void disp_buf(uint8_t *buf, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        printf("%02x ", buf[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }
    printf("\n");
}

void eos_bq25895_set_ilim(void) {
    uint8_t data = 0;
    eos_i2c_write8(BQ25895_ADDR, 0, 0x3E);
    data = eos_i2c_read8(BQ25895_ADDR, 0);
    ESP_LOGI(TAG, "BUFFER: %02x", data);
}