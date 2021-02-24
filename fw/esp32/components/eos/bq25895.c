#include <stdlib.h>

#include <esp_log.h>

#include "eos.h"
#include "i2c.h"

static const char *TAG = "EOS BQ25895";

#define BQ25895_ADDR                0x6A

void eos_bq25895_set_ilim(void) {
    uint8_t data = 0;
    int ret = EOS_OK;
    eos_i2c_write8(BQ25895_ADDR, 0, 0x26);  // input current: 2.0 A
    eos_i2c_write8(BQ25895_ADDR, 2, 0x28);
    eos_i2c_write8(BQ25895_ADDR, 7, 0x8d);

    ret = eos_i2c_read8(BQ25895_ADDR, 0x00, &data);
    if (!ret) ESP_LOGI(TAG, "REG00: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x01, &data);
    if (!ret) ESP_LOGI(TAG, "REG01: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x02, &data);
    if (!ret) ESP_LOGI(TAG, "REG02: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x03, &data);
    if (!ret) ESP_LOGI(TAG, "REG03: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x04, &data);
    if (!ret) ESP_LOGI(TAG, "REG04: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x05, &data);
    if (!ret) ESP_LOGI(TAG, "REG05: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x06, &data);
    if (!ret) ESP_LOGI(TAG, "REG06: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x07, &data);
    if (!ret) ESP_LOGI(TAG, "REG07: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x08, &data);
    if (!ret) ESP_LOGI(TAG, "REG08: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x09, &data);
    if (!ret) ESP_LOGI(TAG, "REG09: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x0a, &data);
    if (!ret) ESP_LOGI(TAG, "REG0A: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x0b, &data);
    if (!ret) ESP_LOGI(TAG, "REG0B: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x0c, &data);
    if (!ret) ESP_LOGI(TAG, "REG0C: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x0d, &data);
    if (!ret) ESP_LOGI(TAG, "REG0D: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x0e, &data);
    if (!ret) ESP_LOGI(TAG, "REG0E: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x0f, &data);
    if (!ret) ESP_LOGI(TAG, "REG0F: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x10, &data);
    if (!ret) ESP_LOGI(TAG, "REG10: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x11, &data);
    if (!ret) ESP_LOGI(TAG, "REG11: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x12, &data);
    if (!ret) ESP_LOGI(TAG, "REG12: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x13, &data);
    if (!ret) ESP_LOGI(TAG, "REG13: %02x", data);
    ret = eos_i2c_read8(BQ25895_ADDR, 0x14, &data);
    if (!ret) ESP_LOGI(TAG, "REG14: %02x", data);
}
