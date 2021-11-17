#include <stdlib.h>
#include <stdint.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"
#include "msgq.h"
#include "interrupt.h"
#include "event.h"

#include "board.h"

#include "net.h"
#include "spi.h"
#include "spi_priv.h"
#include "spi_cfg.h"
#include "spi_dev.h"

static uint8_t spi_dev;
static uint8_t spi_lock;
static uint16_t spi_div[EOS_SPI_MAX_DEV];

void eos_spi_dev_init(uint8_t wakeup_cause) {
    int i;

    for (i=0; i<EOS_SPI_MAX_DEV; i++) {
        spi_div[i] = spi_cfg[i].div;
        if (spi_cfg[i].csid == SPI_CSID_NONE) {
            GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << spi_cfg[i].cspin);
            GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << spi_cfg[i].cspin);
            GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << spi_cfg[i].cspin);
            GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << spi_cfg[i].cspin);
            GPIO_REG(GPIO_OUTPUT_VAL)   |=  (1 << spi_cfg[i].cspin);
        }
    }
}

int eos_spi_select(unsigned char dev) {
    if (dev == EOS_SPI_DEV_NET) return EOS_ERR;
    if (spi_lock) return EOS_ERR_BUSY;

    if (spi_dev == EOS_SPI_DEV_NET) {
        eos_net_stop();
    } else {
        eos_spi_stop();
    }

    spi_dev = dev;
    eos_spi_start(spi_div[dev], spi_cfg[dev].csid, spi_cfg[dev].cspin, spi_cfg[dev].evt);

    return EOS_OK;
}

int eos_spi_deselect(void) {
    if (spi_dev == EOS_SPI_DEV_NET) return EOS_ERR;
    if (spi_lock) return EOS_ERR_BUSY;

    eos_spi_stop();

    spi_dev = EOS_SPI_DEV_NET;
    eos_net_start(0);

    return EOS_OK;
}

uint8_t eos_spi_dev(void) {
    return spi_dev;
}

uint16_t eos_spi_div(unsigned char dev) {
    return spi_div[dev];
}

uint8_t eos_spi_csid(unsigned char dev) {
    return spi_cfg[dev].csid;
}

uint8_t eos_spi_cspin(unsigned char dev) {
    return spi_cfg[dev].cspin;
}

void eos_spi_lock(void) {
    spi_lock = 1;
}

void eos_spi_unlock(void) {
    spi_lock = 0;
}

void eos_spi_set_div(unsigned char dev, uint16_t div) {
    spi_div[dev] = div;
}
