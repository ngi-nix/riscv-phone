#include <stdint.h>

typedef struct {
    uint16_t div;
    uint8_t csid;
    uint8_t cspin;
    unsigned char evt;
} SPIConfig;

static const SPIConfig spi_cfg[] = {
    {   // DEV_NET
        .div = SPI_DIV_NET,
        .csid = SPI_CSID_NET,
        .cspin = SPI_CSPIN_NET,
        .evt = 0,   // Not SPI event
    },
    {   // DEV_EVE
        .div = SPI_DIV_EVE,
        .csid = SPI_CSID_EVE,
        .cspin = SPI_CSPIN_EVE,
        .evt = 0,
    },
    {   // DEV_SDC
        .div = SPI_DIV_SDC,
        .csid = SPI_CSID_SDC,
        .cspin = SPI_CSPIN_SDC,
        .evt = EOS_SPI_EVT_SDC,
    },
    {   // DEV_CAM
        .div = SPI_DIV_CAM,
        .csid = SPI_CSID_CAM,
        .cspin = SPI_CSPIN_CAM,
        .evt = EOS_SPI_EVT_CAM,
    },
};
