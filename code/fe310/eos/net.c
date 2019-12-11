#include <stdlib.h>
#include <stdint.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"
#include "msgq.h"
#include "interrupt.h"
#include "event.h"

#include "spi.h"
#include "spi_def.h"

#include "net.h"
#include "net_def.h"
#include "irq_def.h"

#define MIN(X, Y)               (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)               (((X) > (Y)) ? (X) : (Y))

static EOSBufQ net_buf_q;
static unsigned char *net_bufq_array[EOS_NET_SIZE_BUFQ];
static unsigned char net_bufq_buffer[EOS_NET_SIZE_BUFQ][EOS_NET_SIZE_BUF];

static EOSMsgQ net_send_q;
static EOSMsgItem net_sndq_array[EOS_NET_SIZE_BUFQ];

static uint8_t net_state_flags = 0;
static unsigned char net_state_type = 0;
static uint32_t net_state_len_tx = 0;
static uint32_t net_state_len_rx = 0;

static uint8_t net_state_next_cnt = 0;
static unsigned char *net_state_next_buf = NULL;

static eos_evt_fptr_t evt_handler[EOS_NET_MAX_MTYPE];
static uint16_t evt_handler_flags_buf_free = 0;
static uint16_t evt_handler_flags_buf_acq = 0;

extern uint32_t _eos_spi_state_len;
extern uint32_t _eos_spi_state_idx_tx;
extern uint32_t _eos_spi_state_idx_rx;
extern unsigned char *_eos_spi_state_buf;

static void net_xchg_reset(void) {
    net_state_flags &= ~NET_STATE_FLAG_CTS;
    net_state_flags |= (NET_STATE_FLAG_RST | NET_STATE_FLAG_XCHG);

    // before starting a transaction, set SPI peripheral to desired mode
    SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_HOLD;

    SPI1_REG(SPI_REG_TXFIFO) = 0;

    SPI1_REG(SPI_REG_RXCTRL) = SPI_RXWM(0);
    SPI1_REG(SPI_REG_IE) = SPI_IP_RXWM;
}

static void net_xchg_start(unsigned char type, unsigned char *buffer, uint16_t len) {
    net_state_flags &= ~NET_STATE_FLAG_CTS;
    net_state_flags |= (NET_STATE_FLAG_INIT | NET_STATE_FLAG_XCHG);

    if (net_state_next_cnt && (net_state_next_buf == NULL)) type |= NET_MTYPE_FLAG_ONEW;
    if (type & NET_MTYPE_FLAG_ONEW) net_state_flags |= NET_STATE_FLAG_ONEW;

    net_state_type = type;
    net_state_len_tx = len;
    net_state_len_rx = 0;

    _eos_spi_state_buf = buffer;

    // before starting a transaction, set SPI peripheral to desired mode
    SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_HOLD;

    SPI1_REG(SPI_REG_TXFIFO) = ((type << 3) | (len >> 8)) & 0xFF;
    SPI1_REG(SPI_REG_TXFIFO) = len & 0xFF;

    SPI1_REG(SPI_REG_RXCTRL) = SPI_RXWM(1);
    SPI1_REG(SPI_REG_IE) = SPI_IP_RXWM;
}

static int net_xchg_next(unsigned char *_buffer) {
    unsigned char type;
    unsigned char *buffer = NULL;
    uint16_t len;

    eos_msgq_pop(&net_send_q, &type, &buffer, &len);
    if (type) {
        net_xchg_start(type, buffer, len);
    } else if (net_state_flags & NET_STATE_FLAG_RTS) {
        if (_buffer == NULL) _buffer = eos_bufq_pop(&net_buf_q);
        if (_buffer) {
            net_xchg_start(0, _buffer, 0);
            return 0;
        }
    }
    return 1;
}

void eos_net_xchg_done(void) {
    SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
    if (net_state_type) {
        int r = eos_evtq_push_isr(EOS_EVT_NET | net_state_type, _eos_spi_state_buf, net_state_len_rx);
        if (r) eos_bufq_push(&net_buf_q, _eos_spi_state_buf);
    } else if (((net_state_flags & NET_STATE_FLAG_ONEW) || net_state_next_cnt) && (net_state_next_buf == NULL)) {
        net_state_next_buf = _eos_spi_state_buf;
        net_state_flags &= ~NET_STATE_FLAG_ONEW;
    } else {
        eos_bufq_push(&net_buf_q, _eos_spi_state_buf);
    }
    net_state_flags &= ~NET_STATE_FLAG_XCHG;
}

static void net_handler_xchg(void) {
    volatile uint32_t r1, r2;

    if (net_state_flags & NET_STATE_FLAG_RST) {
        net_state_flags &= ~NET_STATE_FLAG_RST;

        r1 = SPI1_REG(SPI_REG_RXFIFO);
        SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
        SPI1_REG(SPI_REG_IE) = 0x0;

        return;
    } else if (net_state_flags & NET_STATE_FLAG_INIT) {
        net_state_flags &= ~NET_STATE_FLAG_INIT;
        SPI1_REG(SPI_REG_TXCTRL) = SPI_TXWM(SPI_SIZE_WM);
        SPI1_REG(SPI_REG_IE) = SPI_IP_TXWM;

        r1 = SPI1_REG(SPI_REG_RXFIFO);
        r2 = SPI1_REG(SPI_REG_RXFIFO);

        if (net_state_flags & NET_STATE_FLAG_ONEW) {
            r1 = 0;
            r2 = 0;
        }

        net_state_type = ((r1 & 0xFF) >> 3);
        net_state_len_rx = ((r1 & 0x07) << 8);
        net_state_len_rx |= (r2 & 0xFF);
        _eos_spi_state_len = MAX(net_state_len_tx, net_state_len_rx);
        _eos_spi_state_idx_tx = 0;
        _eos_spi_state_idx_rx = 0;

        // Work around esp32 bug
        if (_eos_spi_state_len < 6) {
            _eos_spi_state_len = 6;
        } else if ((_eos_spi_state_len + 2) % 4 != 0) {
            _eos_spi_state_len = ((_eos_spi_state_len + 2)/4 + 1) * 4 - 2;
        }

        if (_eos_spi_state_len > EOS_NET_SIZE_BUF) {
            SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
            SPI1_REG(SPI_REG_IE) = 0x0;
        }

        return;
    }

    eos_spi_xchg_handler();
}

static void net_handler_cts(void) {
    GPIO_REG(GPIO_RISE_IP) = (1 << NET_PIN_CTS);
    net_state_flags |= NET_STATE_FLAG_CTS;

    if (net_state_flags & NET_STATE_FLAG_RUN) {
        net_xchg_next(NULL);
    }
}

static void net_handler_rts(void) {
    uint32_t rts_offset = (1 << NET_PIN_RTS);
    if (GPIO_REG(GPIO_RISE_IP) & rts_offset) {
        GPIO_REG(GPIO_RISE_IP) = rts_offset;
        net_state_flags |= NET_STATE_FLAG_RTS;
        if ((net_state_flags & NET_STATE_FLAG_RUN) && (net_state_flags & NET_STATE_FLAG_CTS)) net_xchg_reset();
    } else if (GPIO_REG(GPIO_FALL_IP) & rts_offset) {
        GPIO_REG(GPIO_FALL_IP) = rts_offset;
        net_state_flags &= ~NET_STATE_FLAG_RTS;
    }
}

static void net_handler_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    unsigned char idx = (type & ~EOS_EVT_MASK) - 1;

    if (idx >= EOS_NET_MAX_MTYPE) {
        eos_evtq_bad_handler(type, buffer, len);
        eos_net_free(buffer, 0);
        return;
    }
    _eos_net_handle(type, buffer, len, idx, evt_handler, &evt_handler_flags_buf_free, &evt_handler_flags_buf_acq);
}

void eos_net_init(void) {
    int i;

    eos_msgq_init(&net_send_q, net_sndq_array, EOS_NET_SIZE_BUFQ);
    eos_bufq_init(&net_buf_q, net_bufq_array, EOS_NET_SIZE_BUFQ);
    for (i=0; i<EOS_NET_SIZE_BUFQ; i++) {
        eos_bufq_push(&net_buf_q, net_bufq_buffer[i]);
    }

    for (i=0; i<EOS_NET_MAX_MTYPE; i++) {
        evt_handler[i] = eos_evtq_bad_handler;
    }
    eos_evtq_set_handler(EOS_EVT_NET, net_handler_evt);

    GPIO_REG(GPIO_INPUT_EN)     |=  (1 << NET_PIN_CTS);
    GPIO_REG(GPIO_OUTPUT_EN)    &= ~(1 << NET_PIN_CTS);
    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << NET_PIN_CTS);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << NET_PIN_CTS);

    GPIO_REG(GPIO_RISE_IE)      |=  (1 << NET_PIN_CTS);
    eos_intr_set(INT_GPIO_BASE + NET_PIN_CTS, IRQ_PRIORITY_NET_CTS, net_handler_cts);

    GPIO_REG(GPIO_INPUT_EN)     |=  (1 << NET_PIN_RTS);
    GPIO_REG(GPIO_OUTPUT_EN)    &= ~(1 << NET_PIN_RTS);
    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << NET_PIN_RTS);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << NET_PIN_RTS);

    GPIO_REG(GPIO_RISE_IE)      |=  (1 << NET_PIN_RTS);
    GPIO_REG(GPIO_FALL_IE)      |=  (1 << NET_PIN_RTS);
    eos_intr_set(INT_GPIO_BASE + NET_PIN_RTS, IRQ_PRIORITY_NET_RTS, net_handler_rts);
}

void eos_net_start(void) {
    eos_intr_set_handler(INT_SPI1_BASE, net_handler_xchg);
    SPI1_REG(SPI_REG_SCKDIV) = SPI_DIV_NET;
    SPI1_REG(SPI_REG_CSID) = SPI_CS_IDX_NET;

    net_state_flags |= NET_STATE_FLAG_RUN;
    if (net_state_flags & NET_STATE_FLAG_CTS) {
        clear_csr(mstatus, MSTATUS_MIE);
        net_xchg_next(NULL);
        set_csr(mstatus, MSTATUS_MIE);
    }
}

void eos_net_stop(void) {
    uint8_t done = 0;

    if (!(net_state_flags & NET_STATE_FLAG_RUN)) return;

    while (!done) {
        clear_csr(mstatus, MSTATUS_MIE);
        net_state_flags &= ~NET_STATE_FLAG_RUN;
        done = !(net_state_flags & NET_STATE_FLAG_XCHG);
        if (!done) asm volatile ("wfi");
        set_csr(mstatus, MSTATUS_MIE);
    }
}

void _eos_net_handle(unsigned char type, unsigned char *buffer, uint16_t len, unsigned char idx, eos_evt_fptr_t handlers[], uint16_t *flags_buf_free, uint16_t *flags_buf_acq) {
    uint16_t buf_free = ((uint16_t)1 << idx) & *flags_buf_free;
    uint16_t buf_acq = ((uint16_t)1 << idx) & *flags_buf_acq;

    if (buf_free) {
        eos_net_free(buffer, buf_acq);
        buffer = NULL;
        len = 0;
    }

    handlers[idx](type, buffer, len);

    if (buf_free && buf_acq) eos_net_release();
}

void _eos_net_set_handler(unsigned char idx, eos_evt_fptr_t handler, eos_evt_fptr_t handlers[], uint16_t flags, uint16_t *flags_buf_free, uint16_t *flags_buf_acq) {
    if (flags) {
        uint16_t flag = (uint16_t)1 << idx;
        if (flags & EOS_NET_FLAG_BFREE) *flags_buf_free |= flag;
        if (flags & EOS_NET_FLAG_BACQ) *flags_buf_acq |= flag;
    }
    handlers[idx] = handler;
}

void eos_net_set_handler(unsigned char mtype, eos_evt_fptr_t handler, uint8_t flags) {
    if (mtype && (mtype <= EOS_NET_MAX_MTYPE)) {
        mtype--;
    } else {
        return;
    }
    _eos_net_set_handler(mtype, handler, evt_handler, flags, &evt_handler_flags_buf_free, &evt_handler_flags_buf_acq);
}

int _eos_net_acquire(unsigned char reserved) {
    int ret = 0;

    if (reserved) {
        while (!ret) {
            clear_csr(mstatus, MSTATUS_MIE);
            if (net_state_next_buf) {
                ret = 1;
                net_state_next_cnt--;
            } else {
                asm volatile ("wfi");
            }
            set_csr(mstatus, MSTATUS_MIE);
        }
    } else {
        clear_csr(mstatus, MSTATUS_MIE);
        if (net_state_next_buf == NULL) net_state_next_buf = eos_bufq_pop(&net_buf_q);
        ret = (net_state_next_buf != NULL);
        if (!ret) net_state_next_cnt++;
        set_csr(mstatus, MSTATUS_MIE);
    }
    return ret;
}

void eos_net_acquire(void) {
    unsigned char acq = _eos_net_acquire(0);
    if (!acq) _eos_net_acquire(1);
}

void eos_net_release(void) {
    clear_csr(mstatus, MSTATUS_MIE);
    if (!net_state_next_cnt && net_state_next_buf) {
        eos_bufq_push(&net_buf_q, net_state_next_buf);
        net_state_next_buf = NULL;
    }
    set_csr(mstatus, MSTATUS_MIE);
}

unsigned char *eos_net_alloc(void) {
    unsigned char *ret = NULL;

    clear_csr(mstatus, MSTATUS_MIE);
    ret = net_state_next_buf;
    net_state_next_buf = NULL;
    set_csr(mstatus, MSTATUS_MIE);

    return ret;
}

void eos_net_free(unsigned char *buffer, unsigned char more) {
    uint8_t do_release = 1;

    clear_csr(mstatus, MSTATUS_MIE);
    if ((more || net_state_next_cnt) && (net_state_next_buf == NULL)) {
        net_state_next_buf = buffer;
    } else {
        if ((net_state_flags & NET_STATE_FLAG_RUN) && (net_state_flags & NET_STATE_FLAG_CTS)) do_release = net_xchg_next(buffer);
        if (do_release) eos_bufq_push(&net_buf_q, buffer);
    }
    set_csr(mstatus, MSTATUS_MIE);
}

int eos_net_send(unsigned char type, unsigned char *buffer, uint16_t len, unsigned char more) {
    int rv = EOS_OK;

    if (more) {
        type |= NET_MTYPE_FLAG_ONEW;
    }
    clear_csr(mstatus, MSTATUS_MIE);
    if ((net_state_flags & NET_STATE_FLAG_RUN) && (net_state_flags & NET_STATE_FLAG_CTS)) {
        net_xchg_start(type, buffer, len);
    } else {
        rv = eos_msgq_push(&net_send_q, type, buffer, len);
        if (rv) eos_bufq_push(&net_buf_q, buffer);
    }
    set_csr(mstatus, MSTATUS_MIE);

    return rv;
}
