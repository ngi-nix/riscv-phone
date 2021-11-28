#include <stdint.h>

#define EOS_SDC_TYPE_MMC            0x01

#define EOS_SDC_TYPE_SDC1           0x04
#define EOS_SDC_TYPE_SDC2           0x08
#define EOS_SDC_TYPE_SDC            0x0c
#define EOS_SDC_TYPE_MASK           0x0f

#define EOS_SDC_CAP_BLK             0x10
#define EOS_SDC_CAP_ERASE_EN        0x20
#define EOS_SDC_CAP_MASK            0xf0

int eos_sdc_init(uint8_t wakeup_cause);
uint8_t eos_sdc_type(void);
uint8_t eos_sdc_cap(void);
int eos_sdc_get_sect_count(uint32_t timeout, uint32_t *sectors);
int eos_sdc_get_blk_size(uint32_t timeout, uint32_t *size);
int eos_sdc_sync(uint32_t timeout);
int eos_sdc_erase(uint32_t blk_start, uint32_t blk_end, uint32_t timeout);
int eos_sdc_sect_read(uint32_t sect, unsigned int count, uint8_t *buffer);
int eos_sdc_sect_write(uint32_t sect, unsigned int count, uint8_t *buffer);
