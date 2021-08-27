#include <stdint.h>

#define SPI_GPIO_BANK       "gpiochip2"
#define SPI_GPIO_CTS        3
#define SPI_GPIO_RTS        4

#define SPI_MTU             1500
#define SPI_SIZE_BUF        (SPI_MTU + 8)
#define SPI_SIZE_HDR        3

#define SPI_SIZE_BUFQ       64
#define SPI_SIZE_MSGQ       256

#define SPI_MTYPE_TUN       1

#define SPI_OK              0
#define SPI_ERR	            -1
#define SPI_ERR_OPEN        -10
#define SPI_ERR_MSG         -11

unsigned char *spi_alloc(void);
void spi_free(unsigned char *buffer);
int spi_xchg(unsigned char mtype, unsigned char *buffer, uint16_t len);
int spi_init(char *fname);