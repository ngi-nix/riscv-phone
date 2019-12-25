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

#define MEM_WRITE   0x800000

#define EVE_PIN_INT 0

static char eve_cmd_burst;
static uint16_t eve_cmd_offset;
static uint32_t eve_dl_addr;

static int eve_int_mask = EVE_INT_TAG | EVE_INT_TOUCH;
static int eve_multitouch = 0;
static uint8_t eve_tag0;
EVETag eve_tag[5];
static eos_eve_fptr_t eve_renderer;

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
    eos_evtq_push_isr(EOS_EVT_UI, NULL, 0);
    return;
}

static void eve_handler_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    char render = 0;
    int touch_tag_idx = -1;
    uint8_t flags = eos_eve_read8(REG_INT_FLAGS) & eve_int_mask;
    uint8_t touch_tag;
    uint32_t touch_xy;
    EVETag *tag;

    if (!eve_multitouch) {
        if (flags & EVE_INT_TAG) {
            touch_tag = eos_eve_read8(REG_TOUCH_TAG);
            eve_tag[0].value_prev = eve_tag[0].value;
            eve_tag[0].value = touch_tag;
            touch_tag_idx = 0;
            if (touch_tag) eve_tag0 = touch_tag;
        }
        touch_xy = eos_eve_read32(REG_CTOUCH_TOUCH0_XY);
        if (touch_xy == 0x80008000) {
            eve_tag[0].t = 0;
        } else {
            eve_tag[0].t = 1;
            eve_tag[0].x = touch_xy >> 16;
            eve_tag[0].y = touch_xy & 0xffff;
            if (flags & EVE_INT_TOUCH) {
                eve_multitouch = 1;
                eve_int_mask |= EVE_INT_CONVCOMPLETE;
                eos_eve_write8(REG_INT_MASK, eve_int_mask);
            }
        }
    } else if (flags & EVE_INT_CONVCOMPLETE) {
        tag = &eve_tag[0];
        touch_xy = eos_eve_read32(REG_CTOUCH_TOUCH0_XY);
        if (touch_xy == 0x80008000) {
            touch_tag = 0;
            tag->t = 0;
        } else {
            touch_tag = eos_eve_read8(REG_TOUCH_TAG);
            tag->t = 1;
            tag->x = touch_xy >> 16;
            tag->y = touch_xy & 0xffff;
        }
        if (touch_tag != tag->value) {
            tag->value_prev = tag->value;
            tag->value = touch_tag;
            touch_tag_idx = 0;
        }

        tag = &eve_tag[1];
        touch_xy = eos_eve_read32(REG_CTOUCH_TOUCH1_XY);
        if (touch_xy == 0x80008000) {
            touch_tag = 0;
            tag->t = 0;
        } else {
            touch_tag = eos_eve_read8(REG_TOUCH_TAG1);
            tag->t = 1;
            tag->x = touch_xy >> 16;
            tag->y = touch_xy & 0xffff;
        }
        if (touch_tag != tag->value) {
            tag->value_prev = tag->value;
            tag->value = touch_tag;
            touch_tag_idx = 1;
        }

        tag = &eve_tag[2];
        touch_xy = eos_eve_read32(REG_CTOUCH_TOUCH2_XY);
        if (touch_xy == 0x80008000) {
            touch_tag = 0;
            tag->t = 0;
        } else {
            touch_tag = eos_eve_read8(REG_TOUCH_TAG2);
            tag->t = 1;
            tag->x = touch_xy >> 16;
            tag->y = touch_xy & 0xffff;
        }
        if (touch_tag != tag->value) {
            tag->value_prev = tag->value;
            tag->value = touch_tag;
            touch_tag_idx = 2;
        }

        tag = &eve_tag[3];
        touch_xy = eos_eve_read32(REG_CTOUCH_TOUCH3_XY);
        if (touch_xy == 0x80008000) {
            touch_tag = 0;
            tag->t = 0;
        } else {
            touch_tag = eos_eve_read8(REG_TOUCH_TAG3);
            tag->t = 1;
            tag->x = touch_xy >> 16;
            tag->y = touch_xy & 0xffff;
        }
        if (touch_tag != tag->value) {
            tag->value_prev = tag->value;
            tag->value = touch_tag;
            touch_tag_idx = 3;
        }

        tag = &eve_tag[4];
        touch_xy = ((uint32_t)eos_eve_read16(REG_CTOUCH_TOUCH4_X) << 16) | eos_eve_read16(REG_CTOUCH_TOUCH4_Y);
        if (touch_xy == 0x80008000) {
            touch_tag = 0;
            tag->t = 0;
        } else {
            touch_tag = eos_eve_read8(REG_TOUCH_TAG4);
            tag->t = 1;
            tag->x = touch_xy >> 16;
            tag->y = touch_xy & 0xffff;
        }
        if (touch_tag != tag->value) {
            tag->value_prev = tag->value;
            tag->value = touch_tag;
            touch_tag_idx = 4;
        }

        if (!eve_tag[1].t && !eve_tag[2].t && !eve_tag[3].t && !eve_tag[4].t) {
            eve_multitouch = 0;
            eve_int_mask &= ~EVE_INT_CONVCOMPLETE;
            eos_eve_write8(REG_INT_MASK, eve_int_mask);
        }
    }

    if (touch_tag_idx != -1) render = 1;
    if (render) eve_renderer(eve_tag0, touch_tag_idx);

    /*
    uint32_t track = eos_eve_read32(REG_TRACKER);
    printf("F:%x TAG:%x %x %x %x %x %x\n", flags, eve_tag0, eve_tag[0].value, eve_tag[1].value, eve_tag[2].value, eve_tag[3].value, eve_tag[4].value);
    if (flags & 0x04) {
        track_tag = track & 0xffff;
        track_val = track >> 16;
    }
    */

    GPIO_REG(GPIO_LOW_IP) = (1 << EVE_PIN_INT);
    GPIO_REG(GPIO_LOW_IE) |= (1 << EVE_PIN_INT);
    return;
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

void eos_eve_set_renderer(eos_eve_fptr_t renderer) {
    eve_renderer = renderer;
}

EVETag *eos_eve_tag(void) {
    return eve_tag;
}
