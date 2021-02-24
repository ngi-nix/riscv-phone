#include <stdint.h>

void eos_i2c_start(uint32_t baud_rate);
void eos_i2c_stop(void);
void eos_i2c_set_baud_rate(uint32_t baud_rate);
int eos_i2c_read(uint8_t addr, uint8_t reg, uint8_t *buffer, uint16_t len);
int eos_i2c_write(uint8_t addr, uint8_t reg, uint8_t *buffer, uint16_t len);
int eos_i2c_read8(uint8_t addr, uint8_t reg, uint8_t *data);
int eos_i2c_write8(uint8_t addr, uint8_t reg, uint8_t data);
