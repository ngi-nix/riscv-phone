#include <stdint.h>

#include "encoding.h"
#include "platform.h"

#define SPI_MODE0               0x00
#define SPI_MODE1               0x01
#define SPI_MODE2               0x02
#define SPI_MODE3               0x03

#define SPI_SIZE_BUF            1500
#define SPI_SIZE_CHUNK          4
#define SPI_SIZE_BUFQ           4
#define SPI_GPIO_CTS_OFFSET     PIN_2_OFFSET
#define SPI_GPIO_RTS_OFFSET     PIN_3_OFFSET
#define SPI_IOF_MASK            (((uint32_t)1 << IOF_SPI1_SCK) | ((uint32_t)1 << IOF_SPI1_MOSI) | ((uint32_t)1 << IOF_SPI1_MISO))

typedef struct SPIBuffer {
    unsigned char cmd;
    unsigned char *buffer;
    uint16_t len;
    uint16_t len_rx;
    uint16_t idx_tx;
    uint16_t idx_rx;
} SPIBuffer;

typedef struct SPIBufQ {
    uint8_t idx_r;
    uint8_t idx_w;
    unsigned char *array[SPI_SIZE_BUFQ];
} SPIBufQ;

