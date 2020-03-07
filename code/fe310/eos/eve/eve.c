#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>

#include "eve.h"
#include "eve_platform.h"

#define EVE_THRESHOLD_X         5
#define EVE_THRESHOLD_Y         5
#define EVE_TIMEOUT_TAP         1000
#define EVE_TIMEOUT_TRACK       20
#define EVE_TRAVG               3
#define EVE_FRICTION            500

#define EVE_NOTOUCH             0x80000000
#define EVE_MEM_WRITE           0x800000

#define EVE_MAX_TOUCH           5
#define EVE_TAG_SCREEN          0xff

static char _cmd_burst;
static uint16_t _cmd_offset;
static uint32_t _dl_addr;

static int _intr_mask = EVE_INT_TAG | EVE_INT_TOUCH;
static int _multitouch;
static uint8_t _tag0;

static EVETouch _touch[EVE_MAX_TOUCH];
static EVETouchTimer _touch_timer;

static eve_touch_handler_t _touch_handler;
static void *_touch_handler_param;

static uint8_t _tag_opt[255];

static const uint32_t _reg_touch[] = {
    REG_CTOUCH_TOUCH0_XY,
    REG_CTOUCH_TOUCH1_XY,
    REG_CTOUCH_TOUCH2_XY,
    REG_CTOUCH_TOUCH3_XY
};

static const uint32_t _reg_tag[] = {
    REG_TOUCH_TAG,
    REG_TOUCH_TAG1,
    REG_TOUCH_TAG2,
    REG_TOUCH_TAG3,
    REG_TOUCH_TAG4
};

static const uint32_t _reg_track[] = {
    REG_TRACKER,
    REG_TRACKER_1,
    REG_TRACKER_2,
    REG_TRACKER_3,
    REG_TRACKER_4
};

void eve_command(uint8_t command, uint8_t parameter) {
    eve_spi_cs_set();
    eve_spi_xchg24(((uint32_t)command << 16) | ((uint32_t)parameter << 8), 0);
    eve_spi_cs_clear();
}

uint8_t eve_read8(uint32_t addr) {
    uint8_t r;
    eve_spi_cs_set();
    eve_spi_xchg32(addr << 8, 0);
    r = eve_spi_xchg8(0, EVE_SPI_FLAG_BSWAP);
    eve_spi_cs_clear();

    return r;
}

uint16_t eve_read16(uint32_t addr) {
    uint16_t r;
    eve_spi_cs_set();
    eve_spi_xchg32(addr << 8, 0);
    r = eve_spi_xchg16(0, EVE_SPI_FLAG_BSWAP);
    eve_spi_cs_clear();

    return r;
}

uint32_t eve_read32(uint32_t addr) {
    uint32_t r;
    eve_spi_cs_set();
    eve_spi_xchg32(addr << 8, 0);
    r = eve_spi_xchg32(0, EVE_SPI_FLAG_BSWAP);
    eve_spi_cs_clear();

    return r;
}

void eve_write8(uint32_t addr, uint8_t data) {
    eve_spi_cs_set();
    eve_spi_xchg24(addr | EVE_MEM_WRITE, 0);
    eve_spi_xchg8(data, EVE_SPI_FLAG_BSWAP);
    eve_spi_cs_clear();
}

void eve_write16(uint32_t addr, uint16_t data) {
    eve_spi_cs_set();
    eve_spi_xchg24(addr | EVE_MEM_WRITE, 0);
    eve_spi_xchg16(data, EVE_SPI_FLAG_BSWAP);
    eve_spi_cs_clear();
}

void eve_write32(uint32_t addr, uint32_t data) {
    eve_spi_cs_set();
    eve_spi_xchg24(addr | EVE_MEM_WRITE, 0);
    eve_spi_xchg32(data, EVE_SPI_FLAG_BSWAP);
    eve_spi_cs_clear();
}

void eve_active(void) {
    eve_command(EVE_ACTIVE, 0);
}

void eve_brightness(uint8_t b) {
	eve_write8(REG_PWM_DUTY, b);
}

static void _dl_inc(uint32_t i) {
	_dl_addr += i;
}

void eve_dl_start(uint32_t addr) {
    _dl_addr = addr;
}

void eve_dl_write(uint32_t dl) {
    eve_write32(_dl_addr, dl);
    _dl_inc(4);
}

void eve_dl_swap(void) {
    eve_write8(REG_DLSWAP, EVE_DLSWAP_FRAME);
}

uint32_t eve_dl_get_addr(void) {
    return _dl_addr;
}

static void _cmd_inc(uint16_t i) {
	_cmd_offset += i;
	_cmd_offset &= 0x0fff;
}

static void _cmd_begin(uint32_t command) {
    uint8_t flags = 0;

    if (_cmd_burst) {
        flags = EVE_SPI_FLAG_TX;
    } else {
    	uint32_t addr = EVE_RAM_CMD + _cmd_offset;
        eve_spi_cs_set();
        eve_spi_xchg24(addr | EVE_MEM_WRITE, 0);
    }
    eve_spi_xchg32(command, EVE_SPI_FLAG_BSWAP | flags);
    _cmd_inc(4);
}

static void _cmd_end(void) {
    if (!_cmd_burst) eve_spi_cs_clear();
}

static void _cmd_string(const char *s, uint8_t flags) {
    int i = 0;

    while (s[i] != 0) {
        eve_spi_xchg8(s[i], flags);
        i++;
    }
    eve_spi_xchg8(0, flags);
    i++;
    _cmd_inc(i);
}

static void _cmd_buffer(const char *b, int size, uint8_t flags) {
    int i = 0;

    for (i=0; i<size; i++) {
        eve_spi_xchg8(b[i], flags);
    }
    _cmd_inc(size);
}

void eve_cmd(uint32_t cmd, const char *fmt, ...) {
    uint8_t flags = _cmd_burst ? (EVE_SPI_FLAG_TX | EVE_SPI_FLAG_BSWAP) : EVE_SPI_FLAG_BSWAP;
    va_list argv;
    uint16_t *p;
    int i = 0;

    va_start(argv, fmt);
    _cmd_begin(cmd);
    while (fmt[i]) {
        switch (fmt[i]) {
            case 'b':
                eve_spi_xchg8(va_arg(argv, int), flags);
                _cmd_inc(1);
                break;
            case 'h':
                eve_spi_xchg16(va_arg(argv, int), flags);
                _cmd_inc(2);
                break;
            case 'w':
                eve_spi_xchg32(va_arg(argv, int), flags);
                _cmd_inc(4);
                break;
            case '&':
                p = va_arg(argv, uint16_t *);
                *p = _cmd_offset;
                eve_spi_xchg32(0, flags);
                _cmd_inc(4);
                break;
            case 's':
                _cmd_string(va_arg(argv, const char *), flags);
                break;
            case 'p':
                _cmd_buffer(va_arg(argv, const char *), va_arg(argv, int), flags);
                break;
        }
        i++;
    }
	va_end(argv);
    /* padding */
	i = _cmd_offset & 3;  /* equivalent to _cmd_offset % 4 */
    if (i) {
    	i = 4 - i;  /* 3, 2 or 1 */
        _cmd_inc(i);

    	while (i > 0) {
            eve_spi_xchg8(0, flags);
    		i--;
    	}
    }
    _cmd_end();
}

uint32_t eve_cmd_result(uint16_t offset) {
    return eve_read32(EVE_RAM_CMD + offset);
}

void eve_cmd_dl(uint32_t dl) {
    _cmd_begin(dl);
    _cmd_end();
}

int eve_cmd_done(void) {
	uint16_t r = eve_read16(REG_CMD_READ);
    if (r == 0xfff) {
		_cmd_offset = 0;
		eve_write8(REG_CPURESET, 1);
		eve_write16(REG_CMD_READ, 0);
		eve_write16(REG_CMD_WRITE, 0);
		eve_write16(REG_CMD_DL, 0);
		eve_write8(REG_CPURESET, 0);
        return -1;
    }
    return (r == _cmd_offset);
}

int eve_cmd_exec(int w) {
    eve_write16(REG_CMD_WRITE, _cmd_offset);
    if (w) {
        int r;
        do {
            r = eve_cmd_done();
        } while (!r);
        if (r < 0) return EVE_ERR;
    }
    return EVE_OK;
}

void eve_cmd_burst_start(void) {
	uint32_t addr = EVE_RAM_CMD + _cmd_offset;
    eve_spi_cs_set();
    eve_spi_xchg24(addr | EVE_MEM_WRITE, EVE_SPI_FLAG_TX);
    _cmd_burst = 1;
}

void eve_cmd_burst_end(void) {
    eve_spi_flush();
    eve_spi_cs_clear();
    _cmd_burst = 0;
}

static void _touch_timer_clear(void) {
    eve_timer_clear();
    _touch_timer.tag = 0;
    _touch_timer.fc = 0;
}

void eve_handle_touch(void) {
    int i;
    char touch_ex = 0;
    char int_ccomplete = 0;
    uint8_t tag0 = _tag0;
    uint8_t touch_last = 0;
    uint8_t flags = eve_read8(REG_INT_FLAGS) & _intr_mask;

    if (!_multitouch && (flags & EVE_INT_TOUCH)) _multitouch = 1;
    for (i=0; i<EVE_MAX_TOUCH; i++) {
        uint8_t touch_tag;
        uint32_t touch_xy;
        uint64_t now = 0;
        EVETouch *touch = &_touch[i];

        touch->evt &= ~EVE_TOUCH_EVT_MASK;
        touch_xy = i < 4 ? eve_read32(_reg_touch[i]) : (((uint32_t)eve_read16(REG_CTOUCH_TOUCH4_X) << 16) | eve_read16(REG_CTOUCH_TOUCH4_Y));

        if (touch_xy != 0x80008000) {
            int16_t touch_x = touch_xy >> 16;
            int16_t touch_y = touch_xy & 0xffff;
            now = eve_timer_get_tick();
            if (touch->x == EVE_NOTOUCH) {
                if (!_tag0 && _touch_timer.tag) {
                    if (_touch_timer.evt & EVE_TOUCH_ETYPE_TAP1) {
                        int dx = touch_x - touch->x0;
                        int dy = touch_y - touch->y0;
                        dx = dx < 0 ? -dx : dx;
                        dy = dy < 0 ? -dy : dy;
                        if ((dx > EVE_THRESHOLD_X) || (dy > EVE_THRESHOLD_Y)) {
                            touch->evt |= EVE_TOUCH_ETYPE_TAP1;
                        } else {
                            touch->evt |= EVE_TOUCH_ETYPE_TAP2;
                        }
                    }
                    if (_touch_timer.evt & EVE_TOUCH_ETYPE_TRACK) {
                        touch->evt |= EVE_TOUCH_ETYPE_TRACK_STOP;
                    }
                    if (_touch_handler && (touch->evt & EVE_TOUCH_EVT_MASK)) {
                        _touch_handler(_touch_handler_param, _touch_timer.tag, i);
                    }
                    _touch_timer_clear();
                }
                touch->evt = EVE_TOUCH_ETYPE_POINT;
                touch->tag0 = 0;
                touch->tag = 0;
                touch->tag_up = 0;
                touch->tracker.tag = 0;
                touch->tracker.track = 0;
                touch->tracker.val = 0;
                touch->t = 0;
                touch->vx = 0;
                touch->vy = 0;
                touch->x0 = touch_x;
                touch->y0 = touch_y;
            } else if (touch->t) {
                int dt = now - touch->t;
                int vx = ((int)touch_x - touch->x) * (int)(EVE_RTC_FREQ) / dt;
                int vy = ((int)touch_y - touch->y) * (int)(EVE_RTC_FREQ) / dt;
                touch->vx = touch->vx ? (vx + touch->vx * EVE_TRAVG) / (EVE_TRAVG + 1) : vx;
                touch->vy = touch->vy ? (vy + touch->vy * EVE_TRAVG) / (EVE_TRAVG + 1) : vy;
                touch->t = now;
            }
            touch->x = touch_x;
            touch->y = touch_y;
            if (_multitouch || (flags & EVE_INT_TAG)) {
                touch_tag = eve_read8(_reg_tag[i]);
            } else {
                touch_tag = touch->tag;
            }
            touch_ex = 1;
        } else {
            touch_tag = 0;
            if (touch->x != EVE_NOTOUCH) {
                touch->evt |= EVE_TOUCH_ETYPE_POINT_UP;
                if (_touch_timer.tag && (i == 0)) {
                    _touch_timer.evt &= ~EVE_TOUCH_ETYPE_LPRESS;
                    if (!_touch_timer.evt) {
                        _touch_timer_clear();
                    }
                }
                if (touch->tracker.tag && touch->tracker.track) {
                    if (!_touch_timer.tag && (_tag_opt[touch->tracker.tag] & EVE_TOUCH_OPT_INERT)) {
                        _touch_timer.x0 = touch->x;
                        _touch_timer.y0 = touch->y;
                        _touch_timer.tag = touch->tracker.tag;
                        _touch_timer.idx = i;
                        _touch_timer.evt = EVE_TOUCH_ETYPE_TRACK;
                        eve_timer_set(EVE_TIMEOUT_TRACK);
                    } else {
                        touch->evt |= EVE_TOUCH_ETYPE_TRACK_STOP;
                    }
                }
                touch->x = EVE_NOTOUCH;
                touch->y = EVE_NOTOUCH;
            }
        }
        if (touch_tag != touch->tag) {
            if (touch_tag) {
                if (!touch->tag0) {
                    touch->tag0 = touch_tag;
                    if (_tag_opt[touch_tag] & EVE_TOUCH_OPT_TRACK_MASK) {
                        touch->tracker.tag = touch_tag;
                    } else if (_tag_opt[EVE_TAG_SCREEN] & EVE_TOUCH_OPT_TRACK_MASK) {
                        touch->tracker.tag = EVE_TAG_SCREEN;
                    }
                    if (touch->tracker.tag && !(_tag_opt[touch->tracker.tag] & EVE_TOUCH_OPT_TRACK_XY)) {
                        touch->tracker.track = 1;
                        touch->evt |= EVE_TOUCH_ETYPE_TRACK_START;
                        touch->t = now;
                    }
                    if (!_tag0 && ((_tag_opt[touch_tag] | _tag_opt[EVE_TAG_SCREEN]) & EVE_TOUCH_OPT_TIMER_MASK)) {
                        _touch_timer.tag = _tag_opt[touch_tag] & EVE_TOUCH_OPT_TIMER_MASK ? touch_tag : EVE_TAG_SCREEN;
                        _touch_timer.idx = 0;
                        _touch_timer.evt = 0;
                        if (_tag_opt[_touch_timer.tag] & EVE_TOUCH_OPT_LPRESS) _touch_timer.evt |= EVE_TOUCH_ETYPE_LPRESS;
                        if (_tag_opt[_touch_timer.tag] & EVE_TOUCH_OPT_DTAP) _touch_timer.evt |= EVE_TOUCH_ETYPE_TAP1;
                        eve_timer_set(EVE_TIMEOUT_TAP);
                    }
                }
                if (!_tag0) _tag0 = tag0 = touch_tag;
            }
            touch->tag_up = touch->tag;
            if (touch->tag_up) touch->evt |= EVE_TOUCH_ETYPE_TAG_UP;
            touch->tag = touch_tag;
            if (touch->tag) touch->evt |= EVE_TOUCH_ETYPE_TAG;
        }
        if (touch_xy != 0x80008000) {
            char _track = touch->tracker.tag && !touch->tracker.track;
            if (_track || _touch_timer.tag) {
                int dx = touch->x - touch->x0;
                int dy = touch->y - touch->y0;
                dx = dx < 0 ? -dx : dx;
                dy = dy < 0 ? -dy : dy;
                if (_track) {
                    if ((dx > EVE_THRESHOLD_X) && !(_tag_opt[touch->tracker.tag] & EVE_TOUCH_OPT_TRACK_X)) {
                        touch->tracker.tag = 0;
                    }
                    if ((dy > EVE_THRESHOLD_Y) && !(_tag_opt[touch->tracker.tag] & EVE_TOUCH_OPT_TRACK_Y)) {
                        touch->tracker.tag = 0;
                    }
                    if (touch->tracker.tag && ((dx > EVE_THRESHOLD_X) || (dy > EVE_THRESHOLD_Y))) {
                        if (dx > EVE_THRESHOLD_X) {
                            touch->evt |= touch->x > touch->x0 ? EVE_TOUCH_ETYPE_TRACK_RIGHT : EVE_TOUCH_ETYPE_TRACK_LEFT;
                        }
                        if (dy > EVE_THRESHOLD_Y) {
                            touch->evt |= touch->y > touch->y0 ? EVE_TOUCH_ETYPE_TRACK_DOWN : EVE_TOUCH_ETYPE_TRACK_UP;
                        }
                        touch->tracker.track = 1;
                        touch->evt |= EVE_TOUCH_ETYPE_TRACK_START;
                        touch->t = now;
                    }
                }
                if (_touch_timer.tag && ((dx > EVE_THRESHOLD_X) || (dy > EVE_THRESHOLD_Y))) {
                    _touch_timer_clear();
                }
            }
            if (touch->tracker.tag && touch->tracker.track) {
                touch->evt |= _tag_opt[touch->tracker.tag] & EVE_TOUCH_OPT_TRACK_MASK;
            }
            if (touch->evt & EVE_TOUCH_ETYPE_TRACK_REG) {
                uint32_t touch_track = eve_read32(_reg_track[i]);
                if (touch->tracker.tag == (touch_track & 0xff)) {
                    touch->tracker.val = touch_track >> 16;
                } else {
                    touch->evt &= ~EVE_TOUCH_ETYPE_TRACK_REG;
                }
            }
            if (touch->tracker.tag || _touch_timer.tag) int_ccomplete = 1;
        }
        if (touch->evt & EVE_TOUCH_EVT_MASK) touch_last = i + 1;
        if (!_multitouch) break;
        if (_touch_timer.tag) {
            _touch_timer_clear();
        }
    }

    if (!touch_ex) {
        _tag0 = 0;
        _multitouch = 0;
    }

    if (_multitouch) int_ccomplete = 1;

    if (int_ccomplete && !(_intr_mask & EVE_INT_CONVCOMPLETE)) {
        _intr_mask |= EVE_INT_CONVCOMPLETE;
        eve_write8(REG_INT_MASK, _intr_mask);
    }
    if (!int_ccomplete && (_intr_mask & EVE_INT_CONVCOMPLETE)) {
        _intr_mask &= ~EVE_INT_CONVCOMPLETE;
        eve_write8(REG_INT_MASK, _intr_mask);
    }

    for (i=0; i<touch_last; i++) {
        EVETouch *touch = &_touch[i];
        if (_touch_handler && (touch->evt & EVE_TOUCH_EVT_MASK)) {
            _touch_handler(_touch_handler_param, tag0, i);
        }
    }
}

void eve_handle_time(void) {
    if (_touch_handler && _touch_timer.tag) {
        EVETouch *touch = &_touch[_touch_timer.idx];

        if ((_touch_timer.evt & EVE_TOUCH_ETYPE_TAP1) && (touch->x != EVE_NOTOUCH)) _touch_timer.evt &= ~EVE_TOUCH_ETYPE_TAP1;

        if (_touch_timer.evt) {
            char more = 0;
            int _x = touch->x;
            int _y = touch->y;

            touch->evt &= ~EVE_TOUCH_EVT_MASK;
            touch->evt |= _touch_timer.evt;
            if (touch->evt & EVE_TOUCH_ETYPE_TRACK) {
                int dt = eve_timer_get_tick() - touch->t;

                if (_touch_timer.fc == 0) {
                    double d = sqrt(touch->vx * touch->vx + touch->vy * touch->vy);
                    _touch_timer.fc = (double)(EVE_RTC_FREQ) * d / EVE_FRICTION;
                }

                if (dt < _touch_timer.fc / 2) {
                    more = 1;
                } else {
                    touch->evt |= EVE_TOUCH_ETYPE_TRACK_STOP;
                    dt = _touch_timer.fc / 2;
                }
                touch->x = _touch_timer.x0 + (touch->vx * dt - touch->vx * dt / _touch_timer.fc * dt ) / (int)(EVE_RTC_FREQ);
                touch->y = _touch_timer.y0 + (touch->vy * dt - touch->vy * dt / _touch_timer.fc * dt ) / (int)(EVE_RTC_FREQ);

                if (more) eve_timer_set(EVE_TIMEOUT_TRACK);
            }

            _touch_handler(_touch_handler_param, _touch_timer.tag, _touch_timer.idx);

            if (!more) _touch_timer_clear();
            touch->x = _x;
            touch->y = _y;
        }
    }
}

int eve_init(uint32_t *touch_transform) {
    int i;
	uint8_t chipid = 0;
	uint16_t timeout = 0;

    eve_command(EVE_RST_PULSE, 0);
    eve_command(EVE_CLKEXT, 0);
    eve_command(EVE_ACTIVE, 0);         /* start EVE */

    while(chipid != 0x7C) {             /* if chipid is not 0x7c, continue to read it until it is, EVE needs a moment for it's power on self-test and configuration */
		eve_sleep(1);
		chipid = eve_read8(REG_ID);
		timeout++;
		if (timeout > 400) return EVE_ERR;
	}

    eve_write8(REG_PWM_DUTY, 0);

    /* Initialize Display */
	eve_write16(REG_HCYCLE,  EVE_HCYCLE);   /* total number of clocks per line, incl front/back porch */
	eve_write16(REG_HOFFSET, EVE_HOFFSET);  /* start of active line */
	eve_write16(REG_HSYNC0,  EVE_HSYNC0);   /* start of horizontal sync pulse */
	eve_write16(REG_HSYNC1,  EVE_HSYNC1);   /* end of horizontal sync pulse */
	eve_write16(REG_VCYCLE,  EVE_VCYCLE);   /* total number of lines per screen, including pre/post */
	eve_write16(REG_VOFFSET, EVE_VOFFSET);  /* start of active screen */
	eve_write16(REG_VSYNC0,  EVE_VSYNC0);   /* start of vertical sync pulse */
	eve_write16(REG_VSYNC1,  EVE_VSYNC1);   /* end of vertical sync pulse */
	eve_write8(REG_SWIZZLE,  EVE_SWIZZLE);  /* FT8xx output to LCD - pin order */
	eve_write8(REG_PCLK_POL, EVE_PCLKPOL);  /* LCD data is clocked in on this PCLK edge */
	eve_write8(REG_CSPREAD,	 EVE_CSPREAD);  /* helps with noise, when set to 1 fewer signals are changed simultaneously, reset-default: 1 */
	eve_write16(REG_HSIZE,   EVE_HSIZE);    /* active display width */
	eve_write16(REG_VSIZE,   EVE_VSIZE);    /* active display height */

	/* do not set PCLK yet - wait for just after the first display list */

	/* configure Touch */
	eve_write8(REG_TOUCH_MODE, EVE_TMODE_CONTINUOUS);       /* enable touch */
	eve_write16(REG_TOUCH_RZTHRESH, EVE_TOUCH_RZTHRESH);    /* eliminate any false touches */

	/* disable Audio for now */
	eve_write8(REG_VOL_PB, 0x00);       /* turn recorded audio volume down */
	eve_write8(REG_VOL_SOUND, 0x00);    /* turn synthesizer volume off */
	eve_write16(REG_SOUND, 0x6000);     /* set synthesizer to mute */

	/* write a basic display-list to get things started */
    eve_dl_start(EVE_RAM_DL);
    eve_dl_write(CLEAR_COLOR_RGB(0,0,0));
	eve_dl_write(CLEAR(1,1,1));
    eve_dl_write(DISPLAY());
    eve_dl_swap();

	/* nothing is being displayed yet... the pixel clock is still 0x00 */
	eve_write8(REG_GPIO, 0x80);         /* enable the DISP signal to the LCD panel, it is set to output in REG_GPIO_DIR by default */
	eve_write8(REG_PCLK, EVE_PCLK);     /* now start clocking data to the LCD panel */

    eve_write8(REG_INT_EN, 0x01);
    eve_write8(REG_INT_MASK, _intr_mask);
    while(eve_read8(REG_INT_FLAGS));

    if (touch_transform) {
        eve_write32(REG_TOUCH_TRANSFORM_A, touch_transform[0]);
        eve_write32(REG_TOUCH_TRANSFORM_B, touch_transform[1]);
        eve_write32(REG_TOUCH_TRANSFORM_C, touch_transform[2]);
        eve_write32(REG_TOUCH_TRANSFORM_D, touch_transform[3]);
        eve_write32(REG_TOUCH_TRANSFORM_E, touch_transform[4]);
        eve_write32(REG_TOUCH_TRANSFORM_F, touch_transform[5]);
    } else {
        uint32_t touch_transform[6];
        eve_cmd_dl(CMD_DLSTART);
        eve_cmd_dl(CLEAR_COLOR_RGB(0,0,0));
        eve_cmd_dl(CLEAR(1,1,1));
        eve_cmd(CMD_TEXT, "hhhhs", EVE_HSIZE/2, EVE_VSIZE/2, 27, EVE_OPT_CENTER, "Please tap on the dot.");
        eve_cmd(CMD_CALIBRATE, "w", 0);
        eve_cmd_dl(DISPLAY());
        eve_cmd_dl(CMD_SWAP);
        eve_cmd_exec(1);

        touch_transform[0] = eve_read32(REG_TOUCH_TRANSFORM_A);
        touch_transform[1] = eve_read32(REG_TOUCH_TRANSFORM_B);
        touch_transform[2] = eve_read32(REG_TOUCH_TRANSFORM_C);
        touch_transform[3] = eve_read32(REG_TOUCH_TRANSFORM_D);
        touch_transform[4] = eve_read32(REG_TOUCH_TRANSFORM_E);
        touch_transform[5] = eve_read32(REG_TOUCH_TRANSFORM_F);

        printf("TOUCH TRANSFORM:{0x%x,0x%x,0x%x,0x%x,0x%x,0x%x}\n", touch_transform[0], touch_transform[1], touch_transform[2], touch_transform[3], touch_transform[4], touch_transform[5]);
    }

    eve_write32(REG_CTOUCH_EXTENDED, 0x00);
    eve_cmd(CMD_SETROTATE, "w", 2);
    eve_cmd_exec(1);

    eve_sleep(500);
    eve_command(EVE_STANDBY, 0);

    for (i=0; i<EVE_MAX_TOUCH; i++) {
        EVETouch *touch = &_touch[i];
        touch->x = EVE_NOTOUCH;
        touch->y = EVE_NOTOUCH;
    }

    eve_init_platform();
    return EVE_OK;
}

void eve_touch_set_handler(eve_touch_handler_t handler, void *param) {
    _touch_handler = handler;
    _touch_handler_param = param;
}

EVETouch *eve_touch_evt(uint8_t tag0, int touch_idx, uint8_t tag_min, uint8_t tag_max, uint16_t *evt) {
    uint8_t _tag;
    uint16_t _evt;
    EVETouch *ret = NULL;

    *evt = 0;
    if ((touch_idx < 0) || (touch_idx > 4)) return ret;
    if ((tag_min == 0) || (tag_max == 0)) return ret;
    if ((tag0 < tag_min) || (tag0 > tag_max)) return ret;

    ret = &_touch[touch_idx];
    _evt = ret->evt;

    *evt |= _evt & EVE_TOUCH_ETYPE_POINT_MASK;
    if (_evt & EVE_TOUCH_ETYPE_TAG) {
        _tag = ret->tag;
        if ((_tag >= tag_min) && (_tag <= tag_max)) *evt |= EVE_TOUCH_ETYPE_TAG;
    }
    if (_evt & EVE_TOUCH_ETYPE_TAG_UP) {
        _tag = ret->tag_up;
        if ((_tag >= tag_min) && (_tag <= tag_max)) *evt |= EVE_TOUCH_ETYPE_TAG_UP;
    }
    if (_evt & EVE_TOUCH_ETYPE_TRACK_REG) {
        _tag = ret->tracker.tag;
        if ((_tag >= tag_min) && (_tag <= tag_max)) *evt |= EVE_TOUCH_ETYPE_TRACK_REG;
    }
    if (_evt & EVE_TOUCH_ETYPE_TRACK_MASK) {
        _tag = ret->tracker.tag;
        if ((_tag >= tag_min) && (_tag <= tag_max)) *evt |= _evt & (EVE_TOUCH_ETYPE_TRACK_MASK | EVE_TOUCH_ETYPE_TRACK_XY);
    }
    if (_evt & (EVE_TOUCH_ETYPE_LPRESS | EVE_TOUCH_ETYPE_TAP1 | EVE_TOUCH_ETYPE_TAP2)) {
        _tag = _touch_timer.tag;
        if ((_tag >= tag_min) && (_tag <= tag_max)) *evt |= _evt & (EVE_TOUCH_ETYPE_LPRESS | EVE_TOUCH_ETYPE_TAP1 | EVE_TOUCH_ETYPE_TAP2);
    }

    return ret;
}

void eve_touch_set_opt(uint8_t tag, uint8_t opt) {
    _tag_opt[tag] = opt;
}

uint8_t eve_touch_get_opt(uint8_t tag) {
    return _tag_opt[tag];
}

void eve_touch_clear_opt(void) {
    memset(_tag_opt, 0, sizeof(_tag_opt));
}

EVETouchTimer *eve_touch_get_timer(void) {
    return &_touch_timer;
}

