#include <stdlib.h>
#include <stdint.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"

#include "board.h"

#include "i2s.h"
#include "net.h"
#include "spi_dev.h"
#include "display.h"

#define BIT_PUT(b, pin)     if (b) GPIO_REG(GPIO_OUTPUT_VAL) |= (1 << (pin)); else GPIO_REG(GPIO_OUTPUT_VAL) &= ~(1 << (pin));
#define BIT_GET(pin)        ((GPIO_REG(GPIO_INPUT_VAL) & (1 << (pin))) >> (pin))

int eos_disp_select(void) {
    if (eos_i2s_running()) return EOS_ERR_BUSY;
    if (eos_spi_dev() != EOS_SPI_DEV_NET) return EOS_ERR_BUSY;

    eos_net_stop();
    GPIO_REG(GPIO_IOF_EN) &= ~SPI_IOF_MASK;
}

void eos_disp_deselect(void) {
    GPIO_REG(GPIO_OUTPUT_VAL) |= (1 << IOF_SPI1_MOSI);
    GPIO_REG(GPIO_IOF_EN) |= SPI_IOF_MASK;
    eos_net_start(0);
}

void eos_disp_cs_set(void) {
    GPIO_REG(GPIO_OUTPUT_VAL) |= (1 << DISP_PIN_CS);
}

void eos_disp_cs_clear(void) {
    GPIO_REG(GPIO_OUTPUT_VAL) &= ~(1 << DISP_PIN_CS);
}

void eos_disp_write(uint8_t dc, uint8_t data) {
    int i;

    BIT_PUT(dc, IOF_SPI1_MOSI);
    // sleep
    BIT_PUT(1, IOF_SPI1_SCK);
    for (i=0; i<8; i++) {
        // sleep
        BIT_PUT(0, IOF_SPI1_SCK);
        BIT_PUT(data & 0x80, IOF_SPI1_MOSI);
        // sleep
        BIT_PUT(1, IOF_SPI1_SCK);
        data = data << 1;
    }
    // sleep
    BIT_PUT(0, IOF_SPI1_SCK);
}

void eos_disp_read(uint8_t *data) {
    int i;

    *data = 0;
    for (i=0; i<8; i++) {
        // sleep
        BIT_PUT(1, IOF_SPI1_SCK);
        *data = *data << 1;
        *data |= BIT_GET(IOF_SPI1_MISO);
        // sleep
        BIT_PUT(0, IOF_SPI1_SCK);
    }
}
