#include <stdint.h>
#include "event.h"
#include "spi_dev.h"

#define EOS_SPI_MAX_DEV     EOS_DEV_MAX_DEV

#define EOS_SPI_FLAG_TX     0x01
#define EOS_SPI_FLAG_MORE   0x02
#define EOS_SPI_FLAG_BSWAP  0x04

void eos_spi_init(void);
int eos_spi_start(unsigned char dev, uint32_t div, uint32_t csid, uint8_t pin);
int eos_spi_stop(void);
uint8_t eos_spi_dev(void);
void eos_spi_lock(void);
void eos_spi_unlock(void);
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
