#include <stdlib.h>
#include <stdint.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"
#include "msgq.h"
#include "interrupt.h"
#include "event.h"

#include "net.h"
#include "spi.h"
#include "spi_def.h"
#include "irq_def.h"

#define MIN(X, Y)               (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)               (((X) > (Y)) ? (X) : (Y))
#define SPI_IOF_MASK            (((uint32_t)1 << IOF_SPI1_SCK) | ((uint32_t)1 << IOF_SPI1_MOSI) | ((uint32_t)1 << IOF_SPI1_MISO)) | ((uint32_t)1 << IOF_SPI1_SS0) | ((uint32_t)1 << IOF_SPI1_SS2) | ((uint32_t)1 << IOF_SPI1_SS3)

extern EOSMsgQ _eos_event_q;

static uint8_t spi_dev;
static uint8_t spi_dev_cs_pin;
static uint8_t spi_state_flags;
static unsigned char spi_in_xchg;

uint32_t _eos_spi_state_len = 0;
uint32_t _eos_spi_state_idx_tx = 0;
uint32_t _eos_spi_state_idx_rx = 0;
unsigned char *_eos_spi_state_buf = NULL;

static eos_evt_fptr_t evt_handler[EOS_SPI_MAX_DEV];

static void spi_handler_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    unsigned char idx = (type & ~EOS_EVT_MASK) - 1;
    if (idx < EOS_SPI_MAX_DEV) {
        evt_handler[idx](type, buffer, len);
    } else {
        eos_evtq_bad_handler(type, buffer, len);
    }
}

static void spi_flush(void) {
    SPI1_REG(SPI_REG_TXCTRL) = SPI_TXWM(1);
    while (!(SPI1_REG(SPI_REG_IP) & SPI_IP_TXWM));
    while (!(SPI1_REG(SPI_REG_RXFIFO) & SPI_RXFIFO_EMPTY));
}

static void spi_xchg_wait(void) {
    volatile uint8_t done = 0;

    while (!done) {
        clear_csr(mstatus, MSTATUS_MIE);
        done = !(spi_state_flags & SPI_FLAG_XCHG);
        if (!done) asm volatile ("wfi");
        set_csr(mstatus, MSTATUS_MIE);
    }
    spi_in_xchg = 0;
}

void eos_spi_init(void) {
    GPIO_REG(GPIO_OUTPUT_VAL)   |=  (1 << SPI_CS_PIN_CAM);
    GPIO_REG(GPIO_INPUT_EN)     &= ~(1 << SPI_CS_PIN_CAM);
    GPIO_REG(GPIO_OUTPUT_EN)    |=  (1 << SPI_CS_PIN_CAM);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << SPI_CS_PIN_CAM);

    SPI1_REG(SPI_REG_SCKMODE) = SPI_MODE0;
    SPI1_REG(SPI_REG_FMT) = SPI_FMT_PROTO(SPI_PROTO_S) |
      SPI_FMT_ENDIAN(SPI_ENDIAN_MSB) |
      SPI_FMT_DIR(SPI_DIR_RX) |
      SPI_FMT_LEN(8);

    GPIO_REG(GPIO_IOF_SEL) &= ~SPI_IOF_MASK;
    GPIO_REG(GPIO_IOF_EN) |= SPI_IOF_MASK;

    // There is no way here to change the CS polarity.
    // SPI1_REG(SPI_REG_CSDEF) = 0xFFFF;

    eos_intr_set(INT_SPI1_BASE, IRQ_PRIORITY_SPI_XCHG, NULL);
    eos_evtq_set_handler(EOS_EVT_SPI, spi_handler_evt);
}

void eos_spi_dev_start(unsigned char dev) {
    eos_net_stop();
    spi_dev = dev;
    spi_state_flags = SPI_FLAG_CS;
    switch (dev) {
        case EOS_SPI_DEV_DISP:
            SPI1_REG(SPI_REG_SCKDIV) = SPI_DIV_DISP;
            SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
            SPI1_REG(SPI_REG_CSID) = SPI_CS_IDX_DISP;
            break;
        case EOS_SPI_DEV_CARD:
            SPI1_REG(SPI_REG_SCKDIV) = SPI_DIV_CARD;
            SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
            SPI1_REG(SPI_REG_CSID) = SPI_CS_IDX_CARD;
            break;
        case EOS_SPI_DEV_CAM:
            spi_dev_cs_pin = SPI_CS_PIN_CAM;
            SPI1_REG(SPI_REG_SCKDIV) = SPI_DIV_CAM;
            SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_OFF;
            SPI1_REG(SPI_REG_CSID) = SPI_CS_IDX_NONE;
            break;
    }
    eos_intr_set_handler(INT_SPI1_BASE, eos_spi_xchg_handler);
}

void eos_spi_dev_stop(void) {
    if (spi_in_xchg) spi_xchg_wait();
    if (spi_state_flags & EOS_SPI_FLAG_TX) spi_flush();
    if (!(spi_state_flags & SPI_FLAG_CS)) eos_spi_cs_clear();

    spi_dev = EOS_SPI_DEV_NET;
    eos_net_start();
}

void eos_spi_xchg(unsigned char *buffer, uint16_t len, uint8_t flags) {
    if (spi_in_xchg) spi_xchg_wait();
    if (!(flags & EOS_SPI_FLAG_TX) && (spi_state_flags & EOS_SPI_FLAG_TX)) spi_flush();

    spi_in_xchg=1;
    spi_state_flags &= 0xF0;
    spi_state_flags |= (SPI_FLAG_XCHG | flags);
    _eos_spi_state_buf = buffer;
    _eos_spi_state_len = len;
    _eos_spi_state_idx_tx = 0;
    _eos_spi_state_idx_rx = 0;

    if (spi_state_flags & SPI_FLAG_CS) eos_spi_cs_set();
    SPI1_REG(SPI_REG_TXCTRL) = SPI_TXWM(SPI_SIZE_WM);
    SPI1_REG(SPI_REG_IE) = SPI_IP_TXWM;
}

static void spi_xchg_done(void) {
    SPI1_REG(SPI_REG_IE) = 0x0;
    if (spi_dev == EOS_SPI_DEV_NET) {
        eos_net_xchg_done();
    } else {
        spi_state_flags &= ~SPI_FLAG_XCHG;
        if (!(spi_state_flags & (EOS_SPI_FLAG_MORE | SPI_FLAG_CS))) eos_spi_cs_clear();
        eos_msgq_push(&_eos_event_q, EOS_EVT_SPI | spi_dev, _eos_spi_state_buf, _eos_spi_state_len);
    }
}

void eos_spi_xchg_handler(void) {
    int i;
    uint16_t sz_chunk = MIN(_eos_spi_state_len - _eos_spi_state_idx_tx, SPI_SIZE_CHUNK);

    for (i=0; i<sz_chunk; i++) {
        volatile uint32_t x = SPI1_REG(SPI_REG_TXFIFO);
        if (x & SPI_TXFIFO_FULL) break;
        SPI1_REG(SPI_REG_TXFIFO) = _eos_spi_state_buf[_eos_spi_state_idx_tx+i];
    }
    _eos_spi_state_idx_tx += i;

    for (i=0; i<_eos_spi_state_idx_tx - _eos_spi_state_idx_rx; i++) {
        volatile uint32_t x = SPI1_REG(SPI_REG_RXFIFO);
        if (x & SPI_RXFIFO_EMPTY) break;
        _eos_spi_state_buf[_eos_spi_state_idx_rx+i] = x & 0xFF;
    }
    _eos_spi_state_idx_rx += i;

    if (_eos_spi_state_idx_tx == _eos_spi_state_len) {
        if ((_eos_spi_state_idx_rx == _eos_spi_state_len) || (spi_state_flags & EOS_SPI_FLAG_TX)) {
            spi_xchg_done();
        } else {
            SPI1_REG(SPI_REG_RXCTRL) = SPI_RXWM(MIN(_eos_spi_state_len - _eos_spi_state_idx_rx - 1, SPI_SIZE_WM - 1));
            SPI1_REG(SPI_REG_IE) = SPI_IP_RXWM;
        }
    }
}

void eos_spi_cs_set(void) {
	/* cs low */
    if (SPI1_REG(SPI_REG_CSMODE) == SPI_CSMODE_OFF) {
        GPIO_REG(GPIO_OUTPUT_VAL) &= ~(1 << spi_dev_cs_pin);
    } else {
        SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_HOLD;
    }
    spi_state_flags &= ~SPI_FLAG_CS;
}

void eos_spi_cs_clear(void) {
	/* cs high */
    if (SPI1_REG(SPI_REG_CSMODE) == SPI_CSMODE_OFF) {
        GPIO_REG(GPIO_OUTPUT_VAL) |= (1 << spi_dev_cs_pin);
    } else {
        SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
    }
    spi_state_flags |= SPI_FLAG_CS;
}

uint8_t eos_spi_xchg8(uint8_t data, uint8_t flags) {
    volatile uint32_t x = 0;
    uint8_t rx = !(flags & EOS_SPI_FLAG_TX);

    if (spi_in_xchg) spi_xchg_wait();
    if (rx && (spi_state_flags & EOS_SPI_FLAG_TX)) spi_flush();

    spi_state_flags &= 0xF0;
    spi_state_flags |= flags;
    if ((flags & EOS_SPI_FLAG_AUTOCS) && (spi_state_flags & SPI_FLAG_CS)) eos_spi_cs_set();

    while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
    SPI1_REG(SPI_REG_TXFIFO) = data;

    if (rx) {
        while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
    }

    if ((flags & EOS_SPI_FLAG_AUTOCS) && !(flags & EOS_SPI_FLAG_MORE)) eos_spi_cs_clear();

    return x & 0xFF;
}

uint16_t eos_spi_xchg16(uint16_t data, uint8_t flags) {
    volatile uint32_t x = 0;
    uint8_t rx = !(flags & EOS_SPI_FLAG_TX);
    uint16_t r = 0;

    if (spi_in_xchg) spi_xchg_wait();
    if (rx && (spi_state_flags & EOS_SPI_FLAG_TX)) spi_flush();

    spi_state_flags &= 0xF0;
    spi_state_flags |= flags;
    if ((flags & EOS_SPI_FLAG_AUTOCS) && (spi_state_flags & SPI_FLAG_CS)) eos_spi_cs_set();

    if (flags & EOS_SPI_FLAG_BSWAP) {
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0x00FF);
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0xFF00) >> 8;
    } else {
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0xFF00) >> 8;
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0x00FF);
    }

    if (rx) {
        if (flags & EOS_SPI_FLAG_BSWAP) {
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF);
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF) << 8;
        } else {
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF) << 8;
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF);
        }
    }

    if ((flags & EOS_SPI_FLAG_AUTOCS) && !(flags & EOS_SPI_FLAG_MORE)) eos_spi_cs_clear();

    return r;
}

uint32_t eos_spi_xchg24(uint32_t data, uint8_t flags) {
    volatile uint32_t x = 0;
    uint8_t rx = !(flags & EOS_SPI_FLAG_TX);
    uint32_t r = 0;

    if (spi_in_xchg) spi_xchg_wait();
    if (rx && (spi_state_flags & EOS_SPI_FLAG_TX)) spi_flush();

    spi_state_flags &= 0xF0;
    spi_state_flags |= flags;
    if ((flags & EOS_SPI_FLAG_AUTOCS) && (spi_state_flags & SPI_FLAG_CS)) eos_spi_cs_set();

    if (flags & EOS_SPI_FLAG_BSWAP) {
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0x000000FF);
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0x0000FF00) >> 8;
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0x00FF0000) >> 16;
    } else {
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0x00FF0000) >> 16;
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0x0000FF00) >> 8;
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0x000000FF);
    }

    if (rx) {
        if (flags & EOS_SPI_FLAG_BSWAP) {
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF);
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF) << 8;
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF) << 16;
        } else {
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF) << 16;
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF) << 8;
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF);
        }
    }

    if ((flags & EOS_SPI_FLAG_AUTOCS) && !(flags & EOS_SPI_FLAG_MORE)) eos_spi_cs_clear();

    return r;
}

uint32_t eos_spi_xchg32(uint32_t data, uint8_t flags) {
    volatile uint32_t x = 0;
    uint8_t rx = !(flags & EOS_SPI_FLAG_TX);
    uint32_t r = 0;

    if (spi_in_xchg) spi_xchg_wait();
    if (rx && (spi_state_flags & EOS_SPI_FLAG_TX)) spi_flush();

    spi_state_flags &= 0xF0;
    spi_state_flags |= flags;
    if ((flags & EOS_SPI_FLAG_AUTOCS) && (spi_state_flags & SPI_FLAG_CS)) eos_spi_cs_set();

    if (flags & EOS_SPI_FLAG_BSWAP) {
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0x000000FF);
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0x0000FF00) >> 8;
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0x00FF0000) >> 16;
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0xFF000000) >> 24;
    } else {
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0xFF000000) >> 24;
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0x00FF0000) >> 16;
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0x0000FF00) >> 8;
        while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
        SPI1_REG(SPI_REG_TXFIFO) = (data & 0x000000FF);
    }

    if (rx) {
        if (flags & EOS_SPI_FLAG_BSWAP) {
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF);
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF) << 8;
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF) << 16;
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF) << 24;
        } else {
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF) << 24;
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF) << 16;
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF) << 8;
            while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
            r |= (x & 0xFF);
        }
    }

    if ((flags & EOS_SPI_FLAG_AUTOCS) && !(flags & EOS_SPI_FLAG_MORE)) eos_spi_cs_clear();

    return r;
}

void eos_spi_set_handler(unsigned char dev, eos_evt_fptr_t handler) {
    if (dev && (dev <= EOS_SPI_MAX_DEV)) {
        dev--;
    } else {
        return;
    }
    evt_handler[dev] = handler;
}
