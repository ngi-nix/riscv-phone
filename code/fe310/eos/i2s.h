#include <stdint.h>

typedef struct EOSABuf {
    uint16_t idx_r;
    uint16_t idx_w;
    uint16_t size;
    uint8_t *array;
} EOSABuf;

void eos_i2s_init(void);
void eos_i2s_init_mic(uint8_t *mic_arr, uint16_t mic_arr_size, eos_evt_fptr_t mic_wm_handler, uint16_t mic_wm);
void eos_i2s_init_spk(uint8_t *spk_arr, uint16_t spk_arr_size, eos_evt_fptr_t spk_wm_handler, uint16_t spk_wm);
void eos_i2s_start(void);
void eos_i2s_stop(void);
uint16_t eos_i2s_mic_len(void);
uint16_t eos_i2s_mic_read(uint8_t *sample, uint16_t ssize);
int eos_i2s_mic_pop(uint8_t *sample);
uint16_t eos_i2s_spk_len(void);
uint16_t eos_i2s_spk_write(uint8_t *sample, uint16_t ssize);
int eos_i2s_spk_push(uint8_t sample);
