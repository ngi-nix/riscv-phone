#include <stdint.h>
#include <stdlib.h>

#include "timer.h"
#include "spi.h"
#include "spi_dev.h"

#define EVE_ETYPE_INTR      1

#define EVE_RTC_FREQ        EOS_TIMER_RTC_FREQ

#define EVE_SPI_FLAG_BSWAP  EOS_SPI_FLAG_BSWAP
#define EVE_SPI_FLAG_TX     EOS_SPI_FLAG_TX

void *eve_malloc(size_t);
void eve_free(void *);

//#define eve_malloc          malloc
//#define eve_free            free

void eve_spi_start(void);
void eve_spi_stop(void);

#define eve_spi_cs_set      eos_spi_cs_set
#define eve_spi_cs_clear    eos_spi_cs_clear
#define eve_spi_flush       eos_spi_flush
#define eve_spi_xchg8       eos_spi_xchg8
#define eve_spi_xchg16      eos_spi_xchg16
#define eve_spi_xchg24      eos_spi_xchg24
#define eve_spi_xchg32      eos_spi_xchg32
#define eve_spi_lock        eos_spi_lock
#define eve_spi_unlock      eos_spi_unlock

void eve_time_sleep(uint32_t ms);
uint64_t eve_time_get_tick(void);
void eve_timer_set(uint32_t ms);
void eve_timer_clear(void);
