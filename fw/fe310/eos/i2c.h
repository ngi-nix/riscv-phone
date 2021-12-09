#include <stdint.h>

#define EOS_I2C_SPEED       100000

int eos_i2c_init(uint8_t wakeup_cause);
int eos_i2c_run(uint8_t wakeup_cause);
void eos_i2c_start(void);
void eos_i2c_stop(void);
void eos_i2c_speed(uint32_t baud_rate);
int eos_i2c_read8(uint8_t addr, uint8_t reg, uint8_t *buffer, uint16_t len);
int eos_i2c_read16(uint8_t addr, uint16_t reg, uint8_t *buffer, uint16_t len);
int eos_i2c_write8(uint8_t addr, uint8_t reg, uint8_t *buffer, uint16_t len);
int eos_i2c_write16(uint8_t addr, uint16_t reg, uint8_t *buffer, uint16_t len);
