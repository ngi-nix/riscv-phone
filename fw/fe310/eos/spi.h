#include <stdint.h>
#include "event.h"

#define EOS_SPI_DEV_NET         0
#define EOS_SPI_DEV_EVE         1
#define EOS_SPI_DEV_SDC         2
#define EOS_SPI_DEV_CAM         3

#define EOS_SPI_MAX_DEV         4

#define EOS_SPI_FLAG_TX         0x01
#define EOS_SPI_FLAG_MORE       0x02
#define EOS_SPI_FLAG_BSWAP      0x04

void eos_spi_init(void);
int eos_spi_select(unsigned char dev);
int eos_spi_deselect(void);

uint8_t eos_spi_dev(void);
uint16_t eos_spi_div(unsigned char dev);
uint16_t eos_spi_csid(unsigned char dev);
uint16_t eos_spi_cspin(unsigned char dev);

void eos_spi_lock(void);
void eos_spi_unlock(void);
void eos_spi_set_div(unsigned char dev, uint16_t div);
void eos_spi_set_handler(unsigned char dev, eos_evt_handler_t handler);

void _eos_spi_xchg_init(unsigned char *buffer, uint16_t len, uint8_t flags);
void eos_spi_xchg(unsigned char *buffer, uint16_t len, uint8_t flags);
void eos_spi_handle_xchg(void);

void eos_spi_cs_set(void);
void eos_spi_cs_clear(void);
uint8_t eos_spi_xchg8(uint8_t data, uint8_t flags);
uint16_t eos_spi_xchg16(uint16_t data, uint8_t flags);
uint32_t eos_spi_xchg24(uint32_t data, uint8_t flags);
uint32_t eos_spi_xchg32(uint32_t data, uint8_t flags);
void eos_spi_flush(void);
