#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "eve.h"

#define EVE_MEM_WRITE           0x800000

static char cmd_burst;
static uint16_t cmd_offset;

static char dl_burst;
static uint32_t dl_addr;

static uint32_t *touch_calib;
static uint8_t brigtness;
static uint8_t power_state;

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

void eve_readb(uint32_t addr, uint8_t *b, size_t size) {
    int i;

    eve_spi_cs_set();
    eve_spi_xchg32(addr << 8, 0);
    for (i=0; i<size; i++) {
        b[i] = eve_spi_xchg8(0, 0);
    }
    eve_spi_cs_clear();
}

void eve_writeb(uint32_t addr, uint8_t *b, size_t size) {
    int i;

    eve_spi_cs_set();
    eve_spi_xchg24(addr | EVE_MEM_WRITE, 0);
    for (i=0; i<size; i++) {
        eve_spi_xchg8(b[i], 0);
    }
    eve_spi_cs_clear();
}

static void dl_inc(uint32_t i) {
    dl_addr += i;
}

void eve_dl_start(uint32_t addr, char burst) {
    dl_addr = addr;
    dl_burst = burst;
    if (burst) {
        eve_spi_lock();
        eve_spi_cs_set();
        eve_spi_xchg24(addr | EVE_MEM_WRITE, EVE_SPI_FLAG_TX);
    }
}

void eve_dl_write(uint32_t dl) {
    if (dl_burst) {
        eve_spi_xchg32(dl, EVE_SPI_FLAG_TX | EVE_SPI_FLAG_BSWAP);
    } else {
        eve_write32(dl_addr, dl);
    }
    dl_inc(4);
}

void eve_dl_end(void) {
    if (dl_burst) {
        eve_spi_flush();
        eve_spi_cs_clear();
        eve_spi_unlock();
        dl_burst = 0;
    }
}

void eve_dl_swap(void) {
    eve_write8(REG_DLSWAP, EVE_DLSWAP_FRAME);
}

uint32_t eve_dl_get_addr(void) {
    return dl_addr;
}

static void cmd_inc(uint16_t i) {
    cmd_offset += i;
    cmd_offset &= 0x0fff;
}

static void cmd_begin(uint32_t command) {
    uint8_t flags = 0;

    if (cmd_burst) {
        flags = EVE_SPI_FLAG_TX;
    } else {
        uint32_t addr = EVE_RAM_CMD + cmd_offset;
        eve_spi_cs_set();
        eve_spi_xchg24(addr | EVE_MEM_WRITE, 0);
    }
    eve_spi_xchg32(command, EVE_SPI_FLAG_BSWAP | flags);
    cmd_inc(4);
}

static void cmd_end(void) {
    if (!cmd_burst) eve_spi_cs_clear();
}

static void cmd_string(const char *s, uint8_t flags) {
    int i = 0;

    while (s[i] != 0) {
        eve_spi_xchg8(s[i], flags);
        i++;
    }
    eve_spi_xchg8(0, flags);
    i++;
    cmd_inc(i);
}

static void cmd_buffer(const char *b, int size, uint8_t flags) {
    int i = 0;

    for (i=0; i<size; i++) {
        eve_spi_xchg8(b[i], flags);
    }
    cmd_inc(size);
}

void eve_cmd(uint32_t cmd, const char *fmt, ...) {
    uint8_t flags = cmd_burst ? (EVE_SPI_FLAG_TX | EVE_SPI_FLAG_BSWAP) : EVE_SPI_FLAG_BSWAP;
    va_list argv;
    uint16_t *p;
    int i = 0;

    va_start(argv, fmt);
    cmd_begin(cmd);
    while (fmt[i]) {
        switch (fmt[i]) {
            case 'b':
                eve_spi_xchg8(va_arg(argv, int), flags);
                cmd_inc(1);
                break;
            case 'h':
                eve_spi_xchg16(va_arg(argv, int), flags);
                cmd_inc(2);
                break;
            case 'w':
                eve_spi_xchg32(va_arg(argv, int), flags);
                cmd_inc(4);
                break;
            case '&':
                p = va_arg(argv, uint16_t *);
                *p = cmd_offset;
                eve_spi_xchg32(0, flags);
                cmd_inc(4);
                break;
            case 's':
                cmd_string(va_arg(argv, const char *), flags);
                break;
            case 'p':
                cmd_buffer(va_arg(argv, const char *), va_arg(argv, int), flags);
                break;
        }
        i++;
    }
    va_end(argv);
    /* padding */
    i = cmd_offset & 3;  /* equivalent to cmd_offset % 4 */
    if (i) {
        i = 4 - i;  /* 3, 2 or 1 */
        cmd_inc(i);

        while (i > 0) {
            eve_spi_xchg8(0, flags);
            i--;
        }
    }
    cmd_end();
}

uint32_t eve_cmd_result(uint16_t offset) {
    return eve_read32(EVE_RAM_CMD + offset);
}

void eve_cmd_dl(uint32_t dl) {
    cmd_begin(dl);
    cmd_end();
}

int eve_cmd_done(void) {
    uint16_t r = eve_read16(REG_CMD_READ);
    if (r == 0xfff) {
        cmd_offset = 0;
        eve_write8(REG_CPURESET, 1);
        eve_write16(REG_CMD_READ, 0);
        eve_write16(REG_CMD_WRITE, 0);
        eve_write16(REG_CMD_DL, 0);
        eve_write8(REG_CPURESET, 0);
        return -1;
    }
    return (r == cmd_offset);
}

int eve_cmd_exec(int w) {
    eve_write16(REG_CMD_WRITE, cmd_offset);
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
    uint32_t addr = EVE_RAM_CMD + cmd_offset;
    eve_spi_lock();
    eve_spi_cs_set();
    eve_spi_xchg24(addr | EVE_MEM_WRITE, EVE_SPI_FLAG_TX);
    cmd_burst = 1;
}

void eve_cmd_burst_end(void) {
    eve_spi_flush();
    eve_spi_cs_clear();
    eve_spi_unlock();
    cmd_burst = 0;
}

int eve_gpio_get(int gpio) {
    uint16_t reg = eve_read16(REG_GPIOX);
    return !!(reg & (1 << gpio));
}

void eve_gpio_set(int gpio, int val) {
    uint16_t reg = eve_read16(REG_GPIOX);
    uint16_t reg_val = (1 << gpio);
    if (val) {
        reg |= reg_val;
    } else {
        reg &= ~reg_val;
    }
    eve_write16(REG_GPIOX, reg);
}

uint8_t eve_gpio_get_dir(void) {
    uint16_t reg = eve_read16(REG_GPIOX_DIR);
    return reg & 0x000f;
}

void eve_gpio_set_dir(uint8_t dir) {
    uint16_t reg = eve_read16(REG_GPIOX_DIR);
    reg &= 0xfff0;
    reg |= dir & 0x0f;
    eve_write16(REG_GPIOX_DIR, reg);
}

void eve_active(void) {
    eve_command(EVE_ACTIVE, 0);
    if (power_state == EVE_PSTATE_SLEEP) eve_time_sleep(40);

    eve_write8(REG_PWM_DUTY, brigtness);
    if (power_state != EVE_PSTATE_SLEEP) return;

    eve_write8(REG_TOUCH_MODE, EVE_TMODE_CONTINUOUS);
    eve_write8(REG_CTOUCH_EXTENDED, 0x00);

    power_state = EVE_PSTATE_ACTIVE;
}

void eve_standby(void) {
    if (power_state != EVE_PSTATE_ACTIVE) return;

    brigtness = eve_read8(REG_PWM_DUTY);
    eve_command(EVE_STANDBY, 0);

    power_state = EVE_PSTATE_STANDBY;
}

void eve_sleep(void) {
    if (power_state != EVE_PSTATE_ACTIVE) return;

    brigtness = eve_read8(REG_PWM_DUTY);
    eve_write8(REG_PWM_DUTY, 0x0);

    eve_write8(REG_CTOUCH_EXTENDED, 0x01);
    eve_write8(REG_TOUCH_MODE, EVE_TMODE_OFF);

    eve_time_sleep(500);
    eve_command(EVE_SLEEP, 0);

    power_state = EVE_PSTATE_SLEEP;
}

void eve_brightness(uint8_t b) {
    eve_write8(REG_PWM_DUTY, b);
}

void eve_set_touch_calibration(uint32_t *matrix) {
    touch_calib = matrix;
}

static int _init(uint8_t gpio_dir) {
    uint8_t chipid = 0;
    uint16_t timeout = 0;

    eve_command(EVE_RST_PULSE, 0);
    eve_command(EVE_CLKEXT, 0);
    eve_command(EVE_ACTIVE, 0);         /* start EVE */

    while (chipid != 0x7C) {            /* if chipid is not 0x7c, continue to read it until it is, EVE needs a moment for it's power on self-test and configuration */
        eve_time_sleep(1);
        chipid = eve_read8(REG_ID);
        timeout++;
        if (timeout > 400) return EVE_ERR;
    }

    eve_write8(REG_PWM_DUTY, 0);
    eve_write16(REG_GPIOX_DIR, 0x8000 | (gpio_dir & 0x0f));
    eve_write16(REG_GPIOX, 0);

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
    eve_write8(REG_CSPREAD,  EVE_CSPREAD);  /* helps with noise, when set to 1 fewer signals are changed simultaneously, reset-default: 1 */
    eve_write16(REG_HSIZE,   EVE_HSIZE);    /* active display width */
    eve_write16(REG_VSIZE,   EVE_VSIZE);    /* active display height */

    /* do not set PCLK yet - wait for just after the first display list */

    /* disable Audio */
    eve_write16(REG_SOUND, 0x0000);         /* set synthesizer to silence */
    eve_write8(REG_VOL_SOUND, 0x00);        /* turn synthesizer volume off */
    eve_write8(REG_VOL_PB, 0x00);           /* turn recorded audio volume off */

    /* write a basic display-list to get things started */
    eve_dl_start(EVE_RAM_DL, 0);
    eve_dl_write(CLEAR_COLOR_RGB(0,0,0));
    eve_dl_write(CLEAR(1,1,1));
    eve_dl_write(DISPLAY());
    eve_dl_swap();

    /* nothing is being displayed yet... the pixel clock is still 0x00 */
    eve_write16(REG_GPIOX, 0x8000);         /* enable the DISP signal to the LCD panel, it is set to output in REG_GPIOX_DIR by default */
    eve_write8(REG_PCLK, EVE_PCLK);         /* now start clocking data to the LCD panel */

    /* configure Touch */
    eve_write16(REG_TOUCH_CONFIG, 0xb81);                   /* enable touch low power mode: 0xb81 - default: 0x381 */
    eve_write8(REG_TOUCH_MODE, EVE_TMODE_CONTINUOUS);       /* enable touch */
    eve_write16(REG_TOUCH_RZTHRESH, EVE_TOUCH_RZTHRESH);    /* eliminate any false touches */

    if (touch_calib) {
        eve_write32(REG_TOUCH_TRANSFORM_A, touch_calib[0]);
        eve_write32(REG_TOUCH_TRANSFORM_B, touch_calib[1]);
        eve_write32(REG_TOUCH_TRANSFORM_C, touch_calib[2]);
        eve_write32(REG_TOUCH_TRANSFORM_D, touch_calib[3]);
        eve_write32(REG_TOUCH_TRANSFORM_E, touch_calib[4]);
        eve_write32(REG_TOUCH_TRANSFORM_F, touch_calib[5]);
    } else {
        uint32_t touch_calib[6];
        eve_cmd_dl(CMD_DLSTART);
        eve_cmd_dl(CLEAR_COLOR_RGB(0,0,0));
        eve_cmd_dl(CLEAR(1,1,1));
        eve_cmd(CMD_TEXT, "hhhhs", EVE_HSIZE/2, EVE_VSIZE/2, 27, EVE_OPT_CENTER, "Please tap on the dot.");
        eve_cmd(CMD_CALIBRATE, "w", 0);
        eve_cmd_dl(DISPLAY());
        eve_cmd_dl(CMD_SWAP);
        eve_cmd_exec(1);

        touch_calib[0] = eve_read32(REG_TOUCH_TRANSFORM_A);
        touch_calib[1] = eve_read32(REG_TOUCH_TRANSFORM_B);
        touch_calib[2] = eve_read32(REG_TOUCH_TRANSFORM_C);
        touch_calib[3] = eve_read32(REG_TOUCH_TRANSFORM_D);
        touch_calib[4] = eve_read32(REG_TOUCH_TRANSFORM_E);
        touch_calib[5] = eve_read32(REG_TOUCH_TRANSFORM_F);

        printf("TOUCH TRANSFORM:{0x%x,0x%x,0x%x,0x%x,0x%x,0x%x}\n", touch_calib[0], touch_calib[1], touch_calib[2], touch_calib[3], touch_calib[4], touch_calib[5]);
    }

    eve_write8(REG_CTOUCH_EXTENDED, 0x00);

    eve_cmd(CMD_SETROTATE, "w", 2);
    eve_cmd_exec(1);

    return EVE_OK;
}

int eve_init(int pwr_on, uint8_t gpio_dir) {
    eve_spi_start();

    pwr_on = 1; // override this for now

    if (pwr_on) {
        int rv = _init(gpio_dir);
        if (rv) return rv;
    } else {
        power_state = EVE_PSTATE_SLEEP;
        eve_active();
    }

    eve_touch_init();
    eve_platform_init();

    eve_spi_stop();

    return EVE_OK;
}
