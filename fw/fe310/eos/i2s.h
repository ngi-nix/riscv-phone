#include <stdint.h>

#define EOS_I2S_FMT_ALAW        0
#define EOS_I2S_FMT_PCM16       1

/* should match i2s_def.h definitions */
#define EOS_I2S_ETYPE_MIC       1
#define EOS_I2S_ETYPE_SPK       2


typedef struct EOSABuf {
    uint16_t idx_r;
    uint16_t idx_w;
    uint16_t size;
    uint8_t *array;
} EOSABuf;

typedef void (*eos_i2s_handler_t) (unsigned char);

void eos_i2s_init(void);
void eos_i2s_start(uint32_t sample_rate, unsigned char fmt);
void eos_i2s_stop(void);
void eos_i2s_mic_init(uint8_t *mic_arr, uint16_t mic_arr_size);
void eos_i2s_mic_set_handler(eos_i2s_handler_t wm_handler);
void eos_i2s_mic_set_wm(uint16_t wm);
uint16_t eos_i2s_mic_len(void);
uint16_t eos_i2s_mic_read(uint8_t *sample, uint16_t ssize);
int eos_i2s_mic_pop8(uint8_t *sample);
int eos_i2s_mic_pop16(uint16_t *sample);
int eos_i2s_mic_vol_get(void);
void eos_i2s_mic_vol_set(int vol);
void eos_i2s_spk_init(uint8_t *mic_arr, uint16_t mic_arr_size);
void eos_i2s_spk_set_handler(eos_i2s_handler_t wm_handler);
void eos_i2s_spk_set_wm(uint16_t wm);
uint16_t eos_i2s_spk_len(void);
uint16_t eos_i2s_spk_write(uint8_t *sample, uint16_t ssize);
int eos_i2s_spk_push8(uint8_t sample);
int eos_i2s_spk_push16(uint16_t sample);
int eos_i2s_spk_vol_get(void);
void eos_i2s_spk_vol_set(int vol);