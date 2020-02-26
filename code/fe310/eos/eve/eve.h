#include <stdint.h>

#include "eve_def.h"

#define EVE_ETYPE_INTR                  1

/* events */
#define EVE_TOUCH_ETYPE_TRACK           0x0001
#define EVE_TOUCH_ETYPE_TRACK_REG       0x0002
#define EVE_TOUCH_ETYPE_TRACK_START     0x0004
#define EVE_TOUCH_ETYPE_TRACK_STOP      0x0008
#define EVE_TOUCH_ETYPE_TAG             0x0010
#define EVE_TOUCH_ETYPE_TAG_UP          0x0020
#define EVE_TOUCH_ETYPE_POINT           0x0040
#define EVE_TOUCH_ETYPE_POINT_UP        0x0080
#define EVE_TOUCH_ETYPE_LPRESS          0x0100
#define EVE_TOUCH_ETYPE_TAP1            0x0200
#define EVE_TOUCH_ETYPE_TAP2            0x0400

#define EVE_TOUCH_EVT_MASK              0x0fff

#define EVE_TOUCH_ETYPE_TAG_MASK        (EVE_TOUCH_ETYPE_TAG        | EVE_TOUCH_ETYPE_TAG_UP)
#define EVE_TOUCH_ETYPE_TAP_MASK        (EVE_TOUCH_ETYPE_TAP1       | EVE_TOUCH_ETYPE_TAP2)
#define EVE_TOUCH_ETYPE_TRACK_MASK      (EVE_TOUCH_ETYPE_TRACK      | EVE_TOUCH_ETYPE_TRACK_REG | EVE_TOUCH_ETYPE_TRACK_START | EVE_TOUCH_ETYPE_TRACK_STOP)
#define EVE_TOUCH_ETYPE_POINT_MASK      (EVE_TOUCH_ETYPE_POINT      | EVE_TOUCH_ETYPE_POINT_UP)

/* extended events */
#define EVE_TOUCH_ETYPE_TRACK_LEFT      0x1000
#define EVE_TOUCH_ETYPE_TRACK_RIGHT     0x2000
#define EVE_TOUCH_ETYPE_TRACK_UP        0x4000
#define EVE_TOUCH_ETYPE_TRACK_DOWN      0x8000

#define EVE_TOUCH_ETYPE_TRACK_X         (EVE_TOUCH_ETYPE_TRACK_LEFT | EVE_TOUCH_ETYPE_TRACK_RIGHT)
#define EVE_TOUCH_ETYPE_TRACK_Y         (EVE_TOUCH_ETYPE_TRACK_UP   | EVE_TOUCH_ETYPE_TRACK_DOWN)
#define EVE_TOUCH_ETYPE_TRACK_XY        (EVE_TOUCH_ETYPE_TRACK_X    | EVE_TOUCH_ETYPE_TRACK_Y)

/* tag options */
#define EVE_TOUCH_OPT_TRACK             EVE_TOUCH_ETYPE_TRACK
#define EVE_TOUCH_OPT_TRACK_REG         EVE_TOUCH_ETYPE_TRACK_REG
#define EVE_TOUCH_OPT_TRACK_X           0x04
#define EVE_TOUCH_OPT_TRACK_Y           0x08
#define EVE_TOUCH_OPT_INERT             0x10
#define EVE_TOUCH_OPT_LPRESS            0x40
#define EVE_TOUCH_OPT_DTAP              0x80

#define EVE_TOUCH_OPT_TRACK_XY          (EVE_TOUCH_OPT_TRACK_X      | EVE_TOUCH_OPT_TRACK_Y)
#define EVE_TOUCH_OPT_TRACK_MASK        (EVE_TOUCH_OPT_TRACK        | EVE_TOUCH_OPT_TRACK_REG)
#define EVE_TOUCH_OPT_TIMER_MASK        (EVE_TOUCH_OPT_LPRESS       | EVE_TOUCH_OPT_DTAP)

typedef struct EVETouch {
    int x;
    int y;
    int vx;
    int vy;
    int x0;
    int y0;
    uint64_t t;
    uint16_t evt;
    uint8_t tag0;
    uint8_t tag;
    uint8_t tag_up;
    struct {
        uint8_t tag;
        uint8_t track;
        uint16_t val;
    } tracker;
} EVETouch;

typedef struct EVETouchTimer {
    uint8_t tag;
    uint8_t idx;
    uint16_t evt;
    int x0;
    int y0;
    int fc;
} EVETouchTimer;

typedef void (*eve_touch_handler_t) (void *, uint8_t, int);

void eve_command(uint8_t command, uint8_t parameter);

uint8_t eve_read8(uint32_t addr);
uint16_t eve_read16(uint32_t addr);
uint32_t eve_read32(uint32_t addr);
void eve_write8(uint32_t addr, uint8_t data);
void eve_write16(uint32_t addr, uint16_t data);
void eve_write32(uint32_t addr, uint32_t data);

void eve_active(void);
void eve_brightness(uint8_t b);

void eve_dl_start(uint32_t addr);
void eve_dl_write(uint32_t dl);
void eve_dl_swap(void);
uint32_t eve_dl_get_addr(void);

void eve_cmd(uint32_t cmd, const char *fmt, ...);
uint32_t eve_cmd_result(uint16_t offset);
void eve_cmd_dl(uint32_t dl);
int eve_cmd_done(void);
int eve_cmd_exec(int w);
void eve_cmd_burst_start(void);
void eve_cmd_burst_end(void);

int eve_init(uint32_t *touch_transform);

void eve_touch_set_handler(eve_touch_handler_t handler, void *handler_param);
EVETouch *eve_touch_evt(uint8_t tag0, int touch_idx, uint8_t tag_min, uint8_t tag_max, uint16_t *evt);
void eve_touch_set_opt(uint8_t tag, uint8_t opt);

EVETouchTimer *eve_touch_get_timer(void);