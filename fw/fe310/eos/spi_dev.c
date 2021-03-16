#include <stdlib.h>
#include <stdint.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"
#include "spi.h"
#include "net.h"

#include "spi_def.h"
#include "spi_dev.h"


#define SPI_DIV_DISP        16
#define SPI_DIV_CARD        16
#define SPI_DIV_CAM         16

#define SPI_CSID_DISP       2
#define SPI_CSID_CARD       0

#define SPI_CSPIN_CAM       23

static uint16_t spi_dev_div[EOS_DEV_MAX_DEV];

int eos_spi_dev_select(unsigned char dev) {
    uint8_t spi_dev = eos_spi_dev();
    int rv = EOS_ERR;

    if (spi_dev) {
        int rv;

        rv = eos_spi_stop();
        if (rv) return rv;
    } else {
        eos_net_stop();
    }
    switch (dev) {
        case EOS_DEV_DISP:
            rv = eos_spi_start(dev, spi_dev_div[dev - 1], SPI_CSID_DISP, 0);
            break;
        case EOS_DEV_CARD:
            rv = eos_spi_start(dev, spi_dev_div[dev - 1], SPI_CSID_CARD, 0);
            break;
        case EOS_DEV_CAM:
            rv = eos_spi_start(dev, spi_dev_div[dev - 1], SPI_CSID_NONE, SPI_CSPIN_CAM);
            break;
    }

    return rv;
}

int eos_spi_dev_deselect(void) {
    int rv;

    rv = eos_spi_stop();
    if (rv) return rv;
    eos_net_start();

    return EOS_OK;
}

void eos_spi_dev_init(void) {
    spi_dev_div[EOS_DEV_DISP - 1] = SPI_DIV_DISP;
    spi_dev_div[EOS_DEV_CARD - 1] = SPI_DIV_CARD;
    spi_dev_div[EOS_DEV_CAM  - 1] = SPI_DIV_CAM;

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << SPI_CSPIN_CAM);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << SPI_CSPIN_CAM);
    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << SPI_CSPIN_CAM);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << SPI_CSPIN_CAM);
    GPIO_REG(GPIO_OUTPUT_VAL)   |=  (1 << SPI_CSPIN_CAM);
}

void eos_spi_dev_set_div(unsigned char dev, uint16_t div) {
    spi_dev_div[dev-1] = div;
}
