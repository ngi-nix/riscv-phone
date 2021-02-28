#define EOS_DEV_DISP        1
#define EOS_DEV_CARD        2
#define EOS_DEV_CAM         3

#define EOS_DEV_MAX_DEV     3

void eos_spi_dev_init(void);
int eos_spi_dev_select(unsigned char dev);
int eos_spi_dev_deselect(void);

void eos_spi_dev_set_div(unsigned char dev, uint16_t div);
