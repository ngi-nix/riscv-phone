#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"
#include "interrupt.h"
#include "event.h"
#include "timer.h"
#include "spi.h"
#include "eve.h"
#include "irq_def.h"

#define MEM_WRITE       0x800000

#define EVE_PIN_INT     0
#define EVE_MAX_TOUCH   5

#define EVE_ETYPE_INT   1

static char eve_cmd_burst;
static uint16_t eve_cmd_offset;
static uint32_t eve_dl_addr;

static int eve_int_mask = EVE_INT_TAG | EVE_INT_TOUCH;
static int eve_multitouch = 0;
static uint8_t eve_tag0;
static EOSTouch eve_touch[5];
static uint8_t eve_touch_evt[256];
static eos_eve_fptr_t eve_renderer;

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

void eos_eve_command(uint8_t command, uint8_t parameter) {
    eos_spi_cs_set();
    eos_spi_xchg24(((uint32_t)command << 16) | ((uint32_t)parameter << 8), 0);
    eos_spi_cs_clear();
}

uint8_t eos_eve_read8(uint32_t addr) {
    uint8_t r;
    eos_spi_cs_set();
    eos_spi_xchg32(addr << 8, 0);
    r = eos_spi_xchg8(0, EOS_SPI_FLAG_BSWAP);
    eos_spi_cs_clear();

    return r;
}

uint16_t eos_eve_read16(uint32_t addr) {
    uint16_t r;
    eos_spi_cs_set();
    eos_spi_xchg32(addr << 8, 0);
    r = eos_spi_xchg16(0, EOS_SPI_FLAG_BSWAP);
    eos_spi_cs_clear();

    return r;
}

uint32_t eos_eve_read32(uint32_t addr) {
    uint32_t r;
    eos_spi_cs_set();
    eos_spi_xchg32(addr << 8, 0);
    r = eos_spi_xchg32(0, EOS_SPI_FLAG_BSWAP);
    eos_spi_cs_clear();

    return r;
}

void eos_eve_write8(uint32_t addr, uint8_t data) {
    eos_spi_cs_set();
    eos_spi_xchg24(addr | MEM_WRITE, 0);
    eos_spi_xchg8(data, EOS_SPI_FLAG_BSWAP);
    eos_spi_cs_clear();
}

void eos_eve_write16(uint32_t addr, uint16_t data) {
    eos_spi_cs_set();
    eos_spi_xchg24(addr | MEM_WRITE, 0);
    eos_spi_xchg16(data, EOS_SPI_FLAG_BSWAP);
    eos_spi_cs_clear();
}

void eos_eve_write32(uint32_t addr, uint32_t data) {
    eos_spi_cs_set();
    eos_spi_xchg24(addr | MEM_WRITE, 0);
    eos_spi_xchg32(data, EOS_SPI_FLAG_BSWAP);
    eos_spi_cs_clear();
}

void eos_eve_active(void) {
    eos_eve_command(EVE_ACTIVE, 0);
}

void eos_eve_brightness(uint8_t b) {
	eos_eve_write8(REG_PWM_DUTY, b);
}

static void _dl_inc(uint32_t i) {
	eve_dl_addr += i;
}

void eos_eve_dl_start(uint32_t addr) {
    eve_dl_addr = addr;
}

void eos_eve_dl_write(uint32_t dl) {
    eos_eve_write32(eve_dl_addr, dl);
    _dl_inc(4);
}

void eos_eve_dl_swap(void) {
    eos_eve_write8(REG_DLSWAP, EVE_DLSWAP_FRAME);
}

uint32_t eos_eve_dl_addr(void) {
    return eve_dl_addr;
}

static void _cmd_inc(uint16_t i) {
	eve_cmd_offset += i;
	eve_cmd_offset &= 0x0fff;
}

static void _cmd_begin(uint32_t command) {
    uint8_t flags = 0;

    if (eve_cmd_burst) {
        flags = EOS_SPI_FLAG_TX;
    } else {
    	uint32_t addr = EVE_RAM_CMD + eve_cmd_offset;
        eos_spi_cs_set();
        eos_spi_xchg24(addr | MEM_WRITE, 0);
    }
    eos_spi_xchg32(command, EOS_SPI_FLAG_BSWAP | flags);
    _cmd_inc(4);
}

static void _cmd_end(void) {
    if (!eve_cmd_burst) eos_spi_cs_clear();
}

static void _cmd_string(const char *s, uint8_t flags) {
    int i = 0, p = 0;

    while(s[i] != 0) {
        eos_spi_xchg8(s[i], EOS_SPI_FLAG_BSWAP | flags);
        i++;
    }
    /* padding */
	p = i & 3;  /* 0, 1, 2 or 3 */
	p = 4 - p;  /* 4, 3, 2 or 1 */
	i += p;

	while(p > 0) {
        eos_spi_xchg8(0, EOS_SPI_FLAG_BSWAP | flags);
		p--;
	}
    _cmd_inc(i);
}

static void _cmd_buffer(const char *b, int size, uint8_t flags) {
    int i = 0, p = 0;

    for (i=0; i<size; i++) {
        eos_spi_xchg8(b[i], EOS_SPI_FLAG_BSWAP | flags);
    }
    /* padding */
	p = i & 3;  /* 0, 1, 2 or 3 */
	p = 4 - p;  /* 4, 3, 2 or 1 */
	i += p;

	while(p > 0) {
        eos_spi_xchg8(0, EOS_SPI_FLAG_BSWAP | flags);
		p--;
	}
    _cmd_inc(i);
}

void eos_eve_cmd(uint32_t cmd, const char *fmt, ...) {
    uint8_t flags = eve_cmd_burst ? EOS_SPI_FLAG_TX : 0;
    va_list argv;
    uint16_t *p;
    int i = 0;

    va_start(argv, fmt);
    _cmd_begin(cmd);
    while (fmt[i]) {
        switch (fmt[i]) {
            case 'b':
                eos_spi_xchg8(va_arg(argv, int), EOS_SPI_FLAG_BSWAP | flags);
                _cmd_inc(1);
                break;
            case 'h':
                eos_spi_xchg16(va_arg(argv, int), EOS_SPI_FLAG_BSWAP | flags);
                _cmd_inc(2);
                break;
            case 'w':
                eos_spi_xchg32(va_arg(argv, int), EOS_SPI_FLAG_BSWAP | flags);
                _cmd_inc(4);
                break;
            case 'W':
                p = va_arg(argv, uint16_t *);
                *p = eve_cmd_offset;
                eos_spi_xchg32(0, EOS_SPI_FLAG_BSWAP | flags);
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
    _cmd_end();
	va_end(argv);
}

uint32_t eos_eve_cmd_result(uint16_t offset) {
    return eos_eve_read32(EVE_RAM_CMD + offset);
}

void eos_eve_cmd_dl(uint32_t dl) {
    _cmd_begin(dl);
    _cmd_end();
}

int eos_eve_cmd_done(void) {
	uint16_t r = eos_eve_read16(REG_CMD_READ);
    if (r == 0xfff) {
		eve_cmd_offset = 0;
		eos_eve_write8(REG_CPURESET, 1);
		eos_eve_write16(REG_CMD_READ, 0);
		eos_eve_write16(REG_CMD_WRITE, 0);
		eos_eve_write16(REG_CMD_DL, 0);
		eos_eve_write8(REG_CPURESET, 0);
        return -1;
    }
    return (r == eve_cmd_offset);
}

int eos_eve_cmd_exec(int w) {
    eos_eve_write16(REG_CMD_WRITE, eve_cmd_offset);
    if (w) {
        int r;
        do {
            r = eos_eve_cmd_done();
        } while (!r);
        if (r < 0) return EOS_ERR;
    }
    return EOS_OK;
}

void eos_eve_cmd_burst_start(void) {
	uint32_t addr = EVE_RAM_CMD + eve_cmd_offset;
    eos_spi_cs_set();
    eos_spi_xchg24(addr | MEM_WRITE, EOS_SPI_FLAG_TX);
    eve_cmd_burst = 1;
}

void eos_eve_cmd_burst_end(void) {
    eos_spi_cs_clear();
    eve_cmd_burst = 0;
}

static void eve_handler_int(void) {
    GPIO_REG(GPIO_LOW_IE) &= ~(1 << EVE_PIN_INT);
    eos_evtq_push_isr(EOS_EVT_UI | EVE_ETYPE_INT, NULL, 0);
    return;
}

static void eve_handler_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    uint8_t flags;
    uint8_t touch_tag;
    uint32_t touch_xy;
    uint32_t touch_track;
    uint8_t tag0 = eve_tag0;
    uint8_t touch_idx = 0;
    int touch_idx_max = -1;
    int i;
    EOSTouch *touch;

    eos_spi_dev_start(EOS_SPI_DEV_DISP);
    flags = eos_eve_read8(REG_INT_FLAGS) & eve_int_mask;
    if (!eve_multitouch) {
        touch = &eve_touch[0];

        tag0 = eve_tag0;
        touch->evt &= (EOS_TOUCH_ETYPE_DRAG | EOS_TOUCH_ETYPE_TRACK);
        touch_xy = eos_eve_read32(REG_CTOUCH_TOUCH0_XY);
        if (touch_xy == 0x80008000) {
            touch_tag = 0;
            eve_tag0 = 0;
            touch->t = 0;
            touch->evt &= ~(EOS_TOUCH_ETYPE_DRAG | EOS_TOUCH_ETYPE_TRACK);
            if (eve_int_mask & EVE_INT_CONVCOMPLETE) {
                eve_int_mask &= ~EVE_INT_CONVCOMPLETE;
                eos_eve_write8(REG_INT_MASK, eve_int_mask);
            }
        } else {
            touch->t = 1;
            touch->x = touch_xy >> 16;
            touch->y = touch_xy & 0xffff;
            if ((flags & EVE_INT_TAG) && !(touch->evt & (EOS_TOUCH_ETYPE_DRAG | EOS_TOUCH_ETYPE_TRACK))) {
                touch_tag = eos_eve_read8(REG_TOUCH_TAG);
            } else {
                touch_tag = touch->tag;
            }
            if (touch->evt & EOS_TOUCH_ETYPE_TRACK) {
                touch_track = eos_eve_read32(REG_TRACKER);
                touch->tracker.tag = touch_track & 0xffff;
                touch->tracker.val = touch_track >> 16;
            }
            if (touch->evt & (EOS_TOUCH_ETYPE_DRAG | EOS_TOUCH_ETYPE_TRACK)) {
                touch_idx = 1;
                touch_idx_max = 1;
            }
            if (flags & EVE_INT_TOUCH) {
                eve_multitouch = 1;
                if (!(eve_int_mask & EVE_INT_CONVCOMPLETE)) {
                    eve_int_mask |= EVE_INT_CONVCOMPLETE;
                    eos_eve_write8(REG_INT_MASK, eve_int_mask);
                }
            }
        }
        if ((touch_tag != touch->tag) && !(touch->evt & (EOS_TOUCH_ETYPE_DRAG | EOS_TOUCH_ETYPE_TRACK))) {
            touch->tag_prev = touch->tag;
            touch->tag = touch_tag;
            touch_idx = 1;
            touch_idx_max = 1;
            if (touch->tag) touch->evt |= EOS_TOUCH_ETYPE_DOWN;
            if (touch->tag_prev) touch->evt |= EOS_TOUCH_ETYPE_UP;
            if (touch_tag && !eve_tag0) {
                eve_tag0 = touch_tag;
                tag0 = touch_tag;
                if (eve_touch_evt[touch_tag]) {
                    touch->evt |= eve_touch_evt[touch_tag];
                    touch->tracker.tag = 0;
                    if (!(eve_int_mask & EVE_INT_CONVCOMPLETE)) {
                        eve_int_mask |= EVE_INT_CONVCOMPLETE;
                        eos_eve_write8(REG_INT_MASK, eve_int_mask);
                    }
                }
            }
        }
    } else if (flags & EVE_INT_CONVCOMPLETE) {
        int touched = 0;

        for (i=0; i<EVE_MAX_TOUCH; i++) {
            touch = &eve_touch[i];

            touch->evt &= (EOS_TOUCH_ETYPE_DRAG | EOS_TOUCH_ETYPE_TRACK);
            touch_xy = i < 4 ? eos_eve_read32(_reg_touch[i]) : (((uint32_t)eos_eve_read16(REG_CTOUCH_TOUCH4_X) << 16) | eos_eve_read16(REG_CTOUCH_TOUCH4_Y));
            if (touch_xy == 0x80008000) {
                touch_tag = 0;
                touch->t = 0;
                touch->evt &= ~(EOS_TOUCH_ETYPE_DRAG | EOS_TOUCH_ETYPE_TRACK);
            } else {
                touch->t = 1;
                touch->x = touch_xy >> 16;
                touch->y = touch_xy & 0xffff;
                if (!(touch->evt & (EOS_TOUCH_ETYPE_DRAG | EOS_TOUCH_ETYPE_TRACK))) {
                    touch_tag = eos_eve_read8(_reg_tag[i]);
                } else {
                    touch_tag = touch->tag;
                }
                if (touch->evt & EOS_TOUCH_ETYPE_TRACK) {
                    touch_track = eos_eve_read32(_reg_track[i]);
                    touch->tracker.tag = touch_track & 0xffff;
                    touch->tracker.val = touch_track >> 16;
                }
                if (touch->evt & (EOS_TOUCH_ETYPE_DRAG | EOS_TOUCH_ETYPE_TRACK)) {
                    touch_idx |= (1 << i);
                    touch_idx_max = i + 1;
                }
                touched = 1;
            }
            if (touch_tag != touch->tag) {
                touch->tag_prev = touch->tag;
                touch->tag = touch_tag;
                touch_idx |= (1 << i);
                touch_idx_max = i + 1;
                if (touch->tag) touch->evt |= EOS_TOUCH_ETYPE_DOWN;
                if (touch->tag_prev) touch->evt |= EOS_TOUCH_ETYPE_UP;
                if (eve_touch_evt[touch_tag]) {
                    touch->evt |= eve_touch_evt[touch_tag];
                    touch->tracker.tag = 0;
                }
            }
        }

        if (!touched) {
            eve_tag0 = 0;
            eve_multitouch = 0;
            eve_int_mask &= ~EVE_INT_CONVCOMPLETE;
            eos_eve_write8(REG_INT_MASK, eve_int_mask);
        }
    }

    if (touch_idx_max != -1) {
        for (i=0; i<touch_idx_max; i++) {
            if (touch_idx & 1) eve_renderer(tag0, i);
            touch_idx = touch_idx >> 1;
        }
    }

    GPIO_REG(GPIO_LOW_IP) = (1 << EVE_PIN_INT);
    GPIO_REG(GPIO_LOW_IE) |= (1 << EVE_PIN_INT);
    eos_spi_dev_stop();
}

int eos_eve_init(void) {
	uint8_t chipid = 0;
	uint16_t timeout = 0;
    uint32_t touch_transform[6] = {0xfa46,0xfffffcf6,0x422fe,0xffffff38,0x10002,0xf3cb0};

    eos_eve_command(EVE_RST_PULSE, 0);
    eos_eve_command(EVE_CLKEXT, 0);
    eos_eve_command(EVE_ACTIVE, 0);     /* start EVE */

    while(chipid != 0x7C) {             /* if chipid is not 0x7c, continue to read it until it is, EVE needs a moment for it's power on self-test and configuration */
		eos_timer_sleep(1);
		chipid = eos_eve_read8(REG_ID);
		timeout++;
		if (timeout > 400) return EOS_ERR;
	}

    eos_eve_write8(REG_PWM_DUTY, 0);

    /* Initialize Display */
	eos_eve_write16(REG_HCYCLE,  EVE_HCYCLE);   /* total number of clocks per line, incl front/back porch */
	eos_eve_write16(REG_HOFFSET, EVE_HOFFSET);  /* start of active line */
	eos_eve_write16(REG_HSYNC0,  EVE_HSYNC0);   /* start of horizontal sync pulse */
	eos_eve_write16(REG_HSYNC1,  EVE_HSYNC1);   /* end of horizontal sync pulse */
	eos_eve_write16(REG_VCYCLE,  EVE_VCYCLE);   /* total number of lines per screen, including pre/post */
	eos_eve_write16(REG_VOFFSET, EVE_VOFFSET);  /* start of active screen */
	eos_eve_write16(REG_VSYNC0,  EVE_VSYNC0);   /* start of vertical sync pulse */
	eos_eve_write16(REG_VSYNC1,  EVE_VSYNC1);   /* end of vertical sync pulse */
	eos_eve_write8(REG_SWIZZLE,  EVE_SWIZZLE);  /* FT8xx output to LCD - pin order */
	eos_eve_write8(REG_PCLK_POL, EVE_PCLKPOL);  /* LCD data is clocked in on this PCLK edge */
	eos_eve_write8(REG_CSPREAD,	 EVE_CSPREAD);  /* helps with noise, when set to 1 fewer signals are changed simultaneously, reset-default: 1 */
	eos_eve_write16(REG_HSIZE,   EVE_HSIZE);    /* active display width */
	eos_eve_write16(REG_VSIZE,   EVE_VSIZE);    /* active display height */

	/* do not set PCLK yet - wait for just after the first display list */

	/* configure Touch */
	eos_eve_write8(REG_TOUCH_MODE, EVE_TMODE_CONTINUOUS);       /* enable touch */
	eos_eve_write16(REG_TOUCH_RZTHRESH, EVE_TOUCH_RZTHRESH);    /* eliminate any false touches */

	/* disable Audio for now */
	eos_eve_write8(REG_VOL_PB, 0x00);       /* turn recorded audio volume down */
	eos_eve_write8(REG_VOL_SOUND, 0x00);    /* turn synthesizer volume off */
	eos_eve_write16(REG_SOUND, 0x6000);     /* set synthesizer to mute */

	/* write a basic display-list to get things started */
    eos_eve_dl_start(EVE_RAM_DL);
    eos_eve_dl_write(CLEAR_COLOR_RGB(0,0,0));
	eos_eve_dl_write(CLEAR(1,1,1));
    eos_eve_dl_write(DISPLAY());
    eos_eve_dl_swap();

	/* nothing is being displayed yet... the pixel clock is still 0x00 */
	eos_eve_write8(REG_GPIO, 0x80);         /* enable the DISP signal to the LCD panel, it is set to output in REG_GPIO_DIR by default */
	eos_eve_write8(REG_PCLK, EVE_PCLK);     /* now start clocking data to the LCD panel */

    eos_eve_write8(REG_INT_EN, 0x01);
    eos_eve_write8(REG_INT_MASK, eve_int_mask);
    while(eos_eve_read8(REG_INT_FLAGS));

    eos_evtq_set_handler(EOS_EVT_UI, eve_handler_evt);

    GPIO_REG(GPIO_INPUT_EN)     |=  (1 << EVE_PIN_INT);
    GPIO_REG(GPIO_OUTPUT_EN)    &= ~(1 << EVE_PIN_INT);
    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << EVE_PIN_INT);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << EVE_PIN_INT);

    GPIO_REG(GPIO_LOW_IE)       |=  (1 << EVE_PIN_INT);
    eos_intr_set(INT_GPIO_BASE + EVE_PIN_INT, IRQ_PRIORITY_UI, eve_handler_int);

    /*
    // eos_eve_cmd_burst_start();
	eos_eve_cmd_dl(CMD_DLSTART);
	eos_eve_cmd_dl(CLEAR_COLOR_RGB(0,0,0));
	eos_eve_cmd_dl(CLEAR(1,1,1));
    eos_eve_cmd(CMD_TEXT, "hhhhs", EVE_HSIZE/2, EVE_VSIZE/2, 27, EVE_OPT_CENTER, "Please tap on the dot.");
    eos_eve_cmd(CMD_CALIBRATE, "w", 0);
	eos_eve_cmd_dl(DISPLAY());
	eos_eve_cmd_dl(CMD_SWAP);
    // eos_eve_cmd_burst_end();
	eos_eve_cmd_exec(1);

    uint32_t touch_transform[0] = eos_eve_read32(REG_TOUCH_TRANSFORM_A);
    uint32_t touch_transform[1] = eos_eve_read32(REG_TOUCH_TRANSFORM_B);
    uint32_t touch_transform[2] = eos_eve_read32(REG_TOUCH_TRANSFORM_C);
    uint32_t touch_transform[3] = eos_eve_read32(REG_TOUCH_TRANSFORM_D);
    uint32_t touch_transform[4] = eos_eve_read32(REG_TOUCH_TRANSFORM_E);
    uint32_t touch_transform[5] = eos_eve_read32(REG_TOUCH_TRANSFORM_F);

    printf("TOUCH TRANSFORM:{0x%x,0x%x,0x%x,0x%x,0x%x,0x%x}\n", touch_transform[0], touch_transform[1], touch_transform[2], touch_transform[3], touch_transform[4], touch_transform[5]);
    */

    eos_eve_write32(REG_TOUCH_TRANSFORM_A, touch_transform[0]);
    eos_eve_write32(REG_TOUCH_TRANSFORM_B, touch_transform[1]);
    eos_eve_write32(REG_TOUCH_TRANSFORM_C, touch_transform[2]);
    eos_eve_write32(REG_TOUCH_TRANSFORM_D, touch_transform[3]);
    eos_eve_write32(REG_TOUCH_TRANSFORM_E, touch_transform[4]);
    eos_eve_write32(REG_TOUCH_TRANSFORM_F, touch_transform[5]);
    eos_eve_write32(REG_CTOUCH_EXTENDED, 0x00);

    eos_eve_cmd(CMD_SETROTATE, "w", 2);
    eos_eve_cmd_exec(1);

    eos_timer_sleep(500);
    eos_eve_command(EVE_STANDBY, 0);

    return EOS_OK;
}

void eos_eve_set_renderer(eos_eve_fptr_t renderer, uint8_t flags) {
    eve_renderer = renderer;
    eos_evtq_set_flags(EOS_EVT_UI | EVE_ETYPE_INT, flags);
}

EOSTouch *eos_touch_evt(uint8_t tag0, int touch_idx, uint8_t tag_min, uint8_t tag_max, uint8_t *evt) {
    uint8_t tag;
    EOSTouch *ret = NULL;

    *evt = 0;
    if ((touch_idx < 0) || (touch_idx > 4)) return ret;

    ret = &eve_touch[touch_idx];
    if ((tag0 < tag_min) || (tag0 > tag_max)) return ret;

    tag = eve_touch[touch_idx].tag;
    if ((tag >= tag_min) && (tag <= tag_max)) *evt |= (eve_touch[touch_idx].evt & (EOS_TOUCH_ETYPE_DOWN | EOS_TOUCH_ETYPE_DRAG));

    tag = eve_touch[touch_idx].tag_prev;
    if ((tag >= tag_min) && (tag <= tag_max)) *evt |= (eve_touch[touch_idx].evt & EOS_TOUCH_ETYPE_UP);

    tag = eve_touch[touch_idx].tracker.tag;
    if ((tag >= tag_min) && (tag <= tag_max)) *evt |= (eve_touch[touch_idx].evt & EOS_TOUCH_ETYPE_TRACK);

    return ret;
}

void eos_touch_set_evt(uint8_t tag, uint8_t evt) {
    eve_touch_evt[tag] = evt;
}
