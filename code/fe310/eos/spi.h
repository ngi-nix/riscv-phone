#include <stdint.h>
#include "event.h"

#define EOS_SPI_DEV_NET     0
#define EOS_SPI_DEV_DISP    1
#define EOS_SPI_DEV_CARD    2
#define EOS_SPI_DEV_CAM     3

#define EOS_SPI_MAX_DEV     3

#define EOS_SPI_FLAG_TX     0x01
#define EOS_SPI_FLAG_MORE   0x02
#define EOS_SPI_FLAG_AUTOCS 0x04
#define EOS_SPI_FLAG_BSWAP  0x08

void eos_spi_init(void);
void eos_spi_dev_start(unsigned char dev);
void eos_spi_dev_stop(void);

void eos_spi_xchg(unsigned char *buffer, uint16_t len, uint8_t flags);
void eos_spi_xchg_handler(void);

void eos_spi_cs_set(void);
void eos_spi_cs_clear(void);
uint8_t eos_spi_xchg8(uint8_t data, uint8_t flags);
uint16_t eos_spi_xchg16(uint16_t data, uint8_t flags);
uint32_t eos_spi_xchg24(uint32_t data, uint8_t flags);
uint32_t eos_spi_xchg32(uint32_t data, uint8_t flags);

void eos_spi_set_handler(unsigned char dev, eos_evt_fptr_t handler);