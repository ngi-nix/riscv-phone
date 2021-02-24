#include <stdlib.h>
#include <stdint.h>

#include "encoding.h"
#include "platform.h"
#include "prci_driver.h"

#include "eos.h"
#include "i2c.h"

void eos_i2c_start(uint32_t baud_rate) {
    eos_i2c_set_baud_rate(baud_rate);

    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << IOF_I2C0_SCL);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << IOF_I2C0_SCL);

    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << IOF_I2C0_SDA);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << IOF_I2C0_SDA);

    GPIO_REG(GPIO_IOF_SEL)      &= ~IOF0_I2C0_MASK;
    GPIO_REG(GPIO_IOF_EN)       |=  IOF0_I2C0_MASK;
}

void eos_i2c_stop(void) {
    GPIO_REG(GPIO_PULLUP_EN)    |=  (1 << IOF_I2C0_SCL);
    GPIO_REG(GPIO_PULLUP_EN)    |=  (1 << IOF_I2C0_SDA);
    GPIO_REG(GPIO_IOF_EN)       &=  ~IOF0_I2C0_MASK;
}

void eos_i2c_set_baud_rate(uint32_t baud_rate) {
    unsigned long clock_rate = PRCI_get_cpu_freq();
    uint16_t prescaler = (clock_rate / (baud_rate * 5)) - 1;

    I2C0_REGB(I2C_CONTROL) &= ~I2C_CONTROL_EN;
    I2C0_REGB(I2C_PRESCALE_LOW) = prescaler & 0xFF;
    I2C0_REGB(I2C_PRESCALE_HIGH) = (prescaler >> 8) & 0xFF;
    I2C0_REGB(I2C_CONTROL) |= I2C_CONTROL_EN;
}


static int i2c_addr(uint8_t addr, uint8_t rw_flag) {
    I2C0_REGB(I2C_TRANSMIT) = ((addr & 0x7F) << 1) | (rw_flag & 0x1);
    I2C0_REGB(I2C_COMMAND) = I2C_CMD_START | I2C_CMD_WRITE;
    while (I2C0_REGB(I2C_STATUS) & I2C_STATUS_TIP);

    if (I2C0_REGB(I2C_STATUS) & I2C_STATUS_RXACK) return EOS_ERR;
    return EOS_OK;
}

static int i2c_reg(uint8_t reg) {
    I2C0_REGB(I2C_TRANSMIT) = reg;
    I2C0_REGB(I2C_COMMAND) = I2C_CMD_WRITE;
    while (I2C0_REGB(I2C_STATUS) & I2C_STATUS_TIP);

    if (I2C0_REGB(I2C_STATUS) & I2C_STATUS_RXACK) return EOS_ERR;
    return EOS_OK;
}

int eos_i2c_read(uint8_t addr, uint8_t reg, uint8_t *buffer, uint16_t len) {
    uint8_t cmd;
    int rv;
    int i;

    rv = i2c_addr(addr, I2C_WRITE);
    if (rv) return rv;

    rv = i2c_reg(reg);
    if (rv) return rv;

    rv = i2c_addr(addr, I2C_READ);
    if (rv) return rv;

    cmd = I2C_CMD_READ;

    for (i=0; i<len; i++) {
        /* Set NACK to end read, and generate STOP condition */
        if (i == (len - 1)) cmd |= I2C_CMD_ACK | I2C_CMD_STOP;
        I2C0_REGB(I2C_COMMAND) = cmd;
        while (I2C0_REGB(I2C_STATUS) & I2C_STATUS_TIP);
        buffer[i] = I2C0_REGB(I2C_RECEIVE);
    }

    return EOS_OK;
}

int eos_i2c_write(uint8_t addr, uint8_t reg, uint8_t *buffer, uint16_t len) {
    uint8_t cmd;
    int rv;
    int i;

    rv = i2c_addr(addr, I2C_WRITE);
    if (rv) return rv;

    rv = i2c_reg(reg);
    if (rv) return rv;

    cmd = I2C_CMD_WRITE;

    for (i=0; i<len; i++) {
        if (i == (len - 1)) cmd |= I2C_CMD_STOP;
        I2C0_REGB(I2C_TRANSMIT) = buffer[i];
        I2C0_REGB(I2C_COMMAND) = cmd;
        while (I2C0_REGB(I2C_STATUS) & I2C_STATUS_TIP);
        if (I2C0_REGB(I2C_STATUS) & I2C_STATUS_RXACK) return EOS_ERR;
    }

    return EOS_OK;
}

int eos_i2c_read8(uint8_t addr, uint8_t reg, uint8_t *data) {
    return eos_i2c_read(addr, reg, data, 1);
}

int eos_i2c_write8(uint8_t addr, uint8_t reg, uint8_t data) {
    return eos_i2c_write(addr, reg, &data, 1);
}
