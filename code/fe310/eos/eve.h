#include <stdint.h>

#include "eve_def.h"

#define EOS_TOUCH_ETYPE_DOWN    0x01
#define EOS_TOUCH_ETYPE_UP      0x02
#define EOS_TOUCH_ETYPE_DRAG    0x04
#define EOS_TOUCH_ETYPE_TRACK   0x08

typedef struct EOSTouch {
    uint16_t x;
    uint16_t y;
    uint8_t tag;
    uint8_t tag_prev;
    uint8_t evt;
    char t;
    struct {
        uint16_t tag;
        uint16_t val;
    } tracker;
} EOSTouch;

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
void eos_eve_set_renderer(eos_eve_fptr_t renderer, uint8_t flags);
EOSTouch *eos_touch_evt(uint8_t tag0, int touch_idx, uint8_t tag_min, uint8_t tag_max, uint8_t *evt);
void eos_touch_set_evt(uint8_t tag, uint8_t evt);
