#include <stdint.h>

#include "eve_def.h"
#include "eve_touch.h"
#include "eve_phy.h"
#include "eve_vtrack.h"
#include "eve_platform.h"

#define EVE_OK              0
#define EVE_ERR             -1

#define EVE_ERR_FULL        -10
#define EVE_ERR_EMPTY       -11

#define EVE_ERR_NOMEM       -100

#define EVE_PSTATE_ACTIVE   0
#define EVE_PSTATE_STANDBY  1
#define EVE_PSTATE_SLEEP    3

#define COLOR_RGBC(c)       ((4UL<<24)|((c)&16777215UL))
#define CLEAR_COLOR_RGBC(c) ((2UL<<24)|((c)&16777215UL))

typedef struct EVERect {
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
} EVERect;

void eve_command(uint8_t command, uint8_t parameter);

uint8_t eve_read8(uint32_t addr);
uint16_t eve_read16(uint32_t addr);
uint32_t eve_read32(uint32_t addr);
void eve_write8(uint32_t addr, uint8_t data);
void eve_write16(uint32_t addr, uint16_t data);
void eve_write32(uint32_t addr, uint32_t data);

void eve_readb(uint32_t addr, uint8_t *buf, size_t size);
void eve_writeb(uint32_t addr, uint8_t *buf, size_t size);

void eve_dl_start(uint32_t addr, char burst);
void eve_dl_write(uint32_t dl);
void eve_dl_end(void);
void eve_dl_swap(void);
uint32_t eve_dl_get_addr(void);

void eve_cmd(uint32_t cmd, const char *fmt, ...);
void eve_cmd_write(uint8_t *buffer, size_t size);
void eve_cmd_end(void);
uint32_t eve_cmd_result(uint16_t offset);
void eve_cmd_dl(uint32_t dl);
int eve_cmd_done(void);
int eve_cmd_exec(int w);
void eve_cmd_burst_start(void);
void eve_cmd_burst_end(void);

int eve_init(uint8_t wakeup_cause, int touch_calibrate, uint32_t *touch_matrix, uint8_t gpio_dir);
void eve_start(uint8_t wakeup_cause);
void eve_stop(void);

int eve_gpio_get(int gpio);
void eve_gpio_set(int gpio, int val);
uint8_t eve_gpio_get_dir(void);
void eve_gpio_set_dir(uint8_t dir);

void eve_standby(void);
void eve_sleep(void);
void eve_active(void);
void eve_brightness(uint8_t b);
