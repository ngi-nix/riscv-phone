#define SPI_MODE0               0x00
#define SPI_MODE1               0x01
#define SPI_MODE2               0x02
#define SPI_MODE3               0x03

/* DO NOT TOUCH THEESE */
#define SPI_SIZE_CHUNK          4
#define SPI_SIZE_WM             2

#define SPI_FLAG_XCHG           0x10

#define SPI_CSID_NONE           1
#define SPI_CSPIN_NONE          0xff

#define SPI_IOF_MASK            (((uint32_t)1 << IOF_SPI1_SCK) | ((uint32_t)1 << IOF_SPI1_MOSI) | ((uint32_t)1 << IOF_SPI1_MISO)) | ((uint32_t)1 << IOF_SPI1_SS0) | ((uint32_t)1 << IOF_SPI1_SS2) | ((uint32_t)1 << IOF_SPI1_SS3)

typedef struct {
    uint8_t div;
    uint8_t csid;
    uint8_t cspin;
} SPIConfig;

static const SPIConfig spi_cfg[] = {
    {   // DEV_NET
        .div = SPI_DIV_NET,
        .csid = SPI_CSID_NET,
        .cspin = SPI_CSPIN_NET,
    },
    {   // DEV_EVE
        .div = SPI_DIV_EVE,
        .csid = SPI_CSID_EVE,
        .cspin = SPI_CSPIN_EVE,
    },
    {   // DEV_SDC
        .div = SPI_DIV_SDC,
        .csid = SPI_CSID_SDC,
        .cspin = SPI_CSPIN_SDC,
    },
    {   // DEV_CAM
        .div = SPI_DIV_CAM,
        .csid = SPI_CSID_CAM,
        .cspin = SPI_CSPIN_CAM,
    },
};