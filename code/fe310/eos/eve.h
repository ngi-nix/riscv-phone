#include <stdint.h>

#include "eve_def.h"

void eos_eve_command(uint8_t command, uint8_t parameter);

uint8_t eos_eve_read8(uint32_t addr);
uint16_t eos_eve_read16(uint32_t addr);
uint32_t eos_eve_read32(uint32_t addr);
void eos_eve_write8(uint32_t addr, uint8_t data);
void eos_eve_write16(uint32_t addr, uint16_t data);
void eos_eve_write32(uint32_t addr, uint32_t data);

void eos_eve_active(void);
void eos_eve_standby(void);
void eos_eve_sleep(void);
void eos_eve_reset(void);
void eos_eve_brightness(uint8_t b);

void eos_eve_dl_start(void);
void eos_eve_dl_write(uint32_t dl);
void eos_eve_dl_end(void);

void eos_eve_cmd(uint32_t cmd, const char *fmt, ...);
void eos_eve_cmd_dl(uint32_t dl);
int eos_eve_cmd_done(void);
int eos_eve_cmd_exec(int w);
void eos_eve_cmd_burst_start(void);
void eos_eve_cmd_burst_end(void);

int eos_eve_init(void);

