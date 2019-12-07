#include <stdio.h>
#include <esp_log.h>
#include <driver/i2c.h>

#include "eos.h"

static const char *TAG = "EOS I2C";

#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000
#define I2C_MASTER_GPIO_SCL         25
#define I2C_MASTER_GPIO_SDA         26

#define I2C_MASTER_TX_BUF_DISABLE   0       /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0       /*!< I2C master doesn't need buffer */
#define ACK_CHECK_EN                0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS               0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL                     0x0     /*!< I2C ack value */
#define NCK_VAL                     0x1     /*!< I2C nack value */

/**
 * @brief i2c initialization
 */

void eos_i2c_init(void) {
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_GPIO_SDA;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_GPIO_SCL;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

/**
 * @brief i2c read
 */

int eos_i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len) {
    int i, ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr << 1 | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr << 1 | I2C_MASTER_READ, ACK_CHECK_EN);
    for (i=0; i < len - 1; i++) {
        i2c_master_read_byte(cmd, data+i, ACK_VAL);
    }
    i2c_master_read_byte(cmd, data+i, NCK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return EOS_ERR;
    }
    return EOS_OK;
}

/**
 * @brief i2c read8
 */

uint8_t eos_i2c_read8(uint8_t addr, uint8_t reg) {
    uint8_t data;
    eos_i2c_read(addr, reg, &data, 1);
    return data;
}

/**
 * @brief i2c write
 */

int eos_i2c_write(uint8_t addr, uint8_t reg, uint8_t *data, size_t len) {
    int i, ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr << 1 | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    for (i=0; i < len; i++) {
        i2c_master_write_byte(cmd, *(data+i), ACK_CHECK_EN);
    }
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return EOS_ERR;
    }
    return EOS_OK;
}

/**
 * @brief i2c write8
 */

void eos_i2c_write8(uint8_t addr, uint8_t reg, uint8_t data) {
    eos_i2c_write(addr, reg, &data, 1);
}

