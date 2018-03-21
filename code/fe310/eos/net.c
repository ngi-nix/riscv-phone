#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"
#include "msgq.h"
#include "interrupt.h"

#include "net.h"
#include "spi.h"

#define MIN(X, Y)               (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)               (((X) > (Y)) ? (X) : (Y))
#define SPI_BUFQ_IDX_MASK(IDX)  ((IDX) & (SPI_SIZE_BUFQ - 1))

EOSMsgQ _eos_spi_send_q;
static EOSMsgItem spi_sndq_array[SPI_SIZE_BUFQ];

SPIBufQ _eos_spi_buf_q;
static unsigned char spi_bufq_array[SPI_SIZE_BUFQ][SPI_SIZE_BUF];

extern EOSMsgQ _eos_event_q;

uint8_t _eos_spi_state_flags = 0;
uint32_t _eos_spi_state_len = 0;
uint32_t _eos_spi_state_len_tx = 0;
uint32_t _eos_spi_state_len_rx = 0;
uint32_t _eos_spi_state_idx_tx = 0;
uint32_t _eos_spi_state_idx_rx = 0;
unsigned char _eos_spi_state_cmd = 0;
unsigned char *_eos_spi_state_buf = NULL;
uint8_t _eos_spi_state_next_cnt = 0;
unsigned char *_eos_spi_state_next_buf = NULL;

static eos_evt_fptr_t evt_handler[EOS_NET_MAX_CMD];
static uint16_t evt_handler_wrapper_en = 0;

static int spi_bufq_push(unsigned char *buffer) {
    _eos_spi_buf_q.array[SPI_BUFQ_IDX_MASK(_eos_spi_buf_q.idx_w++)] = buffer;
    return EOS_OK;
}

static unsigned char *spi_bufq_pop(void) {
    if (_eos_spi_buf_q.idx_r == _eos_spi_buf_q.idx_w) return NULL;
    return _eos_spi_buf_q.array[SPI_BUFQ_IDX_MASK(_eos_spi_buf_q.idx_r++)];
}

static void spi_xchg_reset(void) {
    _eos_spi_state_flags &= ~SPI_FLAG_CTS;
    _eos_spi_state_flags |= SPI_FLAG_RST;
    // before starting a transaction, set SPI peripheral to desired mode
    SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_HOLD;

    while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
    SPI1_REG(SPI_REG_TXFIFO) = 0;

    SPI1_REG(SPI_REG_RXCTRL) = SPI_RXWM(0);
    SPI1_REG(SPI_REG_IE) = SPI_IP_RXWM;
}

static void spi_xchg_start(unsigned char cmd, unsigned char *buffer, uint16_t len) {
    _eos_spi_state_flags &= ~SPI_FLAG_CTS;
    _eos_spi_state_flags |= SPI_FLAG_INIT;

    if (_eos_spi_state_next_cnt && (_eos_spi_state_next_buf == NULL)) cmd |= EOS_NET_CMD_FLAG_ONEW;
    if (cmd & EOS_NET_CMD_FLAG_ONEW) _eos_spi_state_flags |= SPI_FLAG_ONEW;

    _eos_spi_state_cmd = cmd;
    _eos_spi_state_buf = buffer;
    _eos_spi_state_len_tx = len;
    _eos_spi_state_len_rx = 0;
    _eos_spi_state_idx_tx = 0;
    _eos_spi_state_idx_rx = 0;
    
    // before starting a transaction, set SPI peripheral to desired mode
    SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_HOLD;

    while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
    SPI1_REG(SPI_REG_TXFIFO) = ((cmd << 3) | (len >> 8)) & 0xFF;

    while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
    SPI1_REG(SPI_REG_TXFIFO) = len & 0xFF;

    SPI1_REG(SPI_REG_RXCTRL) = SPI_RXWM(1);
    SPI1_REG(SPI_REG_IE) = SPI_IP_RXWM;
}

static int spi_xchg_next(unsigned char *_buffer) {
    unsigned char cmd;
    unsigned char *buffer = NULL;
    uint16_t len;

    eos_msgq_pop(&_eos_spi_send_q, &cmd, &buffer, &len);
    if (cmd) {
        spi_xchg_start(cmd, buffer, len);
    } else if (_eos_spi_state_flags & SPI_FLAG_RTS) {
        if (_buffer == NULL) _buffer = spi_bufq_pop();
        if (_buffer) {
            spi_xchg_start(0, _buffer, 0);
            return 0;
        }
    }
    return 1;
}

static void spi_xchg_handler(void) {
    volatile uint32_t r1, r2;
    int i;
    
    if (_eos_spi_state_flags & SPI_FLAG_RST) {
        while ((r1 = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
        SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
        SPI1_REG(SPI_REG_IE) = 0x0;
        _eos_spi_state_flags &= ~SPI_FLAG_RST;

        return;
    } else if (_eos_spi_state_flags & SPI_FLAG_INIT) {
        while ((r1 = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
        while ((r2 = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
    
        if (_eos_spi_state_cmd & EOS_NET_CMD_FLAG_ONEW) {
            r1 = 0;
            r2 = 0;
        }

        _eos_spi_state_cmd = ((r1 & 0xFF) >> 3);
        _eos_spi_state_len_rx = ((r1 & 0x07) << 8);
        _eos_spi_state_len_rx |= (r2 & 0xFF);
        _eos_spi_state_len = MAX(_eos_spi_state_len_tx, _eos_spi_state_len_rx);

        // Work around esp32 bug
        if (_eos_spi_state_len < 6) {
            _eos_spi_state_len = 6;
        } else if ((_eos_spi_state_len + 2) % 4 != 0) {
            _eos_spi_state_len = ((_eos_spi_state_len + 2)/4 + 1) * 4 - 2;
        }

        SPI1_REG(SPI_REG_TXCTRL) = SPI_TXWM(SPI_SIZE_TXWM);
        SPI1_REG(SPI_REG_RXCTRL) = SPI_RXWM(SPI_SIZE_RXWM);
        SPI1_REG(SPI_REG_IE) = SPI_IP_TXWM | SPI_IP_RXWM;
        _eos_spi_state_flags &= ~SPI_FLAG_INIT;
        return;
    }

    if (SPI1_REG(SPI_REG_IP) & SPI_IP_TXWM) {
        uint16_t sz_chunk = MIN(_eos_spi_state_len - _eos_spi_state_idx_tx, SPI_SIZE_CHUNK);
        for (i=0; i<sz_chunk; i++) {
            volatile uint32_t x = SPI1_REG(SPI_REG_TXFIFO);
            if (x & SPI_TXFIFO_FULL) break;
            SPI1_REG(SPI_REG_TXFIFO) = _eos_spi_state_buf[_eos_spi_state_idx_tx+i];
        }
        _eos_spi_state_idx_tx += i;
    }
    
    for (i=0; i<_eos_spi_state_idx_tx - _eos_spi_state_idx_rx; i++) {
        volatile uint32_t x = SPI1_REG(SPI_REG_RXFIFO);
        if (x & SPI_RXFIFO_EMPTY) break;
        _eos_spi_state_buf[_eos_spi_state_idx_rx+i] = x & 0xFF;
    }
    _eos_spi_state_idx_rx += i;

    if (_eos_spi_state_idx_rx == _eos_spi_state_len) {
        SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
        SPI1_REG(SPI_REG_IE) = 0x0;
        if (_eos_spi_state_cmd) {
            int r = eos_msgq_push(&_eos_event_q, EOS_EVT_NET | _eos_spi_state_cmd, _eos_spi_state_buf, _eos_spi_state_len_rx);
            if (r) spi_bufq_push(_eos_spi_state_buf);
        } else if (((_eos_spi_state_flags & SPI_FLAG_ONEW) || _eos_spi_state_next_cnt) && (_eos_spi_state_next_buf == NULL)) {
            _eos_spi_state_next_buf = _eos_spi_state_buf;
            _eos_spi_state_flags &= ~SPI_FLAG_ONEW;
        } else {
            spi_bufq_push(_eos_spi_state_buf);
        }
    } else if (_eos_spi_state_idx_tx == _eos_spi_state_len) {
        SPI1_REG(SPI_REG_RXCTRL) = SPI_RXWM(MIN(_eos_spi_state_len - _eos_spi_state_idx_rx - 1, SPI_SIZE_RXWM));
        SPI1_REG(SPI_REG_IE) = SPI_IP_RXWM;
    }
}

static void spi_cts_hanler(void) {
    GPIO_REG(GPIO_RISE_IP) = (0x1 << SPI_PIN_CTS);
    _eos_spi_state_flags |= SPI_FLAG_CTS;

    if (_eos_spi_state_flags & SPI_FLAG_RDY) {
        spi_xchg_next(NULL);
    } else {
        uint32_t iof_mask = ((uint32_t)1 << IOF_SPI1_SS2);
        GPIO_REG(GPIO_IOF_EN) &= ~iof_mask;
    }
}

static void spi_rts_hanler(void) {
    uint32_t rts_offset = (0x1 << SPI_PIN_RTS);
    if (GPIO_REG(GPIO_RISE_IP) & rts_offset) {
        GPIO_REG(GPIO_RISE_IP) = rts_offset;
        _eos_spi_state_flags |= SPI_FLAG_RTS;
        if ((_eos_spi_state_flags & SPI_FLAG_RDY) && (_eos_spi_state_flags & SPI_FLAG_CTS)) spi_xchg_reset();
    } else if (GPIO_REG(GPIO_FALL_IP) & rts_offset) {
        GPIO_REG(GPIO_FALL_IP) = rts_offset;
        _eos_spi_state_flags &= ~SPI_FLAG_RTS;
    }
}

static void net_handler(unsigned char cmd, unsigned char *buffer, uint16_t len) {
    if ((cmd & ~EOS_EVT_MASK) > EOS_NET_MAX_CMD) {
        eos_evtq_bad_handler(cmd, buffer, len);
    } else {
        unsigned char idx = (cmd & ~EOS_EVT_MASK) - 1;
        uint16_t wrap = ((uint16_t)1 << idx) & evt_handler_wrapper_en;

        if (wrap) {
            eos_net_free(buffer, 1);
            buffer = NULL;
            len = 0;
        }

        evt_handler[idx](cmd, buffer, len);

        if (wrap) eos_net_release();
    }
}


void eos_net_init(void) {
    int i;
    
    _eos_spi_buf_q.idx_r = 0;
    _eos_spi_buf_q.idx_w = 0;
    for (i=0; i<SPI_SIZE_BUFQ; i++) {
        spi_bufq_push(spi_bufq_array[i]);
    }

    eos_msgq_init(&_eos_spi_send_q, spi_sndq_array, SPI_SIZE_BUFQ);
    GPIO_REG(GPIO_IOF_SEL) &= ~SPI_IOF_MASK;
    GPIO_REG(GPIO_IOF_EN) |= SPI_IOF_MASK;
    eos_intr_set(INT_SPI1_BASE, 5, spi_xchg_handler);

    GPIO_REG(GPIO_OUTPUT_EN) &= ~(0x1 << SPI_PIN_CTS);
    GPIO_REG(GPIO_PULLUP_EN) |= (0x1 << SPI_PIN_CTS);
    GPIO_REG(GPIO_INPUT_EN) |= (0x1 << SPI_PIN_CTS);
    GPIO_REG(GPIO_RISE_IE) |= (0x1 << SPI_PIN_CTS);
    eos_intr_set(INT_GPIO_BASE + SPI_PIN_CTS, 4, spi_cts_hanler);
    
    GPIO_REG(GPIO_OUTPUT_EN) &= ~(0x1 << SPI_PIN_RTS);
    GPIO_REG(GPIO_PULLUP_EN) |= (0x1 << SPI_PIN_RTS);
    GPIO_REG(GPIO_INPUT_EN) |= (0x1 << SPI_PIN_RTS);
    GPIO_REG(GPIO_RISE_IE) |= (0x1 << SPI_PIN_RTS);
    GPIO_REG(GPIO_FALL_IE) |= (0x1 << SPI_PIN_RTS);
    eos_intr_set(INT_GPIO_BASE + SPI_PIN_RTS, 4, spi_rts_hanler);

    for (i=0; i<EOS_NET_MAX_CMD; i++) {
        evt_handler[i] = eos_evtq_bad_handler;
    }
    eos_evtq_set_handler(EOS_EVT_NET, net_handler, 0);
}

void eos_net_start(uint32_t sckdiv) {
    uint32_t iof_mask = ((uint32_t)1 << IOF_SPI1_SS2);

    GPIO_REG(GPIO_IOF_SEL) &= ~iof_mask;
    GPIO_REG(GPIO_IOF_EN) |= iof_mask;

    SPI1_REG(SPI_REG_SCKDIV) = sckdiv;
    SPI1_REG(SPI_REG_SCKMODE) = SPI_MODE0;
    SPI1_REG(SPI_REG_FMT) = SPI_FMT_PROTO(SPI_PROTO_S) |
      SPI_FMT_ENDIAN(SPI_ENDIAN_MSB) |
      SPI_FMT_DIR(SPI_DIR_RX) |
      SPI_FMT_LEN(8);

    // enable CS pin for selected channel/pin
    SPI1_REG(SPI_REG_CSID) = 2; 
    
    // There is no way here to change the CS polarity.
    // SPI1_REG(SPI_REG_CSDEF) = 0xFFFF;
    
    clear_csr(mstatus, MSTATUS_MIE);
    _eos_spi_state_flags |= SPI_FLAG_RDY;
    if (_eos_spi_state_flags & SPI_FLAG_CTS) spi_xchg_next(NULL);
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_net_stop(void) {
    volatile uint8_t done = 0;
    
    clear_csr(mstatus, MSTATUS_MIE);
    _eos_spi_state_flags &= ~SPI_FLAG_RDY;
    if (_eos_spi_state_flags & SPI_FLAG_CTS) {
        uint32_t iof_mask = ((uint32_t)1 << IOF_SPI1_SS2);
        GPIO_REG(GPIO_IOF_EN) &= ~iof_mask;
        done = 1;
    }
    set_csr(mstatus, MSTATUS_MIE);
    
    while (!done) {
        clear_csr(mstatus, MSTATUS_MIE);
        done = _eos_spi_state_flags & SPI_FLAG_CTS;
        if (!done) asm volatile ("wfi");
        set_csr(mstatus, MSTATUS_MIE);
    }
}

void eos_net_set_handler(unsigned char cmd, eos_evt_fptr_t handler, uint8_t flags) {
    if (flags & EOS_EVT_FLAG_WRAP) {
        uint16_t flag = (uint16_t)1 << ((cmd & ~EOS_EVT_MASK) - 1);
        evt_handler_wrapper_en |= flag;
    }
    evt_handler[(cmd & ~EOS_EVT_MASK) - 1] = handler;
}

int eos_net_acquire(unsigned char reserved) {
    int ret = 0;
    
    if (reserved) {
        while (!ret) {
            clear_csr(mstatus, MSTATUS_MIE);
            if (_eos_spi_state_next_buf) {
                ret = 1;
                _eos_spi_state_next_cnt--;
            } else {
                asm volatile ("wfi");
            }
            set_csr(mstatus, MSTATUS_MIE);
        }
    } else {
        clear_csr(mstatus, MSTATUS_MIE);
        if (_eos_spi_state_next_buf == NULL) _eos_spi_state_next_buf = spi_bufq_pop();
        ret = (_eos_spi_state_next_buf != NULL);
        if (!ret) _eos_spi_state_next_cnt++;
        set_csr(mstatus, MSTATUS_MIE);
    }
    return ret;
}

int eos_net_release(void) {
    int rv = EOS_OK;

    clear_csr(mstatus, MSTATUS_MIE);
    if (!_eos_spi_state_next_cnt && _eos_spi_state_next_buf) {
        rv = spi_bufq_push(_eos_spi_state_next_buf);
        if (!rv) _eos_spi_state_next_buf = NULL;
    }
    set_csr(mstatus, MSTATUS_MIE);
    
    return rv;
}

unsigned char *eos_net_alloc(void) {
    unsigned char *ret = NULL;

    while (ret == NULL) {
        clear_csr(mstatus, MSTATUS_MIE);
        if (_eos_spi_state_next_buf) {
            ret = _eos_spi_state_next_buf;
            _eos_spi_state_next_buf = NULL;
        } else {
            asm volatile ("wfi");
        }
        set_csr(mstatus, MSTATUS_MIE);
    }
    
    return ret;
}

int eos_net_free(unsigned char *buffer, unsigned char reserve) {
    int rv = EOS_OK;
    uint8_t do_release = 1;

    clear_csr(mstatus, MSTATUS_MIE);
    if ((reserve || _eos_spi_state_next_cnt) && (_eos_spi_state_next_buf == NULL)) {
        _eos_spi_state_next_buf = buffer;
    } else {
        if ((_eos_spi_state_flags & SPI_FLAG_RDY) && (_eos_spi_state_flags & SPI_FLAG_CTS)) do_release = spi_xchg_next(buffer);
        if (do_release) rv = spi_bufq_push(buffer);
    }
    set_csr(mstatus, MSTATUS_MIE);

    return rv;
}

int eos_net_send(unsigned char cmd, unsigned char *buffer, uint16_t len) {
    int rv = EOS_OK;

    clear_csr(mstatus, MSTATUS_MIE);
    if ((_eos_spi_state_flags & SPI_FLAG_RDY) && (_eos_spi_state_flags & SPI_FLAG_CTS)) {
        spi_xchg_start(cmd, buffer, len);
    } else {
        rv = eos_msgq_push(&_eos_spi_send_q, cmd, buffer, len);
    }
    set_csr(mstatus, MSTATUS_MIE);

    return rv;
}
