#include <stdlib.h>

#include <esp_log.h>

#include "eos.h"
#include "i2c.h"

static const char *TAG = "EOS BQ25895";

#define BQ25895_ADDR                0x6A

void eos_bq25895_set_ilim(void) {
    uint8_t data = 0;
    eos_i2c_write8(BQ25895_ADDR, 0, 0x1c);
    eos_i2c_write8(BQ25895_ADDR, 2, 0x28);
    eos_i2c_write8(BQ25895_ADDR, 7, 0x8d);

    data = eos_i2c_read8(BQ25895_ADDR, 0x00);
    ESP_LOGI(TAG, "REG00: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x01);
    ESP_LOGI(TAG, "REG01: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x02);
    ESP_LOGI(TAG, "REG02: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x03);
    ESP_LOGI(TAG, "REG03: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x04);
    ESP_LOGI(TAG, "REG04: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x05);
    ESP_LOGI(TAG, "REG05: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x06);
    ESP_LOGI(TAG, "REG06: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x07);
    ESP_LOGI(TAG, "REG07: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x08);
    ESP_LOGI(TAG, "REG08: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x09);
    ESP_LOGI(TAG, "REG09: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x0a);
    ESP_LOGI(TAG, "REG0A: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x0b);
    ESP_LOGI(TAG, "REG0B: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x0c);
    ESP_LOGI(TAG, "REG0C: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x0d);
    ESP_LOGI(TAG, "REG0D: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x0e);
    ESP_LOGI(TAG, "REG0E: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x0f);
    ESP_LOGI(TAG, "REG0F: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x10);
    ESP_LOGI(TAG, "REG10: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x11);
    ESP_LOGI(TAG, "REG11: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x12);
    ESP_LOGI(TAG, "REG12: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x13);
    ESP_LOGI(TAG, "REG13: %02x", data);
    data = eos_i2c_read8(BQ25895_ADDR, 0x14);
    ESP_LOGI(TAG, "REG14: %02x", data);
}
