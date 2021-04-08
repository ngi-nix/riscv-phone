#include <stdlib.h>
#include <stdint.h>

#include "encoding.h"
#include "platform.h"
#include "prci_driver.h"

#include "eos.h"
#include "interrupt.h"
#include "event.h"

#include "board.h"

#include "i2s.h"
#include "i2s_priv.h"

#define I2S_REG_CK(o)       _REG32(I2S_CTRL_ADDR_CK, o)
#define I2S_REG_WS_MIC(o)   _REG32(I2S_CTRL_ADDR_WS_MIC, o)
#define I2S_REG_WS_SPK(o)   _REG32(I2S_CTRL_ADDR_WS_SPK, o)

#define MIN(X, Y)           (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)           (((X) > (Y)) ? (X) : (Y))

#define EOS_ABUF_IDX_MASK(IDX, SIZE)  ((IDX) & ((SIZE) - 1))

EOSABuf _eos_i2s_mic_buf;
EOSABuf _eos_i2s_spk_buf;
uint32_t _eos_i2s_fmt = 0;
uint32_t _eos_i2s_mic_wm = 0;
uint32_t _eos_i2s_spk_wm = 0;
uint32_t _eos_i2s_mic_evt_enable = 0;
uint32_t _eos_i2s_spk_evt_enable = 0;

static eos_i2s_handler_t i2s_spk_handler = NULL;
static eos_i2s_handler_t i2s_mic_handler = NULL;
static uint32_t i2s_clk_period;
static uint8_t i2s_mic_volume = 0;      /* 0 - 8 */
static uint8_t i2s_spk_volume = 16;     /* 0 - 16 */

static void _abuf_init(EOSABuf *buf, uint8_t *array, uint16_t size) {
    buf->idx_r = 0;
    buf->idx_w = 0;
    buf->size = size;
    buf->array = array;
}

static int _abuf_push8(EOSABuf *buf, uint8_t sample) {
    if ((uint16_t)(buf->idx_w - buf->idx_r) == buf->size) return EOS_ERR_FULL;

    buf->array[EOS_ABUF_IDX_MASK(buf->idx_w, buf->size)] = sample;
    buf->idx_w++;
    return EOS_OK;
}

static int _abuf_push16(EOSABuf *buf, uint16_t sample) {
    if ((uint16_t)(buf->idx_w - buf->idx_r) == buf->size) return EOS_ERR_FULL;

    buf->array[EOS_ABUF_IDX_MASK(buf->idx_w, buf->size)] = sample >> 8;
    buf->array[EOS_ABUF_IDX_MASK(buf->idx_w + 1, buf->size)] = sample & 0xFF;
    buf->idx_w += 2;
    return EOS_OK;
}

static int _abuf_pop8(EOSABuf *buf, uint8_t *sample) {
    if (buf->idx_r == buf->idx_w) {
        return EOS_ERR_EMPTY;
    } else {
        *sample = buf->array[EOS_ABUF_IDX_MASK(buf->idx_r, buf->size)];
        buf->idx_r++;
        return EOS_OK;
    }
}

static int _abuf_pop16(EOSABuf *buf, uint16_t *sample) {
    if (buf->idx_r == buf->idx_w) {
        return EOS_ERR_EMPTY;
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

static void i2s_handle_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    switch(type & ~EOS_EVT_MASK) {
        case EOS_I2S_ETYPE_MIC:
            if (i2s_mic_handler) i2s_mic_handler(type);
            clear_csr(mstatus, MSTATUS_MIE);
            _eos_i2s_mic_evt_enable = 1;
            set_csr(mstatus, MSTATUS_MIE);
            break;
        case EOS_I2S_ETYPE_SPK:
            if (i2s_spk_handler) i2s_spk_handler(type);
            clear_csr(mstatus, MSTATUS_MIE);
            _eos_i2s_spk_evt_enable = 1;
            set_csr(mstatus, MSTATUS_MIE);
            break;
        default:
            eos_evtq_bad_handler(type, buffer, len);
            break;
    }
}

static void _mic_vol_set(uint8_t vol) {
    I2S_REG_WS_MIC(PWM_CMP2) = i2s_clk_period * (vol + 1);
    I2S_REG_WS_MIC(PWM_CMP3) = I2S_REG_WS_MIC(PWM_CMP2) + i2s_clk_period * 16;
}

static void _spk_vol_set(uint8_t vol) {
    int spk_cmp = vol + i2s_mic_volume - 16;

    if (spk_cmp <= 0) {
        I2S_REG_WS_SPK(PWM_CMP1) = i2s_clk_period * (32 + spk_cmp);
        I2S_REG_WS_SPK(PWM_CMP2) = i2s_clk_period * (64 + spk_cmp);
        I2S_REG_WS_SPK(PWM_CMP3) = i2s_clk_period * 33;
        GPIO_REG(GPIO_OUTPUT_XOR) &= ~(1 << I2S_PIN_WS_SPK);
    } else {
        I2S_REG_WS_SPK(PWM_CMP1) = i2s_clk_period * spk_cmp;
        I2S_REG_WS_SPK(PWM_CMP2) = i2s_clk_period * (32 + spk_cmp);
        I2S_REG_WS_SPK(PWM_CMP3) = i2s_clk_period * (33 + spk_cmp);
        GPIO_REG(GPIO_OUTPUT_XOR) |= (1 << I2S_PIN_WS_SPK);
    }
}

extern void _eos_i2s_start_pwm(void);

void eos_i2s_init(void) {
    eos_evtq_set_handler(EOS_EVT_I2S, i2s_handle_evt);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_CK);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_CK);
    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << I2S_PIN_CK);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << I2S_PIN_CK);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_CK_SW);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_CK_SW);
    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << I2S_PIN_CK_SW);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << I2S_PIN_CK_SW);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_WS_MIC);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_WS_MIC);
    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << I2S_PIN_WS_MIC);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << I2S_PIN_WS_MIC);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_WS_SPK);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_WS_SPK);
    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << I2S_PIN_WS_SPK);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << I2S_PIN_WS_SPK);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_CK_SR);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_CK_SR);
    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << I2S_PIN_CK_SR);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << I2S_PIN_CK_SR);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_SD_IN);
    GPIO_REG(GPIO_OUTPUT_EN)    &= ~(1 << I2S_PIN_SD_IN);
    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << I2S_PIN_SD_IN);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << I2S_PIN_SD_IN);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_SD_OUT);
    GPIO_REG(GPIO_OUTPUT_EN)    &= ~(1 << I2S_PIN_SD_OUT);
    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << I2S_PIN_SD_OUT);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << I2S_PIN_SD_OUT);

    GPIO_REG(GPIO_OUTPUT_VAL)   |=  (1 << I2S_PIN_CK_SW);
    GPIO_REG(GPIO_OUTPUT_VAL)   &= ~((1 << I2S_PIN_CK) | (1 << I2S_PIN_CK_SR) | (1 << I2S_PIN_WS_MIC) | (1 << I2S_PIN_WS_SPK));
}

void eos_i2s_start(uint32_t sample_rate, unsigned char fmt) {
    i2s_clk_period = ((PRCI_get_cpu_freq() / (sample_rate * 64)) & ~I2S_PWM_SCALE_CK_MASK) + 1;

    I2S_REG_CK(PWM_CMP0)        = i2s_clk_period >> I2S_PWM_SCALE_CK;
    I2S_REG_CK(PWM_CMP1)        = I2S_REG_CK(PWM_CMP0) / 2;
    I2S_REG_CK(PWM_CMP2)        = 0;
    I2S_REG_CK(PWM_CMP3)        = 0;

    I2S_REG_WS_MIC(PWM_CMP0)    = i2s_clk_period * 64 - 1;
    I2S_REG_WS_MIC(PWM_CMP1)    = i2s_clk_period * 32;
    _mic_vol_set(i2s_mic_volume);

    I2S_REG_WS_SPK(PWM_CMP0)    = i2s_clk_period * 64 - 1;
    _spk_vol_set(i2s_spk_volume);

    I2S_REG_CK(PWM_COUNT)       = 0;
    I2S_REG_WS_MIC(PWM_COUNT)   = 0;
    I2S_REG_WS_SPK(PWM_COUNT)   = i2s_clk_period / 2;

    _eos_i2s_fmt = fmt;
    _eos_i2s_mic_evt_enable = 1;
    _eos_i2s_spk_evt_enable = 1;

    eos_intr_set_priority(I2S_IRQ_WS_ID, IRQ_PRIORITY_I2S_WS);
    eos_intr_set_priority(I2S_IRQ_SD_ID, 0);
    eos_intr_enable(I2S_IRQ_WS_ID);
    eos_intr_enable(I2S_IRQ_SD_ID);

    _eos_i2s_start_pwm();
    /*
    I2S_REG_CK(PWM_CFG)         = PWM_CFG_ENALWAYS | PWM_CFG_ZEROCMP | I2S_PWM_SCALE_CK;
    I2S_REG_WS_MIC(PWM_CFG)     = PWM_CFG_ENALWAYS | PWM_CFG_ZEROCMP | PWM_CFG_CMP2GANG;
    I2S_REG_WS_SPK(PWM_CFG)     = PWM_CFG_ENALWAYS | PWM_CFG_ZEROCMP | PWM_CFG_CMP1GANG;
    */

    GPIO_REG(GPIO_INPUT_EN)     |=  (1 << I2S_PIN_SD_IN);
    GPIO_REG(GPIO_OUTPUT_VAL)   |=  (1 << I2S_PIN_CK_SR);

    GPIO_REG(GPIO_IOF_SEL)      |=  (1 << I2S_PIN_CK);
    GPIO_REG(GPIO_IOF_EN)       |=  (1 << I2S_PIN_CK);

    GPIO_REG(GPIO_IOF_SEL)      |=  (1 << I2S_PIN_CK_SW);
    GPIO_REG(GPIO_IOF_EN)       |=  (1 << I2S_PIN_CK_SW);

    GPIO_REG(GPIO_IOF_SEL)      |=  (1 << I2S_PIN_WS_MIC);
    GPIO_REG(GPIO_IOF_EN)       |=  (1 << I2S_PIN_WS_MIC);

    GPIO_REG(GPIO_IOF_SEL)      |=  (1 << I2S_PIN_WS_SPK);
    GPIO_REG(GPIO_IOF_EN)       |=  (1 << I2S_PIN_WS_SPK);
}

void eos_i2s_stop(void) {
    I2S_REG_CK(PWM_CFG)         = 0;
    I2S_REG_WS_MIC(PWM_CFG)     = 0;
    I2S_REG_WS_SPK(PWM_CFG)     = 0;
    I2S_REG_CK(PWM_COUNT)       = 0;
    I2S_REG_WS_MIC(PWM_COUNT)   = 0;
    I2S_REG_WS_SPK(PWM_COUNT)   = 0;

    _eos_i2s_mic_evt_enable = 0;
    _eos_i2s_spk_evt_enable = 0;
    eos_intr_set_priority(I2S_IRQ_WS_ID, 0);
    eos_intr_set_priority(I2S_IRQ_SD_ID, 0);
    eos_intr_disable(I2S_IRQ_WS_ID);
    eos_intr_disable(I2S_IRQ_SD_ID);

    GPIO_REG(GPIO_IOF_EN)       &= ~(1 << I2S_PIN_CK);
    GPIO_REG(GPIO_IOF_SEL)      &= ~(1 << I2S_PIN_CK);

    GPIO_REG(GPIO_IOF_EN)       &= ~(1 << I2S_PIN_CK_SW);
    GPIO_REG(GPIO_IOF_SEL)      &= ~(1 << I2S_PIN_CK_SW);

    GPIO_REG(GPIO_IOF_EN)       &= ~(1 << I2S_PIN_WS_MIC);
    GPIO_REG(GPIO_IOF_SEL)      &= ~(1 << I2S_PIN_WS_MIC);

    GPIO_REG(GPIO_IOF_EN)       &= ~(1 << I2S_PIN_WS_SPK);
    GPIO_REG(GPIO_IOF_SEL)      &= ~(1 << I2S_PIN_WS_SPK);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_SD_IN);

    GPIO_REG(GPIO_OUTPUT_VAL)   |=  (1 << I2S_PIN_CK_SW);
    GPIO_REG(GPIO_OUTPUT_VAL)   &= ~((1 << I2S_PIN_CK) | (1 << I2S_PIN_CK_SR) | (1 << I2S_PIN_WS_MIC) | (1 << I2S_PIN_WS_SPK));
}

void eos_i2s_mic_init(uint8_t *mic_arr, uint16_t mic_arr_size) {
    clear_csr(mstatus, MSTATUS_MIE);
    _abuf_init(&_eos_i2s_mic_buf, mic_arr, mic_arr_size);
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_i2s_mic_set_handler(eos_i2s_handler_t wm_handler) {
    clear_csr(mstatus, MSTATUS_MIE);
    i2s_mic_handler = wm_handler;
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

int eos_i2s_mic_vol_get(void) {
    return i2s_mic_volume;
}

void eos_i2s_mic_vol_set(int vol) {
    if ((vol < 0) || (vol > 8)) return;

    i2s_mic_volume = vol;

    clear_csr(mstatus, MSTATUS_MIE);
    _mic_vol_set(vol);
    _spk_vol_set(i2s_spk_volume);
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_i2s_spk_init(uint8_t *spk_arr, uint16_t spk_arr_size) {
    clear_csr(mstatus, MSTATUS_MIE);
    _abuf_init(&_eos_i2s_spk_buf, spk_arr, spk_arr_size);
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_i2s_spk_set_handler(eos_i2s_handler_t wm_handler) {
    clear_csr(mstatus, MSTATUS_MIE);
    i2s_spk_handler = wm_handler;
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

int eos_i2s_spk_vol_get(void) {
    return i2s_spk_volume;
}

void eos_i2s_spk_vol_set(int vol) {
    if ((vol < 0) || (vol > 16)) return;

    i2s_spk_volume = vol;

    clear_csr(mstatus, MSTATUS_MIE);
    _spk_vol_set(vol);
    set_csr(mstatus, MSTATUS_MIE);
}
