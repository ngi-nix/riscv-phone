#include <unistd.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"
#include "interrupt.h"
#include "event.h"

#include "i2s.h"
#include "i2s_def.h"

#define I2S_PWM_REG_CK  PWM1_REG
#define I2S_PWM_REG_WS  PWM2_REG

#define EOS_ABUF_IDX_MASK(IDX, SIZE)  ((IDX) & ((SIZE) - 1))

EOSABuf _eos_i2s_mic_buf;
EOSABuf _eos_i2s_spk_buf;
uint32_t _eos_i2s_ck_period = 0;
uint32_t _eos_i2s_mic_wm = 0;
uint32_t _eos_i2s_spk_wm = 0;
uint32_t _eos_i2s_mic_volume = 3;
uint32_t _eos_i2s_spk_volume = 3;
uint32_t _eos_i2s_mic_rd = 1;
uint32_t _eos_i2s_spk_wr = 1;
static eos_evt_fptr_t evt_handler[I2S_MAX_HANDLER];

static void _abuf_init(EOSABuf *buf, uint8_t *array, uint16_t size) {
    buf->idx_r = 0;
    buf->idx_w = 0;
    buf->size = size;
    buf->array = array;
}

static int _abuf_push(EOSABuf *buf, uint8_t sample) {
    if (buf->idx_w - buf->idx_r == buf->size) return EOS_ERR_Q_FULL;

    buf->array[EOS_ABUF_IDX_MASK(buf->idx_w, buf->size)] = sample;
    buf->idx_w++;
    return EOS_OK;
}

static int _abuf_pop(EOSABuf *buf, uint8_t *sample) {
    if (buf->idx_r == buf->idx_w) {
        return EOS_ERR_Q_EMPTY;
    } else {
        *sample = buf->array[EOS_ABUF_IDX_MASK(buf->idx_r, buf->size)];
        buf->idx_r++;
        return EOS_OK;
    }
}

static uint16_t _abuf_read(EOSABuf *buf, uint8_t *sample, uint16_t ssize) {
    uint16_t i;

    for (i=0; i<ssize; i++) {
        if (buf->idx_r == buf->idx_w) break;
        sample[i] = buf->array[EOS_ABUF_IDX_MASK(buf->idx_r, buf->size)];
        buf->idx_r++;
    }
    return i;
}

static uint16_t _abuf_write(EOSABuf *buf, uint8_t *sample, uint16_t ssize) {
    uint16_t i;

    for (i=0; i<ssize; i++) {
        if (buf->idx_w - buf->idx_r == buf->size) break;
        buf->array[EOS_ABUF_IDX_MASK(buf->idx_w, buf->size)] = sample[i];
        buf->idx_w++;
    }
    return i;
}

static uint16_t _abuf_len(EOSABuf *buf) {
    return buf->idx_w - buf->idx_r;
}

static void audio_handler(unsigned char cmd, unsigned char *buffer, uint16_t len) {
    if ((cmd & ~EOS_EVT_MASK) < I2S_MAX_HANDLER) {
        evt_handler[cmd & ~EOS_EVT_MASK](cmd, buffer, len);
    } else {
        eos_evtq_bad_handler(cmd, buffer, len);
    }
}

void eos_i2s_init(void) {
    int i;
    unsigned long f = get_cpu_freq();

    _eos_i2s_ck_period = 512;
    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_LR);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_LR);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << I2S_PIN_LR);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_CK);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_CK);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << I2S_PIN_CK);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_WS);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_WS);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << I2S_PIN_WS);

    GPIO_REG(GPIO_INPUT_EN)     |=  (1 << I2S_PIN_SD);
    GPIO_REG(GPIO_OUTPUT_EN)    &= ~(1 << I2S_PIN_SD);
    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << I2S_PIN_SD);

    GPIO_REG(GPIO_OUTPUT_VAL)   &= ~(1 << I2S_PIN_LR);

    I2S_PWM_REG_CK(PWM_CFG)     = 0;
    I2S_PWM_REG_CK(PWM_COUNT)   = 0;
    I2S_PWM_REG_CK(PWM_CMP0)    = _eos_i2s_ck_period - 1;
    I2S_PWM_REG_CK(PWM_CMP1)    = I2S_PWM_REG_CK(PWM_CMP0) / 2;
    I2S_PWM_REG_CK(PWM_CMP2)    = I2S_PWM_REG_CK(PWM_CMP0) / 8;

    I2S_PWM_REG_WS(PWM_CFG)     = 0;
    I2S_PWM_REG_WS(PWM_COUNT)   = 0;
    I2S_PWM_REG_WS(PWM_CMP0)    = _eos_i2s_ck_period * 64 - 1;
    I2S_PWM_REG_WS(PWM_CMP1)    = I2S_PWM_REG_WS(PWM_CMP0) / 2;
    I2S_PWM_REG_WS(PWM_CMP2)    = I2S_PWM_REG_WS(PWM_CMP0) - _eos_i2s_ck_period;

    eos_intr_set(I2S_IRQ_SD_ID, I2S_IRQ_SD_PRIORITY, NULL);
    eos_intr_set(I2S_IRQ_CK_ID, I2S_IRQ_CK_PRIORITY, NULL);
    eos_intr_set(I2S_IRQ_WS_ID, I2S_IRQ_WS_PRIORITY, NULL);
    eos_intr_set(I2S_IRQ_CI_ID, I2S_IRQ_CI_PRIORITY, NULL);

    for (i=0; i<I2S_MAX_HANDLER; i++) {
        evt_handler[i] = eos_evtq_bad_handler;
    }
    eos_evtq_set_handler(EOS_EVT_AUDIO, audio_handler, EOS_EVT_FLAG_WRAP);
}

void eos_i2s_init_mic(uint8_t *mic_arr, uint16_t mic_arr_size, uint16_t mic_wm, eos_evt_fptr_t mic_wm_handler) {
    _abuf_init(&_eos_i2s_mic_buf, mic_arr, mic_arr_size);
    _eos_i2s_mic_wm = mic_wm;
    evt_handler[I2S_EVT_MIC] = mic_wm_handler;
}

void eos_i2s_init_spk(uint8_t *spk_arr, uint16_t spk_arr_size, uint16_t spk_wm, eos_evt_fptr_t spk_wm_handler) {
    _abuf_init(&_eos_i2s_spk_buf, spk_arr, spk_arr_size);
    _eos_i2s_spk_wm = spk_wm;
    evt_handler[I2S_EVT_SPK] = spk_wm_handler;
}

void eos_i2s_start(void) {
    I2S_PWM_REG_CK(PWM_COUNT)   = 0;
    I2S_PWM_REG_WS(PWM_COUNT)   = 0;

    I2S_PWM_REG_CK(PWM_CFG)     = PWM_CFG_ENALWAYS | PWM_CFG_ZEROCMP;
    I2S_PWM_REG_WS(PWM_CFG)     = PWM_CFG_ENALWAYS | PWM_CFG_ZEROCMP;

    GPIO_REG(GPIO_IOF_SEL)      |=  (1 << I2S_PIN_CK);
    GPIO_REG(GPIO_IOF_EN)       |=  (1 << I2S_PIN_CK);
    
    GPIO_REG(GPIO_IOF_SEL)      |=  (1 << I2S_PIN_WS);
    GPIO_REG(GPIO_IOF_EN)       |=  (1 << I2S_PIN_WS);
}

void eos_i2s_stop(void) {
    I2S_PWM_REG_CK(PWM_CFG)     = 0;
    I2S_PWM_REG_WS(PWM_CFG)     = 0;
    I2S_PWM_REG_CK(PWM_COUNT)   = 0;
    I2S_PWM_REG_WS(PWM_COUNT)   = 0;

    GPIO_REG(GPIO_IOF_EN)       &= ~(1 << I2S_PIN_CK);
    GPIO_REG(GPIO_IOF_SEL)      &= ~(1 << I2S_PIN_CK);
    
    GPIO_REG(GPIO_IOF_EN)       &= ~(1 << I2S_PIN_WS);
    GPIO_REG(GPIO_IOF_SEL)      &= ~(1 << I2S_PIN_WS);
    
    GPIO_REG(GPIO_OUTPUT_VAL)   &= ~((1 << I2S_PIN_CK) | (1 << I2S_PIN_WS));
}

uint16_t eos_i2s_mic_len(void) {
    uint16_t ret = _abuf_len(&_eos_i2s_mic_buf);
    set_csr(mstatus, MSTATUS_MIE);
    return ret;
}

uint16_t eos_i2s_mic_read(uint8_t *sample, uint16_t ssize) {
    int i;
    uint16_t _ssize = 0;
    
    for (i=0; i<ssize/I2S_ABUF_SIZE_CHUNK && _ssize == i*I2S_ABUF_SIZE_CHUNK; i++) {
        clear_csr(mstatus, MSTATUS_MIE);
        _ssize += _abuf_read(&_eos_i2s_mic_buf, sample+i*I2S_ABUF_SIZE_CHUNK, I2S_ABUF_SIZE_CHUNK);
        if ((ssize == _ssize) || (_ssize != (i+1)*I2S_ABUF_SIZE_CHUNK)) _eos_i2s_mic_rd = 1;
        set_csr(mstatus, MSTATUS_MIE);
    }
    if ((ssize > _ssize) && (_ssize == i*I2S_ABUF_SIZE_CHUNK)) {
        clear_csr(mstatus, MSTATUS_MIE);
        _ssize += _abuf_read(&_eos_i2s_mic_buf, sample+i*I2S_ABUF_SIZE_CHUNK, ssize - _ssize);
        _eos_i2s_mic_rd = 1;
        set_csr(mstatus, MSTATUS_MIE);
    }
    return _ssize;
}

int eos_i2s_mic_pop(uint8_t *sample) {
    clear_csr(mstatus, MSTATUS_MIE);
    int ret = _abuf_pop(&_eos_i2s_mic_buf, sample);
    _eos_i2s_mic_rd = 1;
    set_csr(mstatus, MSTATUS_MIE);
    return ret;
}

uint16_t eos_i2s_spk_len(void) {
    clear_csr(mstatus, MSTATUS_MIE);
    uint16_t ret = _abuf_len(&_eos_i2s_spk_buf);
    set_csr(mstatus, MSTATUS_MIE);
    return ret;
}

uint16_t eos_i2s_spk_write(uint8_t *sample, uint16_t ssize) {
    int i;
    uint16_t _ssize = 0;
    for (i=0; i<ssize/I2S_ABUF_SIZE_CHUNK && _ssize == i*I2S_ABUF_SIZE_CHUNK; i++) {
        clear_csr(mstatus, MSTATUS_MIE);
        _ssize += _abuf_write(&_eos_i2s_spk_buf, sample+i*I2S_ABUF_SIZE_CHUNK, I2S_ABUF_SIZE_CHUNK);
        if ((ssize == _ssize) || (_ssize != (i+1)*I2S_ABUF_SIZE_CHUNK)) _eos_i2s_spk_wr = 1;
        set_csr(mstatus, MSTATUS_MIE);
    }
    if ((ssize > _ssize) && (_ssize == i*I2S_ABUF_SIZE_CHUNK)) {
        clear_csr(mstatus, MSTATUS_MIE);
        _ssize += _abuf_write(&_eos_i2s_spk_buf, sample+i*I2S_ABUF_SIZE_CHUNK, ssize - _ssize);
        _eos_i2s_spk_wr = 1;
        set_csr(mstatus, MSTATUS_MIE);
    }
    return _ssize;

}

int eos_i2s_spk_push(uint8_t sample) {
    clear_csr(mstatus, MSTATUS_MIE);
    int ret = _abuf_push(&_eos_i2s_spk_buf, sample);
    _eos_i2s_spk_wr = 1;
    set_csr(mstatus, MSTATUS_MIE);
    return ret;
}
