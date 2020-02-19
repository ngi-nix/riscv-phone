#define EOS_DEV_DISP        1
#define EOS_DEV_CARD        2
#define EOS_DEV_CAM         3

#define EOS_DEV_MAX_DEV     3

void eos_spi_dev_init(void);
void eos_spi_dev_start(unsigned char dev);
void eos_spi_dev_stop(void);
