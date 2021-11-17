#include <stdint.h>

#define EOS_SPI_DEV_NET         0
#define EOS_SPI_DEV_EVE         1
#define EOS_SPI_DEV_SDC         2
#define EOS_SPI_DEV_CAM         3

#define EOS_SPI_MAX_DEV         4

void eos_spi_dev_init(uint8_t wakeup_cause);
int eos_spi_select(unsigned char dev);
int eos_spi_deselect(void);

uint8_t eos_spi_dev(void);
uint16_t eos_spi_div(unsigned char dev);
uint8_t eos_spi_csid(unsigned char dev);
uint8_t eos_spi_cspin(unsigned char dev);

void eos_spi_lock(void);
void eos_spi_unlock(void);
void eos_spi_set_div(unsigned char dev, uint16_t div);
