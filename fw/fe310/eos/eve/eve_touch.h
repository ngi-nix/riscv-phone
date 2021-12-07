#include <stdint.h>

#define EVE_TOUCH_TIMEOUT_TAP           1000
#define EVE_TOUCH_TIMEOUT_TRACK         20

#define EVE_TOUCH_THRESHOLD_X           5
#define EVE_TOUCH_THRESHOLD_Y           5
#define EVE_TOUCH_TRAVG                 3

#define EVE_NOTAG                       0
#define EVE_NOTOUCH                     0x80008000
#define EVE_MAX_TOUCH                   5

/* events */
#define EVE_TOUCH_ETYPE_TAG             0x0001
#define EVE_TOUCH_ETYPE_TAG_UP          0x0002
#define EVE_TOUCH_ETYPE_POINT           0x0004
#define EVE_TOUCH_ETYPE_POINT_UP        0x0008
#define EVE_TOUCH_ETYPE_TRACK           0x0010
#define EVE_TOUCH_ETYPE_TRACK_START     0x0020
#define EVE_TOUCH_ETYPE_TRACK_STOP      0x0040
#define EVE_TOUCH_ETYPE_TRACK_ABORT     0x0080
#define EVE_TOUCH_ETYPE_TIMER           0x0100
#define EVE_TOUCH_ETYPE_TIMER_ABORT     0x0200
#define EVE_TOUCH_ETYPE_TRACK_REG       0x0400
#define EVE_TOUCH_ETYPE_LPRESS          0x0800
#define EVE_TOUCH_ETYPE_TAP1            0x1000
#define EVE_TOUCH_ETYPE_TAP2            0x2000
#define EVE_TOUCH_ETYPE_USR             0x4000
#define EVE_TOUCH_ETYPE_USR1            0x8000

#define EVE_TOUCH_ETYPE_TRACK_MASK      (EVE_TOUCH_ETYPE_TRACK  | EVE_TOUCH_ETYPE_TRACK_START | EVE_TOUCH_ETYPE_TRACK_STOP | EVE_TOUCH_ETYPE_TRACK_ABORT | EVE_TOUCH_ETYPE_TRACK_REG)
#define EVE_TOUCH_ETYPE_TIMER_MASK      (EVE_TOUCH_ETYPE_TIMER  | EVE_TOUCH_ETYPE_TIMER_ABORT)
#define EVE_TOUCH_ETYPE_POINT_MASK      (EVE_TOUCH_ETYPE_POINT  | EVE_TOUCH_ETYPE_POINT_UP)
#define EVE_TOUCH_ETYPE_USR_MASK        (EVE_TOUCH_ETYPE_USR    | EVE_TOUCH_ETYPE_USR1)

/* extended events */
#define EVE_TOUCH_EETYPE_NOTOUCH        0x0001
#define EVE_TOUCH_EETYPE_LPRESS         0x0002
#define EVE_TOUCH_EETYPE_TAP2           0x0004

#define EVE_TOUCH_EETYPE_TRACK_LEFT     0x0010
#define EVE_TOUCH_EETYPE_TRACK_RIGHT    0x0020
#define EVE_TOUCH_EETYPE_TRACK_UP       0x0040
#define EVE_TOUCH_EETYPE_TRACK_DOWN     0x0080

#define EVE_TOUCH_EETYPE_TRACK_ABORT    0x0100
#define EVE_TOUCH_EETYPE_TIMER_ABORT    0x0200

#define EVE_TOUCH_EETYPE_USR            0x1000
#define EVE_TOUCH_EETYPE_USR1           0x2000
#define EVE_TOUCH_EETYPE_USR2           0x4000
#define EVE_TOUCH_EETYPE_USR3           0x8000

#define EVE_TOUCH_EETYPE_TRACK_X        (EVE_TOUCH_EETYPE_TRACK_LEFT    | EVE_TOUCH_EETYPE_TRACK_RIGHT)
#define EVE_TOUCH_EETYPE_TRACK_Y        (EVE_TOUCH_EETYPE_TRACK_UP      | EVE_TOUCH_EETYPE_TRACK_DOWN)
#define EVE_TOUCH_EETYPE_TRACK_XY       (EVE_TOUCH_EETYPE_TRACK_X       | EVE_TOUCH_EETYPE_TRACK_Y)
#define EVE_TOUCH_EETYPE_ABORT          (EVE_TOUCH_EETYPE_TRACK_ABORT   | EVE_TOUCH_EETYPE_TIMER_ABORT)

/* tag options */
#define EVE_TOUCH_OPT_TRACK             0x01
#define EVE_TOUCH_OPT_TRACK_REG         0x02
#define EVE_TOUCH_OPT_TRACK_X           0x04
#define EVE_TOUCH_OPT_TRACK_Y           0x08
#define EVE_TOUCH_OPT_TRACK_EXT_X       0x10
#define EVE_TOUCH_OPT_TRACK_EXT_Y       0x20
#define EVE_TOUCH_OPT_LPRESS            0x40
#define EVE_TOUCH_OPT_TAP2              0x80

#define EVE_TOUCH_OPT_TRACK_XY          (EVE_TOUCH_OPT_TRACK_X      | EVE_TOUCH_OPT_TRACK_Y)
#define EVE_TOUCH_OPT_TRACK_EXT_XY      (EVE_TOUCH_OPT_TRACK_EXT_X  | EVE_TOUCH_OPT_TRACK_EXT_Y)

typedef struct EVETouch {
    int x;
    int y;
    int vx;
    int vy;
    int x0;
    int y0;
    uint64_t t;
    uint16_t eevt;
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
    EVETouch *touch;
    uint32_t to;
    uint16_t evt;
    uint8_t tag0;
} EVETouchTimer;

typedef void (*eve_touch_handler_t) (EVETouch *, uint16_t, uint8_t, void *);

void eve_handle_touch(uint16_t intr_flags);
void eve_handle_time(void);

void eve_touch_init(int touch_calibrate, uint32_t *touch_matrix);
void eve_touch_start(void);
void eve_touch_stop(void);

void eve_touch_set_handler(eve_touch_handler_t handler, void *handler_param);
EVETouch *eve_touch_get(int i);
int8_t eve_touch_get_idx(EVETouch *touch);
uint16_t eve_touch_evt(EVETouch *touch, uint16_t evt, uint8_t tag0, uint8_t tag_min, uint8_t tag_n);

void eve_touch_set_opt(uint8_t tag, uint8_t opt);
uint8_t eve_touch_get_opt(uint8_t tag);
void eve_touch_clear_opt(void);

void eve_touch_timer_set(EVETouch *touch, uint16_t evt, uint8_t tag0, uint32_t to);
void eve_touch_timer_clear(EVETouch *touch);
uint16_t eve_touch_timer_get_evt(EVETouch *touch);
void eve_touch_timer_set_evt(EVETouch *touch, uint16_t evt);
void eve_touch_timer_start(uint8_t tag0, uint32_t to);
void eve_touch_timer_stop(void);

EVETouchTimer *eve_touch_timer_get(void);
