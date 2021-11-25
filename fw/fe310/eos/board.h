#define SPI_DIV_NET             16
#define SPI_DIV_EVE             4
#define SPI_DIV_SDC             1024
#define SPI_DIV_CAM             24

#define SPI_CSID_NET            0
#define SPI_CSID_EVE            3
#define SPI_CSID_SDC            SPI_CSID_NONE
#define SPI_CSID_CAM            2

#define SPI_IOF_MASK            ((1 << IOF_SPI1_SCK) | (1 << IOF_SPI1_MOSI) | (1 << IOF_SPI1_MISO) | (1 << IOF_SPI1_SS0) | (1 << IOF_SPI1_SS2) | (1 << IOF_SPI1_SS3))

#define SPI_CSPIN_NET           2
#define SPI_CSPIN_EVE           10
#define SPI_CSPIN_SDC           0
#define SPI_CSPIN_CAM           9

#define NET_PIN_RTS             20
#define NET_PIN_CTS             22

#define DISP_PIN_CS             11

#define EVE_PIN_INTR            23

#define I2S_PIN_CK              1       /* PWM 0.1 */
#define I2S_PIN_CK_SW           21      /* PWM 1.2 */
#define I2S_PIN_CK_SR           18
#define I2S_PIN_WS_MIC          19      /* PWM 1.1 */
#define I2S_PIN_WS_SPK          11      /* PWM 2.1 */
#define I2S_PIN_SD_IN           13
#define I2S_PIN_SD_OUT          12

#define I2S_IRQ_WS_ID           (INT_PWM2_BASE + 0)
#define I2S_IRQ_SD_ID           (INT_PWM2_BASE + 3)

#define I2S_CTRL_ADDR_CK        PWM0_CTRL_ADDR
#define I2S_CTRL_ADDR_WS_MIC    PWM1_CTRL_ADDR
#define I2S_CTRL_ADDR_WS_SPK    PWM2_CTRL_ADDR

#define I2S_IDLE_CYCLES         8

#define EVE_GPIO_DIR            0xf

#define EVE_GPIO_CAM            0
#define EVE_GPIO_DISP_EN        1
#define EVE_GPIO_GAIN_SEL       2
#define EVE_GPIO_HAPT           3
