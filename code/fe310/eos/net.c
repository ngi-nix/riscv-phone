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

static EOSMsgQ spi_sndq;
static EOSMsgItem spi_sndq_array[SPI_SIZE_BUFQ];
static eos_evt_fptr_t evt_handler[EOS_NET_MAX_CMD];
static uint16_t evt_handler_wrapper_acq = 0;
static uint16_t evt_handler_wrapper_en = 0;

static SPIBufQ spi_bufq;
static unsigned char spi_bufq_array[SPI_SIZE_BUFQ][SPI_SIZE_BUF];

static int spi_bufq_push(unsigned char *buffer) {
    spi_bufq.array[SPI_BUFQ_IDX_MASK(spi_bufq.idx_w++)] = buffer;
    return EOS_OK;
}

static unsigned char *spi_bufq_pop(void) {
    if (spi_bufq.idx_r == spi_bufq.idx_w) return NULL;
    return spi_bufq.array[SPI_BUFQ_IDX_MASK(spi_bufq.idx_r++)];
}

static SPIState spi_state;

static void spi_reset(void) {
    int i;

    spi_state.flags &= ~SPI_FLAG_CTS;
    spi_state.flags |= SPI_FLAG_RST;
    // before starting a transaction, set SPI peripheral to desired mode
    SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_HOLD;

    while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
    SPI1_REG(SPI_REG_TXFIFO) = 0;

    SPI1_REG(SPI_REG_RXCTRL) = SPI_RXWM(0);
    SPI1_REG(SPI_REG_IE) = SPI_IP_RXWM;
}

static void spi_xchg(unsigned char cmd, unsigned char *buffer, uint16_t len) {
    spi_state.flags &= ~SPI_FLAG_CTS;
    spi_state.flags |= SPI_FLAG_INIT;
    if (spi_state.next_buf == NULL) {
        if (cmd & EOS_NET_CMD_FLAG_ONEW) {
            spi_state.flags |= SPI_FLAG_ONEW;
        } else if (spi_state.next_cnt) {
            cmd |= EOS_NET_CMD_FLAG_ONEW;
        }
    } else if (cmd & EOS_NET_CMD_FLAG_ONEW) {
        cmd &= ~EOS_NET_CMD_FLAG_ONEW;
    }

    spi_state.cmd = cmd;
    spi_state.buf = buffer;
    spi_state.len_tx = len;
    spi_state.len_rx = 0;
    spi_state.idx_tx = 0;
    spi_state.idx_rx = 0;
    
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

    eos_msgq_pop(&spi_sndq, &cmd, &buffer, &len);
    if (cmd) {
        spi_xchg(cmd, buffer, len);
    } else if (spi_state.flags & SPI_FLAG_RTS) {
        if (_buffer == NULL) _buffer = spi_bufq_pop();
        if (_buffer) {
            spi_xchg(0, _buffer, 0);
            return 0;
        }
    }
    return 1;
}

static void spi_xchg_handler(void) {
    volatile uint32_t r1, r2;
    int i;
    
    if (spi_state.flags & SPI_FLAG_RST) {
        while ((r1 = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
        SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
        SPI1_REG(SPI_REG_IE) = 0x0;
        spi_state.flags &= ~SPI_FLAG_RST;

        return;
    } else if (spi_state.flags & SPI_FLAG_INIT) {
        while ((r1 = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
        while ((r2 = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
    
        if (spi_state.cmd & EOS_NET_CMD_FLAG_ONEW) {
            r1 = 0;
            r2 = 0;
        }

        spi_state.cmd = ((r1 & 0xFF) >> 3);
        spi_state.len_rx = ((r1 & 0x07) << 8);
        spi_state.len_rx |= (r2 & 0xFF);
        spi_state.len = MAX(spi_state.len_tx, spi_state.len_rx);

        // Work around esp32 bug
        if (spi_state.len < 6) {
            spi_state.len = 6;
        } else if ((spi_state.len + 2) % 4 != 0) {
            spi_state.len = ((spi_state.len + 2)/4 + 1) * 4 - 2;
        }

        SPI1_REG(SPI_REG_TXCTRL) = SPI_TXWM(SPI_SIZE_CHUNK/2);
        SPI1_REG(SPI_REG_RXCTRL) = SPI_RXWM(SPI_SIZE_CHUNK - 1);
        SPI1_REG(SPI_REG_IE) = SPI_IP_TXWM | SPI_IP_RXWM;
        spi_state.flags &= ~SPI_FLAG_INIT;
    }

    if (SPI1_REG(SPI_REG_IP) & SPI_IP_TXWM) {
        uint16_t sz_chunk = MIN(spi_state.len - spi_state.idx_tx, SPI_SIZE_CHUNK);
        for (i=0; i<sz_chunk; i++) {
            if (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL) break;
            SPI1_REG(SPI_REG_TXFIFO) = spi_state.buf[spi_state.idx_tx+i];
        }
        spi_state.idx_tx += i;
    }
    
    for (i=0; i<spi_state.idx_tx - spi_state.idx_rx; i++) {
        volatile uint32_t x = SPI1_REG(SPI_REG_RXFIFO);
        if (x & SPI_RXFIFO_EMPTY) break;
        spi_state.buf[spi_state.idx_rx+i] = x & 0xFF;
    }
    spi_state.idx_rx += i;

    if (spi_state.idx_rx == spi_state.len) {
        SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
        SPI1_REG(SPI_REG_IE) = 0x0;
        if (spi_state.cmd) {
            int r = eos_evtq_push(EOS_EVT_NET | spi_state.cmd, spi_state.buf, spi_state.len_rx);
            if (r) spi_bufq_push(spi_state.buf);
        } else if ((spi_state.next_cnt || (spi_state.flags & SPI_FLAG_ONEW)) && (spi_state.next_buf == NULL)) {
            spi_state.next_buf = spi_state.buf;
            spi_state.flags &= ~SPI_FLAG_ONEW;
        } else {
            spi_bufq_push(spi_state.buf);
        }
    } else if (spi_state.idx_tx == spi_state.len) {
        SPI1_REG(SPI_REG_RXCTRL) = SPI_RXWM(MIN(spi_state.len - spi_state.idx_rx - 1, SPI_SIZE_CHUNK - 1));
        SPI1_REG(SPI_REG_IE) = SPI_IP_RXWM;
    }
}

static void spi_cts_hanler(void) {
    GPIO_REG(GPIO_RISE_IP) = (0x1 << SPI_GPIO_CTS_OFFSET);
    spi_state.flags |= SPI_FLAG_CTS;

    if (spi_state.flags & SPI_FLAG_RDY) {
        spi_xchg_next(NULL);
    } else {
        uint32_t iof_mask = ((uint32_t)1 << IOF_SPI1_SS2);
        GPIO_REG(GPIO_IOF_EN) &= ~iof_mask;
    }
}

static void spi_rts_hanler(void) {
    uint32_t rts_offset = (0x1 << SPI_GPIO_RTS_OFFSET);
    if (GPIO_REG(GPIO_RISE_IP) & rts_offset) {
        GPIO_REG(GPIO_RISE_IP) = rts_offset;
        spi_state.flags |= SPI_FLAG_RTS;
        if ((spi_state.flags & SPI_FLAG_RDY) && (spi_state.flags & SPI_FLAG_CTS)) spi_reset();
    }
    
    if (GPIO_REG(GPIO_FALL_IP) & rts_offset) {
        GPIO_REG(GPIO_FALL_IP) = rts_offset;
        spi_state.flags &= ~SPI_FLAG_RTS;
    }
}

static void net_handler(unsigned char cmd, unsigned char *buffer, uint16_t len) {
    if ((cmd & ~EOS_EVT_MASK) > EOS_NET_MAX_CMD) {
        eos_evtq_bad_handler(cmd, buffer, len);
    } else {
        unsigned char idx = (cmd & ~EOS_EVT_MASK) - 1;
        uint16_t flag = (uint16_t)1 << idx;
        if (flag & evt_handler_wrapper_en) {
            eos_evtq_handler_wrapper(cmd, buffer, len, &evt_handler_wrapper_acq, flag, evt_handler[idx]);
        } else {
            evt_handler[idx](cmd, buffer, len);
        }
    }
}


void eos_net_init(void) {
    int i;
    
    spi_bufq.idx_r = 0;
    spi_bufq.idx_w = 0;
    for (i=0; i<SPI_SIZE_BUFQ; i++) {
        spi_bufq_push(spi_bufq_array[i]);
    }
    memset(&spi_state, 0, sizeof(spi_state));

    eos_msgq_init(&spi_sndq, spi_sndq_array, SPI_SIZE_BUFQ);
    GPIO_REG(GPIO_IOF_SEL) &= ~SPI_IOF_MASK;
    GPIO_REG(GPIO_IOF_EN) |= SPI_IOF_MASK;
    eos_intr_set(INT_SPI1_BASE, 5, spi_xchg_handler);

    GPIO_REG(GPIO_OUTPUT_EN) &= ~(0x1 << SPI_GPIO_CTS_OFFSET);
    GPIO_REG(GPIO_PULLUP_EN) |= (0x1 << SPI_GPIO_CTS_OFFSET);
    GPIO_REG(GPIO_INPUT_EN) |= (0x1 << SPI_GPIO_CTS_OFFSET);
    GPIO_REG(GPIO_RISE_IE) |= (0x1 << SPI_GPIO_CTS_OFFSET);
    eos_intr_set(INT_GPIO_BASE + SPI_GPIO_CTS_OFFSET, 5, spi_cts_hanler);
    
    GPIO_REG(GPIO_OUTPUT_EN) &= ~(0x1 << SPI_GPIO_RTS_OFFSET);
    GPIO_REG(GPIO_PULLUP_EN) |= (0x1 << SPI_GPIO_RTS_OFFSET);
    GPIO_REG(GPIO_INPUT_EN) |= (0x1 << SPI_GPIO_RTS_OFFSET);
    GPIO_REG(GPIO_RISE_IE) |= (0x1 << SPI_GPIO_RTS_OFFSET);
    GPIO_REG(GPIO_FALL_IE) |= (0x1 << SPI_GPIO_RTS_OFFSET);
    eos_intr_set(INT_GPIO_BASE + SPI_GPIO_RTS_OFFSET, 5, spi_rts_hanler);

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
    spi_state.flags |= SPI_FLAG_RDY;
    if (spi_state.flags & SPI_FLAG_CTS) spi_xchg_next(NULL);
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_net_stop(void) {
    volatile uint8_t done = 0;
    
    clear_csr(mstatus, MSTATUS_MIE);
    spi_state.flags &= ~SPI_FLAG_RDY;
    if (spi_state.flags & SPI_FLAG_CTS) {
        uint32_t iof_mask = ((uint32_t)1 << IOF_SPI1_SS2);
        GPIO_REG(GPIO_IOF_EN) &= ~iof_mask;
        done = 1;
    }
    set_csr(mstatus, MSTATUS_MIE);
    
    while (!done) {
        clear_csr(mstatus, MSTATUS_MIE);
        done = spi_state.flags & SPI_FLAG_CTS;
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

int eos_net_reserve(unsigned char *buffer) {
    int rv = EOS_OK;

    clear_csr(mstatus, MSTATUS_MIE);
    spi_state.next_cnt++;
    if (spi_state.next_buf == NULL) {
        spi_state.next_buf = buffer;
    } else {
        rv = spi_bufq_push(buffer);
    }
    set_csr(mstatus, MSTATUS_MIE);
    
    return rv;
}

int eos_net_acquire(unsigned char reserved) {
    int ret = 0;
    
    if (reserved) {
        while (!ret) {
            clear_csr(mstatus, MSTATUS_MIE);
            if (spi_state.next_buf) {
                ret = 1;
            } else {
                asm volatile ("wfi");
            }
            set_csr(mstatus, MSTATUS_MIE);
        }
    } else {
        clear_csr(mstatus, MSTATUS_MIE);
        spi_state.next_cnt++;
        spi_state.next_buf = spi_bufq_pop();
        ret = (spi_state.next_buf != NULL);
        set_csr(mstatus, MSTATUS_MIE);
    }
    return ret;
}

int eos_net_release(unsigned char reserved) {
    int rv = EOS_OK;

    clear_csr(mstatus, MSTATUS_MIE);
    if (reserved) spi_state.next_cnt--;
    if (!spi_state.next_cnt && spi_state.next_buf) {
        rv = spi_bufq_push((unsigned char *)spi_state.next_buf);
        if (!rv) spi_state.next_buf = NULL;
    }
    set_csr(mstatus, MSTATUS_MIE);
    
    return rv;
}

unsigned char *eos_net_alloc(void) {
    unsigned char *ret = NULL;

    while (ret == NULL) {
        clear_csr(mstatus, MSTATUS_MIE);
        if (spi_state.next_buf) {
            ret = spi_state.next_buf;
            spi_state.next_buf = NULL;
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
    if ((reserve || spi_state.next_cnt) && (spi_state.next_buf == NULL)) {
        spi_state.next_buf = buffer;
    } else {
        if ((spi_state.flags & SPI_FLAG_RDY) && (spi_state.flags & SPI_FLAG_CTS)) do_release = spi_xchg_next(buffer);
        if (do_release) rv = spi_bufq_push(buffer);
    }
    set_csr(mstatus, MSTATUS_MIE);

    return rv;
}

int eos_net_send(unsigned char cmd, unsigned char *buffer, uint16_t len) {
    int rv = EOS_OK;

    clear_csr(mstatus, MSTATUS_MIE);
    if ((spi_state.flags & SPI_FLAG_RDY) && (spi_state.flags & SPI_FLAG_CTS)) {
        spi_xchg(cmd, buffer, len);
    } else {
        rv = eos_msgq_push(&spi_sndq, cmd, buffer, len);
    }
    set_csr(mstatus, MSTATUS_MIE);

    return rv;
}
