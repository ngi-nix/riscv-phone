#include <stdint.h>

#define OV2640_ADDR     0x30

int eos_ov2640_init(void);
int eos_ov2640_sleep(int enable);
int eos_ov2640_set_pixfmt(pixformat_t fmt);
int eos_ov2640_set_framesize(framesize_t framesize);
