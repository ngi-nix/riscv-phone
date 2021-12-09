#include <stdlib.h>
#include <stdint.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"
#include "msgq.h"
#include "interrupt.h"
#include "event.h"
#include "timer.h"
#include "pwr.h"

#include "board.h"

#include "spi.h"
#include "spi_priv.h"
#include "spi_dev.h"

#include "net.h"

#define NET_SIZE_HDR            3
#define NET_STATE_FLAG_RUN      0x01
#define NET_STATE_FLAG_INIT     0x02
#define NET_STATE_FLAG_XCHG     0x04
#define NET_STATE_FLAG_ONEW     0x10
#define NET_STATE_FLAG_REPW     0x20
#define NET_STATE_FLAG_RTS      0x40
#define NET_STATE_FLAG_CTS      0x80

#define MIN(X, Y)               (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)               (((X) > (Y)) ? (X) : (Y))

static EOSBufQ net_buf_q;
static unsigned char *net_bufq_array[EOS_NET_SIZE_BUFQ];
static unsigned char net_bufq_buffer[EOS_NET_SIZE_BUFQ][EOS_NET_SIZE_BUF];

static EOSMsgQ net_send_q;
static EOSMsgItem net_sndq_array[EOS_NET_SIZE_BUFQ];

static volatile uint8_t net_state_flags = 0;
static unsigned char net_state_type = 0;
static uint32_t net_state_len_tx = 0;
static uint32_t net_state_len_rx = 0;
unsigned char *net_state_buf = NULL;

static uint8_t net_state_next_cnt = 0;
static unsigned char *net_state_next_buf = NULL;

static eos_evt_handler_t net_handler[EOS_NET_MAX_MTYPE];
static uint16_t net_wrapper_acq[EOS_EVT_MAX_EVT];
static uint16_t net_flags_acq[EOS_EVT_MAX_EVT];

static int net_xchg_sleep(void) {
    int i;
    int rv = EOS_OK;
    volatile uint32_t x = 0;
    net_state_flags &= ~NET_STATE_FLAG_CTS;

    SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_HOLD;

    SPI1_REG(SPI_REG_TXFIFO) = 0xFF;
    while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
    if (x & 0xFF) rv = EOS_ERR_BUSY;

    for (i=0; i<7; i++) {
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = 0;
        while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
    }

    SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;

    return rv;
}

static void net_xchg_wake(void) {
    int i;
    volatile uint32_t x = 0;
    net_state_flags &= ~NET_STATE_FLAG_CTS;

    SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_HOLD;

    for (i=0; i<8; i++) {
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = 0;
        while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
    }

    SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
}

static void net_xchg_reset(void) {
    volatile uint32_t x = 0;
    net_state_flags &= ~NET_STATE_FLAG_CTS;

    SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_HOLD;

    SPI1_REG(SPI_REG_TXFIFO) = 0;
    while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);

    SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
}

static void net_xchg_start(unsigned char type, unsigned char *buffer, uint16_t len) {
    net_state_flags &= ~NET_STATE_FLAG_CTS;
    net_state_flags |= (NET_STATE_FLAG_INIT | NET_STATE_FLAG_XCHG);

    if (net_state_next_cnt && (net_state_next_buf == NULL)) type |= EOS_NET_MTYPE_FLAG_ONEW;
    if (type & EOS_NET_MTYPE_FLAG_ONEW) net_state_flags |= NET_STATE_FLAG_ONEW;
    if (type & EOS_NET_MTYPE_FLAG_REPW) net_state_flags |= NET_STATE_FLAG_REPW;

    net_state_type = type;
    net_state_len_tx = len;
    net_state_len_rx = 0;
    net_state_buf = buffer;

    SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_HOLD;
    SPI1_REG(SPI_REG_TXFIFO) = type;
    SPI1_REG(SPI_REG_TXFIFO) = (len >> 8) & 0xFF;
    SPI1_REG(SPI_REG_TXFIFO) = (len & 0xFF);
    SPI1_REG(SPI_REG_RXCTRL) = SPI_RXWM(2);
    SPI1_REG(SPI_REG_IE) = SPI_IP_RXWM;
}

static int net_xchg_next(unsigned char *_buffer) {
    unsigned char type;
    unsigned char *buffer = NULL;
    uint16_t len;
    int ret = _buffer ? 1 : 0;

    eos_msgq_pop(&net_send_q, &type, &buffer, &len);
    if (type) {
        net_xchg_start(type, buffer, len);
    } else if (net_state_flags & NET_STATE_FLAG_RTS) {
        if (_buffer) {
            buffer = _buffer;
            ret = 0;
        } else {
            buffer = eos_bufq_pop(&net_buf_q);
        }
        if (buffer) net_xchg_start(0, buffer, 0);
    }

    return ret;
}

static void net_handle_xchg(void) {
    volatile uint32_t r1, r2, r3;
    uint32_t len;

    if (net_state_flags & NET_STATE_FLAG_INIT) {
        net_state_flags &= ~NET_STATE_FLAG_INIT;

        r1 = SPI1_REG(SPI_REG_RXFIFO);
        r2 = SPI1_REG(SPI_REG_RXFIFO);
        r3 = SPI1_REG(SPI_REG_RXFIFO);

        if (net_state_flags & (NET_STATE_FLAG_ONEW | NET_STATE_FLAG_REPW)) {
            r1 = 0;
            r2 = 0;
            r3 = 0;
        }

        net_state_type    = (r1 & 0xFF);
        net_state_len_rx  = (r2 & 0xFF) << 8;
        net_state_len_rx |= (r3 & 0xFF);
        len = MAX(net_state_len_tx, net_state_len_rx);

        if (len > EOS_NET_MTU) {
            net_state_flags &= ~NET_STATE_FLAG_XCHG;
            SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
            SPI1_REG(SPI_REG_IE) = 0x0;
            return;
        }

        // esp32 dma workaraund
        if (len < 8 - NET_SIZE_HDR) {
            len = 8 - NET_SIZE_HDR;
        } else if ((len + NET_SIZE_HDR) % 4 != 0) {
            len = ((len + NET_SIZE_HDR)/4 + 1) * 4 - NET_SIZE_HDR;
        }

        _eos_spi_xchg_init(net_state_buf, len, 0);
        SPI1_REG(SPI_REG_TXCTRL) = SPI_TXWM(SPI_SIZE_WM);
        SPI1_REG(SPI_REG_IE) = SPI_IP_TXWM;
        return;
    }

    eos_spi_handle_xchg();
    if (SPI1_REG(SPI_REG_CSMODE) == SPI_CSMODE_AUTO) {  // exchange done
        if (net_state_flags & NET_STATE_FLAG_REPW) {
            net_state_flags &= ~NET_STATE_FLAG_REPW;
        } else {
            if (net_state_type) {
                int r = eos_evtq_push_isr(EOS_EVT_NET | net_state_type, net_state_buf, net_state_len_rx);
                if (r) eos_bufq_push(&net_buf_q, net_state_buf);
            } else if (((net_state_flags & NET_STATE_FLAG_ONEW) || net_state_next_cnt) && (net_state_next_buf == NULL)) {
                net_state_next_buf = net_state_buf;
                net_state_flags &= ~NET_STATE_FLAG_ONEW;
            } else {
                eos_bufq_push(&net_buf_q, net_state_buf);
            }
        }
        net_state_flags &= ~NET_STATE_FLAG_XCHG;
    }
}

static void net_handle_cts(void) {
    GPIO_REG(GPIO_RISE_IP) = (1 << NET_PIN_CTS);
    net_state_flags |= NET_STATE_FLAG_CTS;

    if (net_state_flags & NET_STATE_FLAG_RUN) {
        net_xchg_next(NULL);
    }
}

static void net_handle_rts(void) {
    uint32_t rts_offset = (1 << NET_PIN_RTS);
    if (GPIO_REG(GPIO_RISE_IP) & rts_offset) {
        GPIO_REG(GPIO_RISE_IP) = rts_offset;
        net_state_flags |= NET_STATE_FLAG_RTS;
        if ((net_state_flags & NET_STATE_FLAG_RUN) && (net_state_flags & NET_STATE_FLAG_CTS)) {
            net_xchg_reset();
        }
    } else if (GPIO_REG(GPIO_FALL_IP) & rts_offset) {
        GPIO_REG(GPIO_FALL_IP) = rts_offset;
        net_state_flags &= ~NET_STATE_FLAG_RTS;
    }
}

static void net_handle_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    unsigned char idx = (type & ~EOS_EVT_MASK) - 1;

    if (idx < EOS_NET_MAX_MTYPE) {
        net_handler[idx](type, buffer, len);
    } else {
        eos_net_bad_handler(type, buffer, len);
    }
}

static int net_acquire(unsigned char reserved) {
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

static void evt_handler_wrapper(unsigned char type, unsigned char *buffer, uint16_t len, unsigned char idx, uint16_t flag) {
    int ok;

    ok = net_acquire(net_wrapper_acq[idx] & flag);
    if (ok) {
        eos_evtq_get_handler(type)(type, buffer, len);
        eos_net_release();
        net_wrapper_acq[idx] &= ~flag;
    } else {
        net_wrapper_acq[idx] |= flag;
        eos_evtq_push(type, buffer, len);
    }
}

static void evt_handler(unsigned char type, unsigned char *buffer, uint16_t len) {
    unsigned char idx = (type & EOS_EVT_MASK) >> 4;

    if (idx && (idx <= EOS_EVT_MAX_EVT)) {
        uint16_t flag = (uint16_t)1 << (type & ~EOS_EVT_MASK);

        idx--;
        if (flag & net_flags_acq[idx]) {
            evt_handler_wrapper(type, buffer, len, idx, flag);
        } else {
            eos_evtq_get_handler(type)(type, buffer, len);
        }
    } else {
        eos_evtq_bad_handler(type, buffer, len);
    }
}

static void net_pause(void) {
    net_state_flags &= ~NET_STATE_FLAG_RUN;
}

static void net_resume(void) {
    net_state_flags |= NET_STATE_FLAG_RUN;
    if (net_state_flags & NET_STATE_FLAG_CTS) {
        net_xchg_next(NULL);
    }
}

static void net_start(void) {
    eos_intr_set_handler(INT_SPI1_BASE, net_handle_xchg);
    SPI1_REG(SPI_REG_SCKDIV) = eos_spi_div(EOS_SPI_DEV_NET);
    SPI1_REG(SPI_REG_CSID) = eos_spi_csid(EOS_SPI_DEV_NET);
}

static void net_stop(void) {
    eos_intr_set_handler(INT_SPI1_BASE, NULL);
}

int eos_net_init(uint8_t wakeup_cause) {
    int i;

    eos_msgq_init(&net_send_q, net_sndq_array, EOS_NET_SIZE_BUFQ);
    eos_bufq_init(&net_buf_q, net_bufq_array, EOS_NET_SIZE_BUFQ);
    for (i=0; i<EOS_NET_SIZE_BUFQ; i++) {
        eos_bufq_push(&net_buf_q, net_bufq_buffer[i]);
    }

    for (i=0; i<EOS_NET_MAX_MTYPE; i++) {
        net_handler[i] = eos_net_bad_handler;
    }
    eos_evtq_set_handler(0, evt_handler);
    eos_evtq_set_handler(EOS_EVT_NET, net_handle_evt);

    GPIO_REG(GPIO_INPUT_EN)     |=  (1 << NET_PIN_CTS);
    GPIO_REG(GPIO_OUTPUT_EN)    &= ~(1 << NET_PIN_CTS);

    GPIO_REG(GPIO_RISE_IE)      |=  (1 << NET_PIN_CTS);
    eos_intr_set(INT_GPIO_BASE + NET_PIN_CTS, IRQ_PRIORITY_NET_CTS, net_handle_cts);

    GPIO_REG(GPIO_INPUT_EN)     |=  (1 << NET_PIN_RTS);
    GPIO_REG(GPIO_OUTPUT_EN)    &= ~(1 << NET_PIN_RTS);

    GPIO_REG(GPIO_RISE_IE)      |=  (1 << NET_PIN_RTS);
    GPIO_REG(GPIO_FALL_IE)      |=  (1 << NET_PIN_RTS);
    eos_intr_set(INT_GPIO_BASE + NET_PIN_RTS, IRQ_PRIORITY_NET_RTS, net_handle_rts);

    /* set initial state */
    clear_csr(mstatus, MSTATUS_MIE);
    if (GPIO_REG(GPIO_INPUT_VAL) & (1 << NET_PIN_CTS)) net_state_flags |= NET_STATE_FLAG_CTS;
    if (GPIO_REG(GPIO_INPUT_VAL) & (1 << NET_PIN_RTS)) net_state_flags |= NET_STATE_FLAG_RTS;
    set_csr(mstatus, MSTATUS_MIE);

    return EOS_OK;
}

int eos_net_run(uint8_t wakeup_cause) {
    net_start();

    clear_csr(mstatus, MSTATUS_MIE);
    if (wakeup_cause != EOS_PWR_WAKE_RST) {
        if (wakeup_cause != EOS_PWR_WAKE_BTN) {
            net_xchg_wake();
        }
        if (!(net_state_flags & NET_STATE_FLAG_CTS)) {
            while (!(GPIO_REG(GPIO_RISE_IP) & (1 << NET_PIN_CTS))) {
                asm volatile ("wfi");
            }
            GPIO_REG(GPIO_RISE_IP) = (1 << NET_PIN_CTS);
        }
        net_xchg_reset();
    }
    net_resume();
    set_csr(mstatus, MSTATUS_MIE);

    return EOS_OK;
}

void eos_net_start(void) {
    net_start();

    clear_csr(mstatus, MSTATUS_MIE);
    net_resume();
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_net_stop(void) {
    uint8_t done = 0;

    clear_csr(mstatus, MSTATUS_MIE);
    if (net_state_flags & NET_STATE_FLAG_RUN) {
        net_state_flags &= ~NET_STATE_FLAG_RUN;
        done = !(net_state_flags & NET_STATE_FLAG_XCHG);
    } else {
        done = 1;
    }
    set_csr(mstatus, MSTATUS_MIE);

    while (!done) {
        clear_csr(mstatus, MSTATUS_MIE);
        done = !(net_state_flags & NET_STATE_FLAG_XCHG);
        if (!done) asm volatile ("wfi");
        set_csr(mstatus, MSTATUS_MIE);
    }
    net_stop();
}

int eos_net_sleep(uint32_t timeout) {
    volatile uint64_t *mtime = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIME);
    uint64_t then_ms = timeout + *mtime * 1000 / EOS_TIMER_RTC_FREQ;
    uint8_t done = 0;
    int rv = EOS_OK;

    clear_csr(mstatus, MSTATUS_MIE);
    if (!(net_state_flags & NET_STATE_FLAG_RUN)) rv = EOS_ERR;
    set_csr(mstatus, MSTATUS_MIE);

    if (rv) return rv;

    do {
        if (*mtime * 1000 / EOS_TIMER_RTC_FREQ > then_ms) return EOS_ERR_TIMEOUT;
        clear_csr(mstatus, MSTATUS_MIE);
        eos_evtq_flush_isr();
        done = (eos_msgq_len(&net_send_q) == 0);
        done = done && (!(net_state_flags & NET_STATE_FLAG_RTS) && (net_state_flags & NET_STATE_FLAG_CTS));
        if (done) done = (net_xchg_sleep() == EOS_OK);
        if (!done) {
            asm volatile ("wfi");
            set_csr(mstatus, MSTATUS_MIE);
        }
    } while (!done);

    while (!(GPIO_REG(GPIO_RISE_IP) & (1 << NET_PIN_CTS))) {
        if (*mtime * 1000 / EOS_TIMER_RTC_FREQ > then_ms) {
            rv = EOS_ERR_TIMEOUT;
            break;
        }
        asm volatile ("wfi");
    }

    if (!rv) {
        GPIO_REG(GPIO_RISE_IP) = (1 << NET_PIN_CTS);
        net_state_flags &= ~NET_STATE_FLAG_RUN;
    }

    set_csr(mstatus, MSTATUS_MIE);

    return rv;
}

void eos_net_bad_handler(unsigned char type, unsigned char *buffer, uint16_t len) {
    eos_evtq_bad_handler(type, buffer, len);
    if (buffer) eos_net_free(buffer, 0);
}

void eos_net_set_handler(unsigned char mtype, eos_evt_handler_t handler) {
    if (handler == NULL) handler = eos_net_bad_handler;
    if (mtype && (mtype <= EOS_NET_MAX_MTYPE)) net_handler[mtype - 1] = handler;
}

void eos_net_acquire_for_evt(unsigned char type, char acq) {
    unsigned char idx = (type & EOS_EVT_MASK) >> 4;
    uint16_t flag = type & ~EOS_EVT_MASK ? (uint16_t)1 << (type & ~EOS_EVT_MASK) : 0xFFFF;

    if (idx && (idx <= EOS_EVT_MAX_EVT)) {
        idx--;
        net_flags_acq[idx] &= ~flag;
        if (acq) net_flags_acq[idx] |= flag;
    }
}

void eos_net_acquire(void) {
    unsigned char acq = net_acquire(0);
    if (!acq) net_acquire(1);
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

    while (!ret) {
        clear_csr(mstatus, MSTATUS_MIE);
        if (net_state_next_buf) {
            ret = net_state_next_buf;
            net_state_next_buf = NULL;
        } else {
            asm volatile ("wfi");
        }
        set_csr(mstatus, MSTATUS_MIE);
    }

    return ret;
}

void eos_net_free(unsigned char *buffer, unsigned char more) {
    uint8_t do_release = 1;

    clear_csr(mstatus, MSTATUS_MIE);
    if ((more || net_state_next_cnt) && (net_state_next_buf == NULL)) {
        net_state_next_buf = buffer;
    } else {
        if ((net_state_flags & NET_STATE_FLAG_RUN) && (net_state_flags & NET_STATE_FLAG_CTS)) {
            do_release = net_xchg_next(buffer);
        }
        if (do_release) {
            eos_bufq_push(&net_buf_q, buffer);
        }
    }
    set_csr(mstatus, MSTATUS_MIE);
}

static int net_xchg(unsigned char *type, unsigned char *buffer, uint16_t *len) {
    int rv = EOS_OK;
    int _sync = 0;
    unsigned char _type = *type;
    uint16_t _len = *len;
    uint8_t spi_dev = EOS_SPI_DEV_NET;

    clear_csr(mstatus, MSTATUS_MIE);
    if ((_type & EOS_NET_MTYPE_FLAG_REPW) || ((_type & EOS_NET_MTYPE_FLAG_ONEW) && !(net_state_flags & NET_STATE_FLAG_RUN))) _sync = 1;

    if (!(net_state_flags & NET_STATE_FLAG_RUN) && _sync) {
        int _rv;

        set_csr(mstatus, MSTATUS_MIE);
        spi_dev = eos_spi_dev();
        _rv = eos_spi_deselect();
        if (_rv) return _rv;
        clear_csr(mstatus, MSTATUS_MIE);
    }

    if (_sync) {
        net_pause();
        while (!(net_state_flags & NET_STATE_FLAG_CTS)) {
            asm volatile ("wfi");
            set_csr(mstatus, MSTATUS_MIE);
            clear_csr(mstatus, MSTATUS_MIE);
        }
        net_xchg_start(_type, buffer, _len);
        if (_type & EOS_NET_MTYPE_FLAG_REPW) {
            while (!(net_state_flags & NET_STATE_FLAG_CTS)) {
                asm volatile ("wfi");
                set_csr(mstatus, MSTATUS_MIE);
                clear_csr(mstatus, MSTATUS_MIE);
            }
            net_xchg_start(0, buffer, 0);
            while (net_state_flags & NET_STATE_FLAG_XCHG) {
                asm volatile ("wfi");
                set_csr(mstatus, MSTATUS_MIE);
                clear_csr(mstatus, MSTATUS_MIE);
            }
            *type = net_state_type;
            *len = net_state_len_rx;
        }
        net_resume();
    } else {
        if ((net_state_flags & NET_STATE_FLAG_RUN) && (net_state_flags & NET_STATE_FLAG_CTS)) {
            net_xchg_start(_type, buffer, _len);
        } else {
            rv = eos_msgq_push(&net_send_q, _type, buffer, _len);
            if (rv) eos_bufq_push(&net_buf_q, buffer);
        }
    }

    set_csr(mstatus, MSTATUS_MIE);
    if (spi_dev != EOS_SPI_DEV_NET) eos_spi_select(spi_dev);

    return rv;
}

int eos_net_send(unsigned char type, unsigned char *buffer, uint16_t len, unsigned char more) {
    if (more) type |= EOS_NET_MTYPE_FLAG_ONEW;
    return net_xchg(&type, buffer, &len);
}

int eos_net_xchg(unsigned char *type, unsigned char *buffer, uint16_t *len) {
    *type |= EOS_NET_MTYPE_FLAG_REPW;
    return net_xchg(type, buffer, len);
}
