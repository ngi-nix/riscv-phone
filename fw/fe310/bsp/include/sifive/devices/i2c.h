// See LICENSE for license details.

#ifndef _SIFIVE_I2C_H
#define _SIFIVE_I2C_H

/* Register offsets */

#define I2C_PRESCALE_LOW   0x00
#define I2C_PRESCALE_HIGH  0x04
#define I2C_CONTROL        0x08
#define I2C_TRANSMIT       0x0c
#define I2C_RECEIVE        0x0c
#define I2C_COMMAND        0x10
#define I2C_STATUS         0x10

/* Constants */

#define I2C_CONTROL_EN      (1UL << 7)
#define I2C_CONTROL_IE      (1UL << 6)
#define I2C_WRITE           (0UL << 0)
#define I2C_READ            (1UL << 0)
#define I2C_CMD_START       (1UL << 7)
#define I2C_CMD_STOP        (1UL << 6)
#define I2C_CMD_READ        (1UL << 5)
#define I2C_CMD_WRITE       (1UL << 4)
#define I2C_CMD_ACK         (1UL << 3)
#define I2C_CMD_IACK        (1UL << 0)
#define I2C_STATUS_RXACK    (1UL << 7)
#define I2C_STATUS_BUSY     (1UL << 6)
#define I2C_STATUS_AL       (1UL << 5)
#define I2C_STATUS_TIP      (1UL << 1)
#define I2C_STATUS_IP       (1UL << 0)

#endif /* _SIFIVE_I2C_H */
