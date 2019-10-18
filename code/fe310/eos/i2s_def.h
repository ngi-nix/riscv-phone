#define I2S_ETYPE_MIC           1
#define I2S_ETYPE_SPK           2

#define I2S_PIN_CK              1       /* PWM 0.1 */
#define I2S_PIN_CK_SW           21      /* PWM 1.2 */
#define I2S_PIN_CK_SR           18
#define I2S_PIN_WS_MIC          19      /* PWM 1.1 */
#define I2S_PIN_WS_SPK          11      /* PWM 2.1 */
#define I2S_PIN_SD_IN           13
#define I2S_PIN_SD_OUT          12

#define I2S_IRQ_WS_ID           (INT_PWM2_BASE + 0)
#define I2S_IRQ_SD_ID           (INT_PWM2_BASE + 2)

#define I2S_IDLE_CYCLES         8

#define I2S_PWM_SCALE_CK        2
#define I2S_PWM_SCALE_CK_MASK   0x0003

/* asm */
#define I2S_ABUF_OFF_IDXR       0
#define I2S_ABUF_OFF_IDXW       2
#define I2S_ABUF_OFF_SIZE       4
#define I2S_ABUF_OFF_ARRAY      8
