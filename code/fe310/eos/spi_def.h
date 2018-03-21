#define SPI_SIZE_BUF            1500
#define SPI_SIZE_BUFQ           2

#define SPI_MODE0               0x00
#define SPI_MODE1               0x01
#define SPI_MODE2               0x02
#define SPI_MODE3               0x03

#define SPI_SIZE_CHUNK          4
#define SPI_SIZE_TXWM           2
#define SPI_SIZE_RXWM           3

#define SPI_PIN_RTS             0   // pin 8
#define SPI_PIN_CTS             23  // pin 7

#define SPI_FLAG_RDY            0x01
#define SPI_FLAG_RST            0x02
#define SPI_FLAG_RTS            0x04
#define SPI_FLAG_CTS            0x08
#define SPI_FLAG_INIT           0x10
#define SPI_FLAG_ONEW           0x20

/* asm */
#define SPI_BUFQ_OFF_IDXR       0
#define SPI_BUFQ_OFF_IDXW       1
#define SPI_BUFQ_OFF_ARRAY      4
