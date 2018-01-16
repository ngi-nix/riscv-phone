#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "encoding.h"
#include "platform.h"
#include "plic/plic_driver.h"

#include "eos.h"
#include "msgq.h"
#include "event.h"
#include "interrupt.h"
#include "net.h"
#include "spi.h"

#define MIN(X, Y)               (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)               (((X) > (Y)) ? (X) : (Y))
#define SPI_BUFQ_IDX_MASK(IDX)  ((IDX) & (SPI_SIZE_BUFQ - 1))

static volatile uint8_t spi_res = 0;
static volatile uint8_t spi_res_next = 0;
static volatile unsigned char *spi_res_buf = NULL;

static EOSMsgQ spi_sndq;
static EOSMsgItem spi_sndq_array[SPI_SIZE_BUFQ];

static SPIBufQ spi_bufq;
static unsigned char spi_bufq_array[SPI_SIZE_BUFQ][SPI_SIZE_BUF];

static int spi_bufq_push(unsigned char *buffer) {
    spi_bufq.array[SPI_BUFQ_IDX_MASK(spi_bufq.idx_w++)] = buffer;
}

static unsigned char *spi_bufq_pop(void) {
    if (spi_bufq.idx_r == spi_bufq.idx_w) return NULL;
    return spi_bufq.array[SPI_BUFQ_IDX_MASK(spi_bufq.idx_r++)];
}

static volatile uint8_t spi_rdy = 0;
static volatile uint8_t spi_cts = 0;
static volatile uint8_t spi_rts = 0;
static SPIBuffer spi_buffer;

static void spi_reset(void) {
    int i;
    volatile uint32_t r;

    spi_cts = 0;
    // before starting a transaction, set SPI peripheral to desired mode
    SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_HOLD;

    while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
    SPI1_REG(SPI_REG_TXFIFO) = 0;
    while ((r = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);

    SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
}

static void spi_xchg(unsigned char cmd, unsigned char *buffer, uint16_t len) {
    volatile uint32_t r1, r2;

    spi_cts = 0;
    spi_buffer.buffer = buffer;
    spi_buffer.len_rx = 0;
    spi_buffer.idx_tx = 0;
    spi_buffer.idx_rx = 0;
    
    if (spi_res_buf == NULL) {
        if (cmd & EOS_NET_CMD_FLAG_ONEW) {
            spi_res_next = 1;
        } else if (spi_res) {
            cmd |= EOS_NET_CMD_FLAG_ONEW;
        }
    } else if (cmd & EOS_NET_CMD_FLAG_ONEW) {
        cmd &= ~EOS_NET_CMD_FLAG_ONEW;
    }

    // before starting a transaction, set SPI peripheral to desired mode
    SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_HOLD;

    while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
    SPI1_REG(SPI_REG_TXFIFO) = ((cmd << 3) | (len >> 8)) & 0xFF;

    while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
    SPI1_REG(SPI_REG_TXFIFO) = len & 0xFF;

    while ((r1 = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
    while ((r2 = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
    
    if (cmd & EOS_NET_CMD_FLAG_ONEW) {
        r1 = 0;
        r2 = 0;
    }

    spi_buffer.cmd = ((r1 & 0xFF) >> 3);
    spi_buffer.len_rx = ((r1 & 0x07) << 8);

    spi_buffer.len_rx |= (r2 & 0xFF);
    spi_buffer.len = MAX(len, spi_buffer.len_rx);

    // Work around esp32 bug
    if (spi_buffer.len < 6) {
        spi_buffer.len = 6;
    } else if ((spi_buffer.len + 2) % 4 != 0) {
        spi_buffer.len = ((spi_buffer.len + 2)/4 + 1) * 4 - 2;
    }

    SPI1_REG(SPI_REG_TXCTRL) = SPI_TXWM(SPI_SIZE_CHUNK/2);
    SPI1_REG(SPI_REG_RXCTRL) = SPI_RXWM(SPI_SIZE_CHUNK - 1);
    SPI1_REG(SPI_REG_IE) = SPI_IP_TXWM | SPI_IP_RXWM;
}

static int spi_xchg_next(unsigned char *_buffer) {
    unsigned char cmd;
    unsigned char *buffer = NULL;
    uint16_t len;

    eos_msgq_pop(&spi_sndq, &cmd, &buffer, &len);
    if (cmd) {
        spi_xchg(cmd, buffer, len);
    } else if (spi_rts) {
        if (_buffer == NULL) _buffer = spi_bufq_pop();
        if (_buffer) {
            spi_xchg(0, _buffer, 0);
            return 0;
        }
    }
    return 1;
}

static void spi_xchg_handler(void) {
    int i;
    
    if (SPI1_REG(SPI_REG_IP) & SPI_IP_TXWM) {
        uint16_t sz_chunk = MIN(spi_buffer.len - spi_buffer.idx_tx, SPI_SIZE_CHUNK);
        for (i=0; i<sz_chunk; i++) {
            if (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL) break;
            SPI1_REG(SPI_REG_TXFIFO) = spi_buffer.buffer[spi_buffer.idx_tx+i];
        }
        spi_buffer.idx_tx += i;
    }
    
    for (i=0; i<spi_buffer.idx_tx - spi_buffer.idx_rx; i++) {
        volatile uint32_t x = SPI1_REG(SPI_REG_RXFIFO);
        if (x & SPI_RXFIFO_EMPTY) break;
        spi_buffer.buffer[spi_buffer.idx_rx+i] = x & 0xFF;
    }
    spi_buffer.idx_rx += i;

    if (spi_buffer.idx_rx == spi_buffer.len) {
        SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
        SPI1_REG(SPI_REG_IE) = 0x0;
        if (spi_buffer.cmd) {
            int r = eos_evtq_push(spi_buffer.cmd | EOS_EVT_MASK_NET, spi_buffer.buffer, spi_buffer.len_rx);
            if (r) spi_bufq_push(spi_buffer.buffer);
        } else if ((spi_res || spi_res_next) && (spi_res_buf == NULL)) {
            spi_res_buf = spi_buffer.buffer;
            spi_res_next = 0;
        } else {
            spi_bufq_push(spi_buffer.buffer);
        }
    } else if (spi_buffer.idx_tx == spi_buffer.len) {
        SPI1_REG(SPI_REG_RXCTRL) = SPI_RXWM(MIN(spi_buffer.len - spi_buffer.idx_rx - 1, SPI_SIZE_CHUNK - 1));
        SPI1_REG(SPI_REG_IE) = SPI_IP_RXWM;
    }
}

static void spi_cts_hanler(void) {
    GPIO_REG(GPIO_RISE_IP) = (0x1 << SPI_GPIO_CTS_OFFSET);
    spi_cts = 1;

    if (spi_rdy) {
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
        spi_rts = 1;
        if (spi_rdy && spi_cts) spi_reset();
    }
    
    if (GPIO_REG(GPIO_FALL_IP) & rts_offset) {
        GPIO_REG(GPIO_FALL_IP) = rts_offset;
        spi_rts = 0;
    }
}

void eos_net_init(void) {
    int i;
    
    spi_bufq.idx_r = 0;
    spi_bufq.idx_w = 0;
    for (i=0; i<SPI_SIZE_BUFQ; i++) {
        spi_bufq_push(spi_bufq_array[i]);
    }
    eos_msgq_init(&spi_sndq, spi_sndq_array, SPI_SIZE_BUFQ);
    GPIO_REG(GPIO_IOF_SEL) &= ~SPI_IOF_MASK;
    GPIO_REG(GPIO_IOF_EN) |= SPI_IOF_MASK;
    eos_intr_set_handler(INT_SPI1_BASE, 5, spi_xchg_handler);

    GPIO_REG(GPIO_OUTPUT_EN) &= ~(0x1 << SPI_GPIO_CTS_OFFSET);
    GPIO_REG(GPIO_PULLUP_EN) |= (0x1 << SPI_GPIO_CTS_OFFSET);
    GPIO_REG(GPIO_INPUT_EN) |= (0x1 << SPI_GPIO_CTS_OFFSET);
    GPIO_REG(GPIO_RISE_IE) |= (0x1 << SPI_GPIO_CTS_OFFSET);
    eos_intr_set_handler(INT_GPIO_BASE + SPI_GPIO_CTS_OFFSET, 5, spi_cts_hanler);

    GPIO_REG(GPIO_OUTPUT_EN) &= ~(0x1 << SPI_GPIO_RTS_OFFSET);
    GPIO_REG(GPIO_PULLUP_EN) |= (0x1 << SPI_GPIO_RTS_OFFSET);
    GPIO_REG(GPIO_INPUT_EN) |= (0x1 << SPI_GPIO_RTS_OFFSET);
    GPIO_REG(GPIO_RISE_IE) |= (0x1 << SPI_GPIO_RTS_OFFSET);
    GPIO_REG(GPIO_FALL_IE) |= (0x1 << SPI_GPIO_RTS_OFFSET);
    eos_intr_set_handler(INT_GPIO_BASE + SPI_GPIO_RTS_OFFSET, 5, spi_rts_hanler);
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
    spi_rdy = 1;
    if (spi_cts) spi_xchg_next(NULL);
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_net_stop(void) {
    volatile uint8_t done = 0;
    
    clear_csr(mstatus, MSTATUS_MIE);
    spi_rdy = 0;
    if (spi_cts) {
        uint32_t iof_mask = ((uint32_t)1 << IOF_SPI1_SS2);
        GPIO_REG(GPIO_IOF_EN) &= ~iof_mask;
        done = 1;
    }
    set_csr(mstatus, MSTATUS_MIE);
    
    while (!done) {
        clear_csr(mstatus, MSTATUS_MIE);
        done = spi_cts;
        if (!done) asm volatile ("wfi");
        set_csr(mstatus, MSTATUS_MIE);
    }
}

int eos_net_reserve(unsigned char *buffer) {
    int rv = EOS_OK;

    clear_csr(mstatus, MSTATUS_MIE);
    spi_res++;
    if (spi_res_buf == NULL) {
        spi_res_buf = buffer;
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
            if (spi_res_buf) {
                ret = 1;
            } else {
                asm volatile ("wfi");
            }
            set_csr(mstatus, MSTATUS_MIE);
        }
    } else {
        clear_csr(mstatus, MSTATUS_MIE);
        spi_res++;
        spi_res_buf = spi_bufq_pop();
        ret = (spi_res_buf != NULL);
        set_csr(mstatus, MSTATUS_MIE);
    }
    return ret;
}

int eos_net_release(unsigned char reserved) {
    int rv = EOS_OK;

    clear_csr(mstatus, MSTATUS_MIE);
    if (reserved) spi_res--;
    if (!spi_res && spi_res_buf) {
        rv = spi_bufq_push((unsigned char *)spi_res_buf);
        if (!rv) spi_res_buf = NULL;
    }
    set_csr(mstatus, MSTATUS_MIE);
    
    return rv;
}

unsigned char *eos_net_alloc(void) {
    volatile unsigned char *ret = NULL;

    while (ret == NULL) {
        clear_csr(mstatus, MSTATUS_MIE);
        if (spi_res_buf) {
            ret = spi_res_buf;
            spi_res_buf = NULL;
        } else {
            asm volatile ("wfi");
        }
        set_csr(mstatus, MSTATUS_MIE);
    }
    
    return (unsigned char *)ret;
}

int eos_net_free(unsigned char *buffer, unsigned char reserve_next) {
    int rv = EOS_OK;
    uint8_t do_release = 1;

    clear_csr(mstatus, MSTATUS_MIE);
    if ((spi_res || reserve_next) && (spi_res_buf == NULL)) {
        spi_res_buf = buffer;
    } else {
        if (spi_rdy && spi_cts) do_release = spi_xchg_next(buffer);
        if (do_release) rv = spi_bufq_push(buffer);
    }
    set_csr(mstatus, MSTATUS_MIE);

    return rv;
}

int eos_net_send(unsigned char cmd, unsigned char *buffer, uint16_t len) {
    int rv = EOS_OK;

    clear_csr(mstatus, MSTATUS_MIE);
    if (spi_rdy && spi_cts) {
        spi_xchg(cmd, buffer, len);
    } else {
        rv = eos_msgq_push(&spi_sndq, cmd, buffer, len);
    }
    set_csr(mstatus, MSTATUS_MIE);

    return rv;
}
