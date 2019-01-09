#define I2S_EVT_MIC             0x0
#define I2S_EVT_SPK             0x1
#define I2S_MAX_HANDLER         2

#define I2S_PIN_CK              1
#define I2S_PIN_CK_SW           12
#define I2S_PIN_CK_SR           18
#define I2S_PIN_WS_MIC          11
#define I2S_PIN_WS_SPK          19
#define I2S_PIN_SD_IN           13
#define I2S_PIN_SD_OUT          20

#define I2S_IRQ_WS_ID           (INT_PWM1_BASE + 0)
#define I2S_IRQ_SD_ID           (INT_PWM1_BASE + 2)

#define I2S_IRQ_WS_PRIORITY     6
#define I2S_IRQ_SD_PRIORITY     7

#define I2S_IDLE_CYCLES         8

#define I2S_PWM_SCALE_CK        2
#define I2S_PWM_SCALE_CK_MASK   0x0003
#define I2S_ABUF_SIZE_CHUNK     16

/* asm */
#define I2S_ABUF_OFF_IDXR       0
#define I2S_ABUF_OFF_IDXW       2
#define I2S_ABUF_OFF_SIZE       4
#define I2S_ABUF_OFF_ARRAY      8
