#include <stdlib.h>
#include <stdint.h>

#include "encoding.h"
#include "platform.h"
#include "prci_driver.h"

#include "eos.h"
#include "interrupt.h"
#include "event.h"

#include "i2s.h"
#include "i2s_def.h"
#include "irq_def.h"

#define I2S_PWM_REG_CK      PWM0_REG
#define I2S_PWM_REG_WS_MIC  PWM1_REG
#define I2S_PWM_REG_WS_SPK  PWM2_REG

#define MIN(X, Y)           (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)           (((X) > (Y)) ? (X) : (Y))

#define EOS_ABUF_IDX_MASK(IDX, SIZE)  ((IDX) & ((SIZE) - 1))

EOSABuf _eos_i2s_mic_buf;
EOSABuf _eos_i2s_spk_buf;
uint32_t _eos_i2s_fmt = 0;
uint32_t _eos_i2s_mic_volume = 0;
uint32_t _eos_i2s_spk_volume = 0;
uint32_t _eos_i2s_mic_wm = 0;
uint32_t _eos_i2s_spk_wm = 0;
uint32_t _eos_i2s_mic_evt_enable = 0;
uint32_t _eos_i2s_spk_evt_enable = 0;

static eos_i2s_fptr_t spk_evt_handler = NULL;
static eos_i2s_fptr_t mic_evt_handler = NULL;

static void _abuf_init(EOSABuf *buf, uint8_t *array, uint16_t size) {
    buf->idx_r = 0;
    buf->idx_w = 0;
    buf->size = size;
    buf->array = array;
}

static int _abuf_push8(EOSABuf *buf, uint8_t sample) {
    if ((uint16_t)(buf->idx_w - buf->idx_r) == buf->size) return EOS_ERR_Q_FULL;

    buf->array[EOS_ABUF_IDX_MASK(buf->idx_w, buf->size)] = sample;
    buf->idx_w++;
    return EOS_OK;
}

static int _abuf_push16(EOSABuf *buf, uint16_t sample) {
    if ((uint16_t)(buf->idx_w - buf->idx_r) == buf->size) return EOS_ERR_Q_FULL;

    buf->array[EOS_ABUF_IDX_MASK(buf->idx_w, buf->size)] = sample >> 8;
    buf->array[EOS_ABUF_IDX_MASK(buf->idx_w + 1, buf->size)] = sample & 0xFF;
    buf->idx_w += 2;
    return EOS_OK;
}

static int _abuf_pop8(EOSABuf *buf, uint8_t *sample) {
    if (buf->idx_r == buf->idx_w) {
        return EOS_ERR_Q_EMPTY;
    } else {
        *sample = buf->array[EOS_ABUF_IDX_MASK(buf->idx_r, buf->size)];
        buf->idx_r++;
        return EOS_OK;
    }
}

static int _abuf_pop16(EOSABuf *buf, uint16_t *sample) {
    if (buf->idx_r == buf->idx_w) {
        return EOS_ERR_Q_EMPTY;
    } else {
        *sample = buf->array[EOS_ABUF_IDX_MASK(buf->idx_r, buf->size)] << 8;
        *sample |= buf->array[EOS_ABUF_IDX_MASK(buf->idx_r + 1, buf->size)];
        buf->idx_r += 2;
        return EOS_OK;
    }
}


static void _abuf_flush(EOSABuf *buf) {
    buf->idx_r = 0;
    buf->idx_w = 0;
}

static uint16_t _abuf_len(EOSABuf *buf) {
    return buf->idx_w - buf->idx_r;
}

static void i2s_handler_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    switch(type & ~EOS_EVT_MASK) {
        case I2S_ETYPE_MIC:
            if (mic_evt_handler) mic_evt_handler(type);
            clear_csr(mstatus, MSTATUS_MIE);
            _eos_i2s_mic_evt_enable = 1;
            set_csr(mstatus, MSTATUS_MIE);
            break;
        case I2S_ETYPE_SPK:
            if (spk_evt_handler) spk_evt_handler(type);
            clear_csr(mstatus, MSTATUS_MIE);
            _eos_i2s_spk_evt_enable = 1;
            set_csr(mstatus, MSTATUS_MIE);
            break;
        default:
            eos_evtq_bad_handler(type, buffer, len);
            break;
    }
}

extern void _eos_i2s_start_pwm(void);

void eos_i2s_init(void) {
    eos_evtq_set_handler(EOS_EVT_AUDIO, i2s_handler_evt);
    eos_evtq_set_flags(EOS_EVT_AUDIO | I2S_ETYPE_MIC, EOS_EVT_FLAG_NET_BUF_ACQ);
}

void eos_i2s_start(uint32_t sample_rate, unsigned char fmt) {
    // Ignore the first run (for icache reasons)
    uint32_t cpu_freq = PRCI_measure_mcycle_freq(3000, RTC_FREQ);
    cpu_freq = PRCI_measure_mcycle_freq(3000, RTC_FREQ);
    uint32_t ck_period = (cpu_freq / (sample_rate * 64)) & ~I2S_PWM_SCALE_CK_MASK;;

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_CK);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_CK);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << I2S_PIN_CK);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_CK_SW);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_CK_SW);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << I2S_PIN_CK_SW);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_CK_SR);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_CK_SR);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << I2S_PIN_CK_SR);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_WS_MIC);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_WS_MIC);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << I2S_PIN_WS_MIC);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_WS_SPK);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_WS_SPK);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << I2S_PIN_WS_SPK);

    GPIO_REG(GPIO_INPUT_EN)     |=  (1 << I2S_PIN_SD_IN);
    GPIO_REG(GPIO_OUTPUT_EN)    &= ~(1 << I2S_PIN_SD_IN);
    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << I2S_PIN_SD_IN);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_SD_OUT);
    GPIO_REG(GPIO_OUTPUT_EN)    &= ~(1 << I2S_PIN_SD_OUT);
    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << I2S_PIN_SD_OUT);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << I2S_PIN_SD_OUT);

    I2S_PWM_REG_CK(PWM_CMP0)    = ck_period >> I2S_PWM_SCALE_CK;
    I2S_PWM_REG_CK(PWM_CMP1)    = I2S_PWM_REG_CK(PWM_CMP0) / 2;
    I2S_PWM_REG_CK(PWM_CMP2)    = 0;
    I2S_PWM_REG_CK(PWM_CMP3)    = 0;

    I2S_PWM_REG_WS_MIC(PWM_CMP0)    = (ck_period + 1) * 64 - 1;
    I2S_PWM_REG_WS_MIC(PWM_CMP1)    = (ck_period + 1) * 32;
    I2S_PWM_REG_WS_MIC(PWM_CMP2)    = (ck_period + 1) * _eos_i2s_mic_volume;
    I2S_PWM_REG_WS_MIC(PWM_CMP3)    = I2S_PWM_REG_WS_MIC(PWM_CMP2) + (ck_period + 1) * 17;

    I2S_PWM_REG_WS_SPK(PWM_CMP0)    = (ck_period + 1) * 64 - 1;
    I2S_PWM_REG_WS_SPK(PWM_CMP1)    = (ck_period + 1) * 32;
    I2S_PWM_REG_WS_SPK(PWM_CMP2)    = (ck_period + 1) * 33;

    I2S_PWM_REG_CK(PWM_COUNT)       = 0;
    I2S_PWM_REG_WS_MIC(PWM_COUNT)   = (ck_period + 1) * 32;
    I2S_PWM_REG_WS_SPK(PWM_COUNT)   = (ck_period + 1) * 32 + (ck_period + 1) * (_eos_i2s_spk_volume + 1 - _eos_i2s_mic_volume) + (ck_period + 1) / 2;

    _eos_i2s_fmt = fmt;
    _eos_i2s_mic_evt_enable = 1;
    _eos_i2s_spk_evt_enable = 1;

    eos_intr_set_priority(I2S_IRQ_WS_ID, IRQ_PRIORITY_I2S_WS);
    eos_intr_set_priority(I2S_IRQ_SD_ID, 0);
    eos_intr_enable(I2S_IRQ_WS_ID);
    eos_intr_enable(I2S_IRQ_SD_ID);

    _eos_i2s_start_pwm();
    /*
    I2S_PWM_REG_CK(PWM_CFG)         = PWM_CFG_ENALWAYS | PWM_CFG_ZEROCMP | I2S_PWM_SCALE_CK;
    I2S_PWM_REG_WS_MIC(PWM_CFG)     = PWM_CFG_ENALWAYS | PWM_CFG_ZEROCMP | PWM_CFG_CMP2GANG;
    I2S_PWM_REG_WS_SPK(PWM_CFG)     = PWM_CFG_ENALWAYS | PWM_CFG_ZEROCMP;
    */

    GPIO_REG(GPIO_IOF_SEL)          |=  (1 << I2S_PIN_CK);
    GPIO_REG(GPIO_IOF_EN)           |=  (1 << I2S_PIN_CK);

    GPIO_REG(GPIO_IOF_SEL)          |=  (1 << I2S_PIN_CK_SW);
    GPIO_REG(GPIO_IOF_EN)           |=  (1 << I2S_PIN_CK_SW);

    GPIO_REG(GPIO_IOF_SEL)          |=  (1 << I2S_PIN_WS_MIC);
    GPIO_REG(GPIO_IOF_EN)           |=  (1 << I2S_PIN_WS_MIC);

    GPIO_REG(GPIO_IOF_SEL)          |=  (1 << I2S_PIN_WS_SPK);
    GPIO_REG(GPIO_IOF_EN)           |=  (1 << I2S_PIN_WS_SPK);

    GPIO_REG(GPIO_OUTPUT_VAL)       |=  (1 << I2S_PIN_CK_SR);
}

void eos_i2s_stop(void) {
    I2S_PWM_REG_CK(PWM_CFG)         = 0;
    I2S_PWM_REG_WS_MIC(PWM_CFG)     = 0;
    I2S_PWM_REG_WS_SPK(PWM_CFG)     = 0;
    I2S_PWM_REG_CK(PWM_COUNT)       = 0;
    I2S_PWM_REG_WS_MIC(PWM_COUNT)   = 0;
    I2S_PWM_REG_WS_SPK(PWM_COUNT)   = 0;

    _eos_i2s_mic_evt_enable = 0;
    _eos_i2s_spk_evt_enable = 0;
    eos_intr_set_priority(I2S_IRQ_WS_ID, 0);
    eos_intr_set_priority(I2S_IRQ_SD_ID, 0);
    eos_intr_disable(I2S_IRQ_WS_ID);
    eos_intr_disable(I2S_IRQ_SD_ID);

    GPIO_REG(GPIO_IOF_EN)           &= ~(1 << I2S_PIN_CK);
    GPIO_REG(GPIO_IOF_SEL)          &= ~(1 << I2S_PIN_CK);

    GPIO_REG(GPIO_IOF_EN)           &= ~(1 << I2S_PIN_CK_SW);
    GPIO_REG(GPIO_IOF_SEL)          &= ~(1 << I2S_PIN_CK_SW);

    GPIO_REG(GPIO_IOF_EN)           &= ~(1 << I2S_PIN_WS_MIC);
    GPIO_REG(GPIO_IOF_SEL)          &= ~(1 << I2S_PIN_WS_MIC);

    GPIO_REG(GPIO_IOF_EN)           &= ~(1 << I2S_PIN_WS_SPK);
    GPIO_REG(GPIO_IOF_SEL)          &= ~(1 << I2S_PIN_WS_SPK);

    GPIO_REG(GPIO_OUTPUT_VAL)       &= ~((1 << I2S_PIN_CK) | (1 << I2S_PIN_CK_SR) | (1 << I2S_PIN_WS_MIC) | (1 << I2S_PIN_WS_SPK));
    GPIO_REG(GPIO_OUTPUT_VAL)       |= (1 << I2S_PIN_CK_SW);
}

void eos_i2s_mic_init(uint8_t *mic_arr, uint16_t mic_arr_size) {
    clear_csr(mstatus, MSTATUS_MIE);
    _abuf_init(&_eos_i2s_mic_buf, mic_arr, mic_arr_size);
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_i2s_mic_set_handler(eos_i2s_fptr_t wm_handler) {
    clear_csr(mstatus, MSTATUS_MIE);
    mic_evt_handler = wm_handler;
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_i2s_mic_set_wm(uint16_t wm) {
    clear_csr(mstatus, MSTATUS_MIE);
    _eos_i2s_mic_wm = wm;
    set_csr(mstatus, MSTATUS_MIE);

}

uint16_t eos_i2s_mic_len(void) {
    clear_csr(mstatus, MSTATUS_MIE);
    uint16_t ret = _abuf_len(&_eos_i2s_mic_buf);
    set_csr(mstatus, MSTATUS_MIE);
    return ret;
}

uint16_t eos_i2s_mic_read(uint8_t *sample, uint16_t ssize) {
    uint16_t i;
    uint16_t _ssize = 0;

    clear_csr(mstatus, MSTATUS_MIE);
    _ssize = MIN(ssize, _abuf_len(&_eos_i2s_mic_buf));
    set_csr(mstatus, MSTATUS_MIE);

    for (i=0; i<_ssize; i++) {
        sample[i] = _eos_i2s_mic_buf.array[EOS_ABUF_IDX_MASK(_eos_i2s_mic_buf.idx_r + i, _eos_i2s_mic_buf.size)];
    }

    clear_csr(mstatus, MSTATUS_MIE);
    _eos_i2s_mic_buf.idx_r += _ssize;
    set_csr(mstatus, MSTATUS_MIE);

    return _ssize;
}

int eos_i2s_mic_pop8(uint8_t *sample) {
    clear_csr(mstatus, MSTATUS_MIE);
    int ret = _abuf_pop8(&_eos_i2s_mic_buf, sample);
    set_csr(mstatus, MSTATUS_MIE);
    return ret;
}

int eos_i2s_mic_pop16(uint16_t *sample) {
    clear_csr(mstatus, MSTATUS_MIE);
    int ret = _abuf_pop16(&_eos_i2s_mic_buf, sample);
    set_csr(mstatus, MSTATUS_MIE);
    return ret;
}

void eos_i2s_spk_init(uint8_t *spk_arr, uint16_t spk_arr_size) {
    clear_csr(mstatus, MSTATUS_MIE);
    _abuf_init(&_eos_i2s_spk_buf, spk_arr, spk_arr_size);
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_i2s_spk_set_handler(eos_i2s_fptr_t wm_handler) {
    clear_csr(mstatus, MSTATUS_MIE);
    spk_evt_handler = wm_handler;
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_i2s_spk_set_wm(uint16_t wm) {
    clear_csr(mstatus, MSTATUS_MIE);
    _eos_i2s_spk_wm = wm;
    set_csr(mstatus, MSTATUS_MIE);
}

uint16_t eos_i2s_spk_len(void) {
    clear_csr(mstatus, MSTATUS_MIE);
    uint16_t ret = _abuf_len(&_eos_i2s_spk_buf);
    set_csr(mstatus, MSTATUS_MIE);
    return ret;
}

uint16_t eos_i2s_spk_write(uint8_t *sample, uint16_t ssize) {
    uint16_t i;
    uint16_t _ssize = 0;

    clear_csr(mstatus, MSTATUS_MIE);
    _ssize = MIN(ssize, _eos_i2s_spk_buf.size - _abuf_len(&_eos_i2s_spk_buf));
    set_csr(mstatus, MSTATUS_MIE);

    for (i=0; i<_ssize; i++) {
        _eos_i2s_spk_buf.array[EOS_ABUF_IDX_MASK(_eos_i2s_spk_buf.idx_w + i, _eos_i2s_spk_buf.size)] = sample[i];
    }

    clear_csr(mstatus, MSTATUS_MIE);
    _eos_i2s_spk_buf.idx_w += _ssize;
    set_csr(mstatus, MSTATUS_MIE);

    return _ssize;
}

int eos_i2s_spk_push8(uint8_t sample) {
    clear_csr(mstatus, MSTATUS_MIE);
    int ret = _abuf_push8(&_eos_i2s_spk_buf, sample);
    set_csr(mstatus, MSTATUS_MIE);
    return ret;
}

int eos_i2s_spk_push16(uint16_t sample) {
    clear_csr(mstatus, MSTATUS_MIE);
    int ret = _abuf_push16(&_eos_i2s_spk_buf, sample);
    set_csr(mstatus, MSTATUS_MIE);
    return ret;
}

