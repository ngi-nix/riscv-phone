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

#define I2S_PIN_PWM         ((1 << I2S_PIN_CK) | (1 << I2S_PIN_CK_SW) | (1 << I2S_PIN_WS_MIC) | (1 << I2S_PIN_WS_SPK))

#define MIN(X, Y)           (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)           (((X) > (Y)) ? (X) : (Y))

#define EOS_ABUF_IDX_MASK(IDX, SIZE)  ((IDX) & ((SIZE) - 1))

EOSABuf i2s_mic_buf;
EOSABuf i2s_spk_buf;

static eos_i2s_handler_t i2s_mic_handler = NULL;
static eos_i2s_handler_t i2s_spk_handler = NULL;
static uint32_t i2s_clk_period;
static uint8_t i2s_mic_volume = 0;      /* 0 - 8 */
static uint8_t i2s_spk_volume = 16;     /* 0 - 16 */

uint32_t _eos_i2s_drvr[] = {
    0,                      /* I2S_MIC_BUF  */
    0,                      /* I2S_SPK_BUF  */
    EOS_I2S_FMT_PCM16,      /* I2S_FMT      */
    EOS_I2S_MODE_STEREO,    /* I2S_MODE     */
    0,                      /* I2S_MIC_WM   */
    0,                      /* I2S_SPK_WM   */
    0,                      /* I2S_MIC_EVT  */
    0,                      /* I2S_SPK_EVT  */
    0,                      /* I2S_MIC_CMP2 */
    0,                      /* I2S_MIC_CMP3 */
    0,                      /* I2S_SAMPLE */
};

#define I2S_MIC_BUF     0
#define I2S_SPK_BUF     1
#define I2S_FMT         2
#define I2S_MODE        3
#define I2S_MIC_WM      4
#define I2S_SPK_WM      5
#define I2S_MIC_EVT     6
#define I2S_SPK_EVT     7
#define I2S_MIC_CMP2    8
#define I2S_MIC_CMP3    9
#define I2S_SAMPLE      10

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
            if (i2s_mic_handler) {
                i2s_mic_handler(type);
                clear_csr(mstatus, MSTATUS_MIE);
                _eos_i2s_drvr[I2S_MIC_EVT] = 1;
                set_csr(mstatus, MSTATUS_MIE);
            }
            break;

        case EOS_I2S_ETYPE_SPK:
            if (i2s_spk_handler) {
                i2s_spk_handler(type);
                clear_csr(mstatus, MSTATUS_MIE);
                _eos_i2s_drvr[I2S_SPK_EVT] = 1;
                set_csr(mstatus, MSTATUS_MIE);
            }
            break;

        default:
            eos_evtq_bad_handler(type, buffer, len);
            break;
    }
}

#define PLIC_PRIORITY     0x0C000000

static void i2s_cmp_set(void) {
    int c = 7;    /* interrupt will trigger c i2s clocks before spk ws */
    int spk_ws_offset = i2s_spk_volume - 16 + i2s_mic_volume;
    volatile  uint32_t *p = (uint32_t *)PLIC_PRIORITY+I2S_IRQ_SD_ID;

    /* interrupt trigger - will start with left channel */
    if (spk_ws_offset - c < 0) {
        I2S_REG_WS_SPK(PWM_CMP3) = i2s_clk_period * (64 + spk_ws_offset - c);
    } else {
        I2S_REG_WS_SPK(PWM_CMP3) = i2s_clk_period * (spk_ws_offset - c);
    }

    /* disable interrupt for this cycle */
    *p = 0;

    /* empty buffers */
    // i2s_mic_buf.idx_r = i2s_mic_buf.idx_w;
    // i2s_spk_buf.idx_w = i2s_spk_buf.idx_r;

    /* adjust spk ws relative to mic ws */
    if (spk_ws_offset <= 0) {
        spk_ws_offset += 32;
        GPIO_REG(GPIO_OUTPUT_XOR) &= ~(1 << I2S_PIN_WS_SPK);
    } else {
        GPIO_REG(GPIO_OUTPUT_XOR) |=  (1 << I2S_PIN_WS_SPK);
    }
    I2S_REG_WS_SPK(PWM_CMP1) = i2s_clk_period * spk_ws_offset;
    I2S_REG_WS_SPK(PWM_CMP2) = i2s_clk_period * (32 + spk_ws_offset);

    /* mic cmp2 relative to interrupt trigger */
    _eos_i2s_drvr[I2S_MIC_CMP2] = (17 + c - i2s_spk_volume) * i2s_clk_period;   /* (17 + c - i2s_spk_volume) == (1 + i2s_mic_volume) - (spk_ws_offset - c) */
    _eos_i2s_drvr[I2S_MIC_CMP3] = 16 * i2s_clk_period;
}

extern void _eos_i2s_start_pwm(void);

int eos_i2s_init(uint8_t wakeup_cause) {
    eos_evtq_set_handler(EOS_EVT_I2S, i2s_handle_evt);

    I2S_REG_CK(PWM_CFG)         = 0;
    I2S_REG_WS_MIC(PWM_CFG)     = 0;
    I2S_REG_WS_SPK(PWM_CFG)     = 0;

    eos_i2s_init_mux();

    GPIO_REG(GPIO_OUTPUT_VAL)   |=  (1 << I2S_PIN_CK_SW);
    GPIO_REG(GPIO_OUTPUT_VAL)   &= ~(1 << I2S_PIN_CK_SR);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_CK_SW);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_CK_SW);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_CK_SR);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_CK_SR);

    return EOS_OK;
}

void eos_i2s_init_mux(void) {
    GPIO_REG(GPIO_OUTPUT_VAL)   &= ~((1 << I2S_PIN_CK) | (1 << I2S_PIN_WS_MIC) | (1 << I2S_PIN_WS_SPK));

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_CK);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_CK);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_WS_MIC);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_WS_MIC);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_WS_SPK);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_WS_SPK);

    GPIO_REG(GPIO_INPUT_EN)     |=  (1 << I2S_PIN_SD_IN);
    GPIO_REG(GPIO_OUTPUT_EN)    &= ~(1 << I2S_PIN_SD_IN);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_SD_OUT);
    GPIO_REG(GPIO_OUTPUT_EN)    &= ~(1 << I2S_PIN_SD_OUT);
}

void eos_i2s_start(uint32_t sample_rate) {
    i2s_clk_period = ((PRCI_get_cpu_freq() / (sample_rate * 64)) & ~I2S_PWM_SCALE_CK_MASK) + 1;

    GPIO_REG(GPIO_INPUT_EN)     |=  (1 << I2S_PIN_SD_IN);
    GPIO_REG(GPIO_OUTPUT_EN)    &= ~(1 << I2S_PIN_SD_IN);

    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_SD_OUT);
    GPIO_REG(GPIO_OUTPUT_EN)    &= ~(1 << I2S_PIN_SD_OUT);

    I2S_REG_CK(PWM_CMP0)        = i2s_clk_period >> I2S_PWM_SCALE_CK;
    I2S_REG_CK(PWM_CMP1)        = I2S_REG_CK(PWM_CMP0) / 2;
    I2S_REG_CK(PWM_CMP2)        = 0;
    I2S_REG_CK(PWM_CMP3)        = 0;

    I2S_REG_WS_MIC(PWM_CMP0)    = i2s_clk_period * 64 - 1;
    I2S_REG_WS_MIC(PWM_CMP1)    = i2s_clk_period * 32;

    I2S_REG_WS_SPK(PWM_CMP0)    = i2s_clk_period * 64 - 1;
    i2s_cmp_set();

    I2S_REG_CK(PWM_COUNT)       = 0;
    I2S_REG_WS_MIC(PWM_COUNT)   = 0;
    I2S_REG_WS_SPK(PWM_COUNT)   = i2s_clk_period / 2;

    if (i2s_mic_buf.array && i2s_mic_buf.size) {
        _eos_i2s_drvr[I2S_MIC_BUF] = (uint32_t)&i2s_mic_buf;
        if (_eos_i2s_drvr[I2S_MIC_WM] == 0) {
            _eos_i2s_drvr[I2S_MIC_WM] = i2s_mic_buf.size / 2;
        }
    }
    if (i2s_spk_buf.array && i2s_spk_buf.size) {
        _eos_i2s_drvr[I2S_SPK_BUF] = (uint32_t)&i2s_spk_buf;
        if (_eos_i2s_drvr[I2S_SPK_WM] == 0) {
            _eos_i2s_drvr[I2S_SPK_WM] = i2s_spk_buf.size / 2;
        }
    }
    if (i2s_mic_handler) _eos_i2s_drvr[I2S_MIC_EVT] = 1;
    if (i2s_spk_handler) _eos_i2s_drvr[I2S_SPK_EVT] = 1;

    eos_intr_set_priority(I2S_IRQ_SD_ID, IRQ_PRIORITY_I2S_SD);
    eos_intr_set_priority(I2S_IRQ_WS_ID, IRQ_PRIORITY_I2S_WS);
    eos_intr_enable(I2S_IRQ_SD_ID);
    eos_intr_enable(I2S_IRQ_WS_ID);

    _eos_i2s_start_pwm();
    /*
    I2S_REG_CK(PWM_CFG)         = PWM_CFG_ENALWAYS | PWM_CFG_ZEROCMP | I2S_PWM_SCALE_CK;
    I2S_REG_WS_MIC(PWM_CFG)     = PWM_CFG_ENALWAYS | PWM_CFG_ZEROCMP | PWM_CFG_CMP2GANG;
    I2S_REG_WS_SPK(PWM_CFG)     = PWM_CFG_ENALWAYS | PWM_CFG_ZEROCMP | PWM_CFG_CMP1GANG;
    */

    GPIO_REG(GPIO_OUTPUT_VAL)   |=  (1 << I2S_PIN_WS_MIC);
    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << I2S_PIN_WS_MIC);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << I2S_PIN_WS_MIC);

    GPIO_REG(GPIO_OUTPUT_VAL)   |=  (1 << I2S_PIN_CK_SR);
    GPIO_REG(GPIO_IOF_SEL)      |=  I2S_PIN_PWM;
    GPIO_REG(GPIO_IOF_EN)       |=  I2S_PIN_PWM;
}

void eos_i2s_stop(void) {
    I2S_REG_CK(PWM_CFG)         = 0;
    I2S_REG_WS_MIC(PWM_CFG)     = 0;
    I2S_REG_WS_SPK(PWM_CFG)     = 0;
    I2S_REG_CK(PWM_COUNT)       = 0;
    I2S_REG_WS_MIC(PWM_COUNT)   = 0;
    I2S_REG_WS_SPK(PWM_COUNT)   = 0;

    _eos_i2s_drvr[I2S_MIC_BUF]  = 0;
    _eos_i2s_drvr[I2S_MIC_EVT]  = 0;
    _eos_i2s_drvr[I2S_MIC_WM]   = 0;

    _eos_i2s_drvr[I2S_SPK_BUF]  = 0;
    _eos_i2s_drvr[I2S_SPK_EVT]  = 0;
    _eos_i2s_drvr[I2S_SPK_WM]   = 0;

    eos_intr_disable(I2S_IRQ_WS_ID);
    eos_intr_disable(I2S_IRQ_SD_ID);

    GPIO_REG(GPIO_OUTPUT_VAL)   |=  (1 << I2S_PIN_CK_SW);
    GPIO_REG(GPIO_OUTPUT_VAL)   &= ~(1 << I2S_PIN_CK_SR);

    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << I2S_PIN_WS_SPK);
    GPIO_REG(GPIO_IOF_EN)       &= ~I2S_PIN_PWM;

    eos_i2s_mic_set_wm(0);
    eos_i2s_spk_set_wm(0);
}

int eos_i2s_running(void) {
    return !!(GPIO_REG(GPIO_IOF_EN) & (1 << I2S_PIN_CK));
}

void eos_i2s_set_fmt(unsigned char fmt) {
    _eos_i2s_drvr[I2S_FMT] = fmt;
}

void eos_i2s_set_mode(unsigned char mode) {
    _eos_i2s_drvr[I2S_MODE] = mode;
}

void eos_i2s_mic_init(uint8_t *mic_arr, uint16_t mic_arr_size) {
    clear_csr(mstatus, MSTATUS_MIE);
    _abuf_init(&i2s_mic_buf, mic_arr, mic_arr_size);
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_i2s_mic_set_handler(eos_i2s_handler_t wm_handler) {
    clear_csr(mstatus, MSTATUS_MIE);
    i2s_mic_handler = wm_handler;
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_i2s_mic_set_wm(uint16_t wm) {
    clear_csr(mstatus, MSTATUS_MIE);
    _eos_i2s_drvr[I2S_MIC_WM] = wm;
    set_csr(mstatus, MSTATUS_MIE);

}

uint16_t eos_i2s_mic_len(void) {
    clear_csr(mstatus, MSTATUS_MIE);
    uint16_t ret = _abuf_len(&i2s_mic_buf);
    set_csr(mstatus, MSTATUS_MIE);
    return ret;
}

uint16_t eos_i2s_mic_read(uint8_t *sample, uint16_t ssize) {
    uint16_t i;
    uint16_t _ssize = 0;

    clear_csr(mstatus, MSTATUS_MIE);
    _ssize = MIN(ssize, _abuf_len(&i2s_mic_buf));
    set_csr(mstatus, MSTATUS_MIE);

    for (i=0; i<_ssize; i++) {
        sample[i] = i2s_mic_buf.array[EOS_ABUF_IDX_MASK(i2s_mic_buf.idx_r + i, i2s_mic_buf.size)];
    }

    clear_csr(mstatus, MSTATUS_MIE);
    i2s_mic_buf.idx_r += _ssize;
    set_csr(mstatus, MSTATUS_MIE);

    return _ssize;
}

int eos_i2s_mic_pop8(uint8_t *sample) {
    int ret;

    clear_csr(mstatus, MSTATUS_MIE);
    ret = _abuf_pop8(&i2s_mic_buf, sample);
    set_csr(mstatus, MSTATUS_MIE);

    return ret;
}

int eos_i2s_mic_pop16(uint16_t *sample) {
    int ret;

    clear_csr(mstatus, MSTATUS_MIE);
    ret = _abuf_pop16(&i2s_mic_buf, sample);
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
    i2s_cmp_set();
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_i2s_spk_init(uint8_t *spk_arr, uint16_t spk_arr_size) {
    clear_csr(mstatus, MSTATUS_MIE);
    _abuf_init(&i2s_spk_buf, spk_arr, spk_arr_size);
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_i2s_spk_set_handler(eos_i2s_handler_t wm_handler) {
    clear_csr(mstatus, MSTATUS_MIE);
    i2s_spk_handler = wm_handler;
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_i2s_spk_set_wm(uint16_t wm) {
    clear_csr(mstatus, MSTATUS_MIE);
    _eos_i2s_drvr[I2S_SPK_WM] = wm;
    set_csr(mstatus, MSTATUS_MIE);
}

uint16_t eos_i2s_spk_len(void) {
    clear_csr(mstatus, MSTATUS_MIE);
    uint16_t ret = _abuf_len(&i2s_spk_buf);
    set_csr(mstatus, MSTATUS_MIE);
    return ret;
}

uint16_t eos_i2s_spk_write(uint8_t *sample, uint16_t ssize) {
    uint16_t i;
    uint16_t _ssize = 0;

    clear_csr(mstatus, MSTATUS_MIE);
    _ssize = MIN(ssize, i2s_spk_buf.size - _abuf_len(&i2s_spk_buf));
    set_csr(mstatus, MSTATUS_MIE);

    for (i=0; i<_ssize; i++) {
        i2s_spk_buf.array[EOS_ABUF_IDX_MASK(i2s_spk_buf.idx_w + i, i2s_spk_buf.size)] = sample[i];
    }

    clear_csr(mstatus, MSTATUS_MIE);
    i2s_spk_buf.idx_w += _ssize;
    set_csr(mstatus, MSTATUS_MIE);

    return _ssize;
}

int eos_i2s_spk_push8(uint8_t sample) {
    int ret;

    clear_csr(mstatus, MSTATUS_MIE);
    ret = _abuf_push8(&i2s_spk_buf, sample);
    set_csr(mstatus, MSTATUS_MIE);

    return ret;
}

int eos_i2s_spk_push16(uint16_t sample) {
    int ret;

    clear_csr(mstatus, MSTATUS_MIE);
    ret = _abuf_push16(&i2s_spk_buf, sample);
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
    i2s_cmp_set();
    set_csr(mstatus, MSTATUS_MIE);
}
