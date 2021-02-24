#include <sys/types.h>
#include <stdint.h>

void eos_i2c_init(void);

int eos_i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);
int eos_i2c_write(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);
int eos_i2c_read8(uint8_t addr, uint8_t reg, uint8_t *data);
int eos_i2c_write8(uint8_t addr, uint8_t reg, uint8_t data);
