#include <stdint.h>

#include "eve_def.h"

typedef struct EVETag {
    uint16_t x;
    uint16_t y;
    uint8_t value;
    uint8_t value_prev;
    char t;
} EVETag;

typedef void (*eos_eve_fptr_t) (uint8_t, int);

void eos_eve_command(uint8_t command, uint8_t parameter);

uint8_t eos_eve_read8(uint32_t addr);
uint16_t eos_eve_read16(uint32_t addr);
uint32_t eos_eve_read32(uint32_t addr);
void eos_eve_write8(uint32_t addr, uint8_t data);
void eos_eve_write16(uint32_t addr, uint16_t data);
void eos_eve_write32(uint32_t addr, uint32_t data);

void eos_eve_active(void);
void eos_eve_brightness(uint8_t b);

void eos_eve_dl_start(uint32_t addr);
void eos_eve_dl_write(uint32_t dl);
void eos_eve_dl_swap(void);
uint32_t eos_eve_dl_addr(void);

void eos_eve_cmd(uint32_t cmd, const char *fmt, ...);
uint32_t eos_eve_cmd_result(uint16_t offset);
void eos_eve_cmd_dl(uint32_t dl);
int eos_eve_cmd_done(void);
int eos_eve_cmd_exec(int w);
void eos_eve_cmd_burst_start(void);
void eos_eve_cmd_burst_end(void);

int eos_eve_init(void);
void eos_eve_set_renderer(eos_eve_fptr_t renderer);
EVETag *eos_eve_tag(void);