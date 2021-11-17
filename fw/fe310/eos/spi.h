#include <stdint.h>
#include "event.h"

#define EOS_SPI_FLAG_TX         0x01
#define EOS_SPI_FLAG_MORE       0x02
#define EOS_SPI_FLAG_BSWAP      0x04

#define EOS_SPI_EVT_SDC         1
#define EOS_SPI_EVT_CAM         2

#define EOS_SPI_MAX_EVT         2

void eos_spi_init(uint8_t wakeup_cause);
void eos_spi_start(uint16_t div, uint8_t csid, uint8_t cspin, unsigned char evt);
void eos_spi_stop(void);
void eos_spi_set_handler(unsigned char evt, eos_evt_handler_t handler);

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
