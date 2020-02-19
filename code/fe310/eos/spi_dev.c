#include <stdlib.h>
#include <stdint.h>

#include "encoding.h"
#include "platform.h"

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

void eos_spi_dev_start(unsigned char dev) {
    eos_net_stop();
    switch (dev) {
        case EOS_DEV_DISP:
            eos_spi_start(dev, SPI_DIV_DISP, SPI_CSID_DISP, 0);
            break;
        case EOS_DEV_CARD:
            eos_spi_start(dev, SPI_DIV_CARD, SPI_CSID_CARD, 0);
            break;
        case EOS_DEV_CAM:
            eos_spi_start(dev, SPI_DIV_CAM, SPI_CSID_NONE, SPI_CSPIN_CAM);
            break;
    }
}

void eos_spi_dev_stop(void) {
    eos_spi_stop();
    eos_net_start();
}

void eos_spi_dev_init(void) {
    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << SPI_CSPIN_CAM);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << SPI_CSPIN_CAM);
    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << SPI_CSPIN_CAM);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << SPI_CSPIN_CAM);
    GPIO_REG(GPIO_OUTPUT_VAL)   |=  (1 << SPI_CSPIN_CAM);
}

