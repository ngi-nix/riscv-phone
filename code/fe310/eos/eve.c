#include <stdarg.h>

#include "eos.h"
#include "spi.h"
#include "timer.h"
#include "eve.h"

#define MEM_WRITE 0x800000

static char cmd_burst;
static uint16_t cmd_offset;
static uint16_t dl_offset;

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

void eos_eve_standby(void) {
    eos_eve_command(EVE_STANDBY, 0);
}

void eos_eve_sleep(void) {
    eos_eve_command(EVE_SLEEP, 0);
}

void eos_eve_pwrdown(void) {
    eos_eve_command(EVE_PWRDOWN, 0);
}

void eos_eve_reset(void) {
    eos_eve_command(EVE_RST_PULSE, 0);
}

void eos_eve_brightness(uint8_t b) {
	eos_eve_write8(REG_PWM_DUTY, b);
}

static void _dl_inc(uint16_t i) {
	dl_offset += i;
}

void eos_eve_dl_start(uint16_t offset) {
    dl_offset = offset;
}

void eos_eve_dl_write(uint32_t dl) {
    uint32_t addr = EVE_RAM_DL + dl_offset;
    eos_eve_write32(addr, dl);
    _dl_inc(4);
}

void eos_eve_dl_swap(void) {
    eos_eve_write8(REG_DLSWAP, EVE_DLSWAP_FRAME);
}

uint16_t eos_eve_dl_offset(void) {
    return dl_offset;
}

static void _cmd_inc(uint16_t i) {
	cmd_offset += i;
	cmd_offset &= 0x0fff;
}

static void _cmd_begin(uint32_t command) {
    uint8_t flags = 0;

    if (cmd_burst) {
        flags = EOS_SPI_FLAG_TX;
    } else {
    	uint32_t addr = EVE_RAM_CMD + cmd_offset;
        eos_spi_cs_set();
        eos_spi_xchg24(addr | MEM_WRITE, 0);
    }
    eos_spi_xchg32(command, EOS_SPI_FLAG_BSWAP | flags);
    _cmd_inc(4);
}

static void _cmd_end(void) {
    if (!cmd_burst) eos_spi_cs_clear();
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

void eos_eve_cmd(uint32_t cmd, const char *fmt, ...) {
    uint8_t flags = cmd_burst ? EOS_SPI_FLAG_TX : 0;
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
                *p = cmd_offset;
                eos_spi_xchg32(0, EOS_SPI_FLAG_BSWAP | flags);
                _cmd_inc(4);
                break;
            case 's':
                _cmd_string(va_arg(argv, const char *), flags);
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
		cmd_offset = 0;
		eos_eve_write8(REG_CPURESET, 1);
		eos_eve_write16(REG_CMD_READ, 0);
		eos_eve_write16(REG_CMD_WRITE, 0);
		eos_eve_write16(REG_CMD_DL, 0);
		eos_eve_write8(REG_CPURESET, 0);
        return -1;
    }
    return (r == cmd_offset);
}

int eos_eve_cmd_exec(int w) {
    eos_eve_write16(REG_CMD_WRITE, cmd_offset);
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
	uint32_t addr = EVE_RAM_CMD + cmd_offset;
    eos_spi_cs_set();
    eos_spi_xchg24(addr | MEM_WRITE, EOS_SPI_FLAG_TX);
    cmd_burst = 1;
}

void eos_eve_cmd_burst_end(void) {
    eos_spi_cs_clear();
    cmd_burst = 0;
}

int eos_eve_init(void) {
	uint8_t chipid = 0;
	uint16_t timeout = 0;

    eos_eve_reset();
    eos_eve_command(EVE_CLKEXT, 0);
    eos_eve_active();           /* start EVE */

    while(chipid != 0x7C) {     /* if chipid is not 0x7c, continue to read it until it is, EVE needs a moment for it's power on self-test and configuration */
		eos_timer_sleep(1);
		chipid = eos_eve_read8(REG_ID);
		timeout++;
		if (timeout > 400) return EOS_ERR;
	}

    eos_eve_brightness(0);

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
    eos_eve_dl_start(0);
    eos_eve_dl_write(CLEAR_COLOR_RGB(0,0,0));
	eos_eve_dl_write(CLEAR(1,1,1));
    eos_eve_dl_write(DISPLAY());
    eos_eve_dl_swap();

	/* nothing is being displayed yet... the pixel clock is still 0x00 */
	eos_eve_write8(REG_GPIO, 0x80);         /* enable the DISP signal to the LCD panel, it is set to output in REG_GPIO_DIR by default */
	eos_eve_write8(REG_PCLK, EVE_PCLK);     /* now start clocking data to the LCD panel */

    eos_timer_sleep(500);
    eos_eve_standby();

    return EOS_OK;
}

