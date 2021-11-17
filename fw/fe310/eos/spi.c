#include <stdlib.h>
#include <stdint.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"
#include "msgq.h"
#include "interrupt.h"
#include "event.h"

#include "board.h"

#include "spi.h"
#include "spi_priv.h"

#define SPI_MODE0               0x00
#define SPI_MODE1               0x01
#define SPI_MODE2               0x02
#define SPI_MODE3               0x03

#define SPI_FLAG_XCHG           0x10

#define SPI_IOF_MASK            (((uint32_t)1 << IOF_SPI1_SCK) | ((uint32_t)1 << IOF_SPI1_MOSI) | ((uint32_t)1 << IOF_SPI1_MISO)) | SPI_IOF_MASK_CS

#define MIN(X, Y)               (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)               (((X) > (Y)) ? (X) : (Y))

static uint8_t spi_cspin;
static volatile uint8_t spi_state_flags;
static unsigned char spi_evt;
static unsigned char spi_in_xchg;

static uint32_t spi_state_len = 0;
static uint32_t spi_state_idx_tx = 0;
static uint32_t spi_state_idx_rx = 0;
static unsigned char *spi_state_buf = NULL;

static eos_evt_handler_t evt_handler[EOS_SPI_MAX_EVT];

static void spi_handle_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    unsigned char idx = (type & ~EOS_EVT_MASK) - 1;
    if (idx < EOS_SPI_MAX_EVT) {
        evt_handler[idx](type, buffer, len);
    } else {
        eos_evtq_bad_handler(type, buffer, len);
    }
}

void eos_spi_init(uint8_t wakeup_cause) {
    int i;

    for (i=0; i<EOS_SPI_MAX_EVT; i++) {
        evt_handler[i] = eos_evtq_bad_handler;
    }
    eos_evtq_set_handler(EOS_EVT_SPI, spi_handle_evt);
    eos_intr_set(INT_SPI1_BASE, IRQ_PRIORITY_SPI_XCHG, NULL);

    SPI1_REG(SPI_REG_SCKMODE) = SPI_MODE0;
    SPI1_REG(SPI_REG_FMT) = SPI_FMT_PROTO(SPI_PROTO_S) |
      SPI_FMT_ENDIAN(SPI_ENDIAN_MSB) |
      SPI_FMT_DIR(SPI_DIR_RX) |
      SPI_FMT_LEN(8);

    GPIO_REG(GPIO_IOF_SEL) &= ~SPI_IOF_MASK;
    GPIO_REG(GPIO_IOF_EN) |= SPI_IOF_MASK;

    // There is no way here to change the CS polarity.
    // SPI1_REG(SPI_REG_CSDEF) = 0xFFFF;
}

void eos_spi_start(uint16_t div, uint8_t csid, uint8_t cspin, unsigned char evt) {
    spi_state_flags = 0;
    spi_evt = evt;
    SPI1_REG(SPI_REG_SCKDIV) = div;
    SPI1_REG(SPI_REG_CSID) = csid;
    if (csid != SPI_CSID_NONE) {
        SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
    } else {
        spi_cspin = cspin;
        SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_OFF;
    }
    eos_intr_set_handler(INT_SPI1_BASE, eos_spi_handle_xchg);
}

void eos_spi_stop(void) {
    eos_spi_flush();
    spi_evt = 0;
}

void eos_spi_set_handler(unsigned char evt, eos_evt_handler_t handler) {
    if (handler == NULL) handler = eos_evtq_bad_handler;
    if (evt && (evt <= EOS_SPI_MAX_EVT)) evt_handler[evt - 1] = handler;
}

void _eos_spi_xchg_init(unsigned char *buffer, uint16_t len, uint8_t flags) {
    spi_state_flags &= 0xF0;
    spi_state_flags |= (SPI_FLAG_XCHG | flags);
    spi_state_buf = buffer;
    spi_state_len = len;
    spi_state_idx_tx = 0;
    spi_state_idx_rx = 0;
}

static void spi_xchg_finish(void) {
    uint8_t done = 0;

    while (!done) {
        clear_csr(mstatus, MSTATUS_MIE);
        done = !(spi_state_flags & SPI_FLAG_XCHG);
        if (!done) asm volatile ("wfi");
        set_csr(mstatus, MSTATUS_MIE);
    }
    spi_in_xchg = 0;
}

void eos_spi_xchg(unsigned char *buffer, uint16_t len, uint8_t flags) {
    if (spi_in_xchg) spi_xchg_finish();

    spi_in_xchg = 1;
    _eos_spi_xchg_init(buffer, len, flags);

    eos_spi_cs_set();
    SPI1_REG(SPI_REG_TXCTRL) = SPI_TXWM(SPI_SIZE_WM);
    SPI1_REG(SPI_REG_IE) = SPI_IP_TXWM;
}

void eos_spi_handle_xchg(void) {
    int i;
    uint16_t sz_chunk = MIN(spi_state_len - spi_state_idx_tx, SPI_SIZE_CHUNK);

    for (i=0; i<sz_chunk; i++) {
        volatile uint32_t x = SPI1_REG(SPI_REG_TXFIFO);
        if (x & SPI_TXFIFO_FULL) break;
        SPI1_REG(SPI_REG_TXFIFO) = spi_state_buf[spi_state_idx_tx+i];
    }
    spi_state_idx_tx += i;

    for (i=0; i<spi_state_idx_tx - spi_state_idx_rx; i++) {
        volatile uint32_t x = SPI1_REG(SPI_REG_RXFIFO);
        if (x & SPI_RXFIFO_EMPTY) break;
        spi_state_buf[spi_state_idx_rx+i] = x & 0xFF;
    }
    spi_state_idx_rx += i;

    if (spi_state_idx_tx == spi_state_len) {
        if ((spi_state_idx_rx == spi_state_len) || (spi_state_flags & EOS_SPI_FLAG_TX)) {
            spi_state_flags &= ~SPI_FLAG_XCHG;
            if (!(spi_state_flags & EOS_SPI_FLAG_MORE)) eos_spi_cs_clear();
            SPI1_REG(SPI_REG_IE) = 0x0;
            if (spi_evt) eos_evtq_push_isr(EOS_EVT_SPI | spi_evt, spi_state_buf, spi_state_len);
        } else {
            SPI1_REG(SPI_REG_RXCTRL) = SPI_RXWM(MIN(spi_state_len - spi_state_idx_rx - 1, SPI_SIZE_WM - 1));
            SPI1_REG(SPI_REG_IE) = SPI_IP_RXWM;
        }
    }
}

void eos_spi_cs_set(void) {
	/* cs low */
    if (SPI1_REG(SPI_REG_CSMODE) == SPI_CSMODE_OFF) {
        GPIO_REG(GPIO_OUTPUT_VAL) &= ~(1 << spi_cspin);
    } else {
        SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_HOLD;
    }
}

void eos_spi_cs_clear(void) {
	/* cs high */
    if (SPI1_REG(SPI_REG_CSMODE) == SPI_CSMODE_OFF) {
        GPIO_REG(GPIO_OUTPUT_VAL) |= (1 << spi_cspin);
    } else {
        SPI1_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
    }
}

uint8_t eos_spi_xchg8(uint8_t data, uint8_t flags) {
    volatile uint32_t x = 0;
    uint8_t rx = !(flags & EOS_SPI_FLAG_TX);

    spi_state_flags &= 0xF0;
    spi_state_flags |= flags;

    while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
    SPI1_REG(SPI_REG_TXFIFO) = data;

    if (rx) {
        while ((x = SPI1_REG(SPI_REG_RXFIFO)) & SPI_RXFIFO_EMPTY);
    }

    return x & 0xFF;
}

uint16_t eos_spi_xchg16(uint16_t data, uint8_t flags) {
    volatile uint32_t x = 0;
    uint8_t rx = !(flags & EOS_SPI_FLAG_TX);
    uint16_t r = 0;

    spi_state_flags &= 0xF0;
    spi_state_flags |= flags;

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

    return r;
}

uint32_t eos_spi_xchg24(uint32_t data, uint8_t flags) {
    volatile uint32_t x = 0;
    uint8_t rx = !(flags & EOS_SPI_FLAG_TX);
    uint32_t r = 0;

    spi_state_flags &= 0xF0;
    spi_state_flags |= flags;

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

    return r;
}

uint32_t eos_spi_xchg32(uint32_t data, uint8_t flags) {
    volatile uint32_t x = 0;
    uint8_t rx = !(flags & EOS_SPI_FLAG_TX);
    uint32_t r = 0;

    spi_state_flags &= 0xF0;
    spi_state_flags |= flags;

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

    return r;
}

void eos_spi_flush(void) {
    volatile uint32_t x = 0;

    if (spi_in_xchg) spi_xchg_finish();

    SPI1_REG(SPI_REG_TXCTRL) = SPI_TXWM(1);
    while (!x) {
        if (SPI1_REG(SPI_REG_IP) & SPI_IP_TXWM) x = SPI1_REG(SPI_REG_RXFIFO) & SPI_RXFIFO_EMPTY;
    }
}
