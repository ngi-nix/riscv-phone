#include <stdlib.h>
#include <stdint.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"
#include "timer.h"
#include "i2s.h"
#include "net.h"
#include "spi_dev.h"
#include "eve/eve.h"

#include "board.h"

#include "lcd.h"

#define BIT_PUT(b, pin)     if (b) GPIO_REG(GPIO_OUTPUT_VAL) |= (1 << (pin)); else GPIO_REG(GPIO_OUTPUT_VAL) &= ~(1 << (pin));
#define BIT_GET(pin)        ((GPIO_REG(GPIO_INPUT_VAL) & (1 << (pin))) >> (pin))

#define SCK_UP              { GPIO_REG(GPIO_OUTPUT_VAL) |= (1 << IOF_SPI1_SCK); }
#define SCK_DN              { GPIO_REG(GPIO_OUTPUT_VAL) &= ~(1 << IOF_SPI1_SCK); }

static inline void _sleep(int n) {
    volatile int x = n;

    while(x) x--;
}

int eos_lcd_select(void) {
    if (eos_i2s_running()) return EOS_ERR_BUSY;
    if (eos_spi_dev() != EOS_SPI_DEV_NET) return EOS_ERR_BUSY;

    eos_net_stop();

    GPIO_REG(GPIO_OUTPUT_VAL)   &= ~(1 << LCD_PIN_CS);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << LCD_PIN_CS);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << LCD_PIN_CS);

    GPIO_REG(GPIO_IOF_EN)       &=  ~SPI_IOF_MASK;

    return EOS_OK;
}

void eos_lcd_deselect(void) {
    GPIO_REG(GPIO_IOF_EN) |= SPI_IOF_MASK;
    eos_net_start();
}

void eos_lcd_cs_set(void) {
    GPIO_REG(GPIO_OUTPUT_VAL) |= (1 << LCD_PIN_CS);
}

void eos_lcd_cs_clear(void) {
    GPIO_REG(GPIO_OUTPUT_VAL) &= ~(1 << LCD_PIN_CS);
}

/* sck frequency for r/w operations is 0.8Mhz */
void eos_lcd_write(uint8_t dc, uint8_t data) {
    int i;

    BIT_PUT(dc, IOF_SPI1_MOSI);
    _sleep(10);
    SCK_UP;
    for (i=0; i<8; i++) {
        _sleep(10);
        SCK_DN;
        BIT_PUT(data & 0x80, IOF_SPI1_MOSI);
        _sleep(10);
        SCK_UP;
        data = data << 1;
    }
    _sleep(10);
    SCK_DN;
}

void eos_lcd_read(uint8_t *data) {
    int i;

    *data = 0;
    for (i=0; i<8; i++) {
        _sleep(10);
        *data = *data << 1;
        *data |= BIT_GET(IOF_SPI1_MISO);
        SCK_UP;
        _sleep(10);
        SCK_DN;
    }
}

static int _init(void) {
    int rv;

    rv = eos_lcd_select();
    if (rv) return rv;
    eos_lcd_cs_set();

    /* LCD Setting */
    eos_lcd_write(0, 0xFF);    // change to Page 1 CMD
    eos_lcd_write(1, 0xFF);
    eos_lcd_write(1, 0x98);
    eos_lcd_write(1, 0x06);
    eos_lcd_write(1, 0x04);
    eos_lcd_write(1, 0x01);

    // eos_lcd_write(0, 0x08); // Output SDA
    // eos_lcd_write(1, 0x10);

    eos_lcd_write(0, 0x20);    // set DE/VSYNC mode
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x21);    // DE = 1 Active
    eos_lcd_write(1, 0x01);

    eos_lcd_write(0, 0x30);    // resolution setting 480 X 854
    eos_lcd_write(1, 0x01);

    eos_lcd_write(0, 0x31);    // inversion setting 2-dot
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x40);    // BT AVDD,AVDD
    eos_lcd_write(1, 0x16);

    eos_lcd_write(0, 0x41);
    eos_lcd_write(1, 0x33);    // 22

    eos_lcd_write(0, 0x42);
    eos_lcd_write(1, 0x03);    // VGL=DDVDH+VCIP-DDVDL, VGH=2DDVDL-VCIP

    eos_lcd_write(0, 0x43);
    eos_lcd_write(1, 0x09);    // set VGH clamp level

    eos_lcd_write(0, 0x44);
    eos_lcd_write(1, 0x06);    // set VGL clamp level

    eos_lcd_write(0, 0x50);    // VREG1
    eos_lcd_write(1, 0x88);

    eos_lcd_write(0, 0x51);    // VREG2
    eos_lcd_write(1, 0x88);

    eos_lcd_write(0, 0x52);    // flicker MSB
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x53);    // flicker LSB
    eos_lcd_write(1, 0x49);    // VCOM

    eos_lcd_write(0, 0x55);    // flicker
    eos_lcd_write(1, 0x49);

    eos_lcd_write(0, 0x60);
    eos_lcd_write(1, 0x07);

    eos_lcd_write(0, 0x61);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x62);
    eos_lcd_write(1, 0x07);

    eos_lcd_write(0, 0x63);
    eos_lcd_write(1, 0x00);

    /* Gamma Setting */
    eos_lcd_write(0, 0xA0);    // positive Gamma
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0xA1);
    eos_lcd_write(1, 0x09);

    eos_lcd_write(0, 0xA2);
    eos_lcd_write(1, 0x11);

    eos_lcd_write(0, 0xA3);
    eos_lcd_write(1, 0x0B);

    eos_lcd_write(0, 0xA4);
    eos_lcd_write(1, 0x05);

    eos_lcd_write(0, 0xA5);
    eos_lcd_write(1, 0x08);

    eos_lcd_write(0, 0xA6);
    eos_lcd_write(1, 0x06);

    eos_lcd_write(0, 0xA7);
    eos_lcd_write(1, 0x04);

    eos_lcd_write(0, 0xA8);
    eos_lcd_write(1, 0x09);

    eos_lcd_write(0, 0xA9);
    eos_lcd_write(1, 0x0C);

    eos_lcd_write(0, 0xAA);
    eos_lcd_write(1, 0x15);

    eos_lcd_write(0, 0xAB);
    eos_lcd_write(1, 0x08);

    eos_lcd_write(0, 0xAC);
    eos_lcd_write(1, 0x0F);

    eos_lcd_write(0, 0xAD);
    eos_lcd_write(1, 0x12);

    eos_lcd_write(0, 0xAE);
    eos_lcd_write(1, 0x09);

    eos_lcd_write(0, 0xAF);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0xC0);    // negative Gamma
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0xC1);
    eos_lcd_write(1, 0x09);

    eos_lcd_write(0, 0xC2);
    eos_lcd_write(1, 0x10);

    eos_lcd_write(0, 0xC3);
    eos_lcd_write(1, 0x0C);

    eos_lcd_write(0, 0xC4);
    eos_lcd_write(1, 0x05);

    eos_lcd_write(0, 0xC5);
    eos_lcd_write(1, 0x08);

    eos_lcd_write(0, 0xC6);
    eos_lcd_write(1, 0x06);

    eos_lcd_write(0, 0xC7);
    eos_lcd_write(1, 0x04);

    eos_lcd_write(0, 0xC8);
    eos_lcd_write(1, 0x08);

    eos_lcd_write(0, 0xC9);
    eos_lcd_write(1, 0x0C);

    eos_lcd_write(0, 0xCA);
    eos_lcd_write(1, 0x14);

    eos_lcd_write(0, 0xCB);
    eos_lcd_write(1, 0x08);

    eos_lcd_write(0, 0xCC);
    eos_lcd_write(1, 0x0F);

    eos_lcd_write(0, 0xCD);
    eos_lcd_write(1, 0x11);

    eos_lcd_write(0, 0xCE);
    eos_lcd_write(1, 0x09);

    eos_lcd_write(0, 0xCF);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0xFF);    // change to Page 6 CMD for GIP timing
    eos_lcd_write(1, 0xFF);
    eos_lcd_write(1, 0x98);
    eos_lcd_write(1, 0x06);
    eos_lcd_write(1, 0x04);
    eos_lcd_write(1, 0x06);

    eos_lcd_write(0, 0x00);
    eos_lcd_write(1, 0x20);

    eos_lcd_write(0, 0x01);
    eos_lcd_write(1, 0x0A);

    eos_lcd_write(0, 0x02);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x03);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x04);
    eos_lcd_write(1, 0x01);

    eos_lcd_write(0, 0x05);
    eos_lcd_write(1, 0x01);

    eos_lcd_write(0, 0x06);
    eos_lcd_write(1, 0x98);

    eos_lcd_write(0, 0x07);
    eos_lcd_write(1, 0x06);

    eos_lcd_write(0, 0x08);
    eos_lcd_write(1, 0x01);

    eos_lcd_write(0, 0x09);
    eos_lcd_write(1, 0x80);

    eos_lcd_write(0, 0x0A);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x0B);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x0C);
    eos_lcd_write(1, 0x01);

    eos_lcd_write(0, 0x0D);
    eos_lcd_write(1, 0x01);

    eos_lcd_write(0, 0x0E);
    eos_lcd_write(1, 0x05);

    eos_lcd_write(0, 0x0F);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x10);
    eos_lcd_write(1, 0xF0);

    eos_lcd_write(0, 0x11);
    eos_lcd_write(1, 0xF4);

    eos_lcd_write(0, 0x12);
    eos_lcd_write(1, 0x01);

    eos_lcd_write(0, 0x13);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x14);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x15);
    eos_lcd_write(1, 0xC0);

    eos_lcd_write(0, 0x16);
    eos_lcd_write(1, 0x08);

    eos_lcd_write(0, 0x17);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x18);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x19);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x1A);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x1B);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x1C);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x1D);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x20);
    eos_lcd_write(1, 0x01);

    eos_lcd_write(0, 0x21);
    eos_lcd_write(1, 0x23);

    eos_lcd_write(0, 0x22);
    eos_lcd_write(1, 0x45);

    eos_lcd_write(0, 0x23);
    eos_lcd_write(1, 0x67);

    eos_lcd_write(0, 0x24);
    eos_lcd_write(1, 0x01);

    eos_lcd_write(0, 0x25);
    eos_lcd_write(1, 0x23);

    eos_lcd_write(0, 0x26);
    eos_lcd_write(1, 0x45);

    eos_lcd_write(0, 0x27);
    eos_lcd_write(1, 0x67);

    eos_lcd_write(0, 0x30);
    eos_lcd_write(1, 0x11);

    eos_lcd_write(0, 0x31);
    eos_lcd_write(1, 0x11);

    eos_lcd_write(0, 0x32);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x33);
    eos_lcd_write(1, 0xEE);

    eos_lcd_write(0, 0x34);
    eos_lcd_write(1, 0xFF);

    eos_lcd_write(0, 0x35);
    eos_lcd_write(1, 0xBB);

    eos_lcd_write(0, 0x36);
    eos_lcd_write(1, 0xAA);

    eos_lcd_write(0, 0x37);
    eos_lcd_write(1, 0xDD);

    eos_lcd_write(0, 0x38);
    eos_lcd_write(1, 0xCC);

    eos_lcd_write(0, 0x39);
    eos_lcd_write(1, 0x66);

    eos_lcd_write(0, 0x3A);
    eos_lcd_write(1, 0x77);

    eos_lcd_write(0, 0x3B);
    eos_lcd_write(1, 0x22);

    eos_lcd_write(0, 0x3C);
    eos_lcd_write(1, 0x22);

    eos_lcd_write(0, 0x3D);
    eos_lcd_write(1, 0x22);

    eos_lcd_write(0, 0x3E);
    eos_lcd_write(1, 0x22);

    eos_lcd_write(0, 0x3F);
    eos_lcd_write(1, 0x22);

    eos_lcd_write(0, 0x40);
    eos_lcd_write(1, 0x22);

    eos_lcd_write(0, 0xFF);    // change to Page 7 CMD for GIP timing
    eos_lcd_write(1, 0xFF);
    eos_lcd_write(1, 0x98);
    eos_lcd_write(1, 0x06);
    eos_lcd_write(1, 0x04);
    eos_lcd_write(1, 0x07);

    eos_lcd_write(0, 0x17);
    eos_lcd_write(1, 0x22);

    eos_lcd_write(0, 0x02);
    eos_lcd_write(1, 0x77);

    eos_lcd_write(0, 0x26);
    eos_lcd_write(1, 0xB2);

    eos_lcd_write(0, 0xFF);    // change to Page 0 CMD for normal command
    eos_lcd_write(1, 0xFF);
    eos_lcd_write(1, 0x98);
    eos_lcd_write(1, 0x06);
    eos_lcd_write(1, 0x04);
    eos_lcd_write(1, 0x00);

    eos_lcd_write(0, 0x3A);
    eos_lcd_write(1, 0x70);    // 24BIT

    eos_lcd_write(0, 0x11);
    eos_time_sleep(120);
    eos_lcd_write(0, 0x29);
    eos_time_sleep(25);

    eos_lcd_cs_clear();
    eos_lcd_deselect();

    return EOS_OK;
}

int eos_lcd_init(uint8_t wakeup_cause) {
    eos_spi_select(EOS_SPI_DEV_EVE);
    eve_gpio_set(EVE_GPIO_LCD_EN, 1);
    eos_spi_deselect();
    eos_time_sleep(200);

    return _init();
}

int eos_lcd_sleep(void) {
    int rv;

    rv = eos_lcd_select();
    if (rv) return rv;

    eos_lcd_cs_set();

    eos_lcd_write(0, 0x28);
    eos_time_sleep(10);
    eos_lcd_write(0, 0x10);

    eos_lcd_cs_clear();
    eos_lcd_deselect();

    eos_spi_select(EOS_SPI_DEV_EVE);
    eve_gpio_set(EVE_GPIO_LCD_EN, 0);
    eos_spi_deselect();

    return EOS_OK;
}

int eos_lcd_wake(void) {
    int rv;

    eos_spi_select(EOS_SPI_DEV_EVE);
    eve_gpio_set(EVE_GPIO_LCD_EN, 1);
    eos_spi_deselect();
    eos_time_sleep(200);

    rv = eos_lcd_select();
    if (rv) return rv;

    eos_lcd_cs_set();

    eos_lcd_write(0, 0x11);
    eos_time_sleep(120);
    eos_lcd_write(0, 0x29);

    eos_lcd_cs_clear();
    eos_lcd_deselect();

    return EOS_OK;
}
