#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"
#include "interrupt.h"
#include "event.h"

#include "uart.h"

static eos_uart_handler_t uart_handler[EOS_UART_MAX_ETYPE];

static void uart_handle_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    unsigned char idx = (type & ~EOS_EVT_MASK) - 1;

    if ((idx < EOS_UART_MAX_ETYPE) && uart_handler[idx]) {
        uart_handler[idx](type);
    } else {
        eos_evtq_bad_handler(type, buffer, len);
    }
}

static void uart_handle_intr(void) {
    if (UART0_REG(UART_REG_IP) & UART_IP_TXWM) {
        UART0_REG(UART_REG_IE) &= ~UART_IP_TXWM;
        eos_evtq_push_isr(EOS_EVT_UART | EOS_UART_ETYPE_TX, NULL, 0);
    }
    if (UART0_REG(UART_REG_IP) & UART_IP_RXWM) {
        UART0_REG(UART_REG_IE) &= ~UART_IP_RXWM;
        eos_evtq_push_isr(EOS_EVT_UART | EOS_UART_ETYPE_RX, NULL, 0);
    }
}

void eos_uart_init(void) {
    int i;

    for (i=0; i<EOS_UART_MAX_ETYPE; i++) {
        uart_handler[i] = NULL;
    }
    eos_evtq_set_handler(EOS_EVT_UART, uart_handle_evt);
    eos_intr_set(INT_UART0_BASE, IRQ_PRIORITY_UART, uart_handle_intr);
}

void eos_uart_set_handler(unsigned char type, eos_uart_handler_t handler) {
    if (type && (type <= EOS_UART_MAX_ETYPE)) uart_handler[type - 1] = handler;
}

void eos_uart_txwm_set(uint8_t wm) {
    UART0_REG(UART_REG_TXCTRL)  &= ~UART_TXWM(0xFFFF);
    UART0_REG(UART_REG_TXCTRL)  |=  UART_TXWM(wm);
    UART0_REG(UART_REG_IE)      |=  UART_IP_TXWM;
}

void eos_uart_txwm_clear(void) {
    UART0_REG(UART_REG_IE)      &= ~UART_IP_TXWM;
}

void eos_uart_rxwm_set(uint8_t wm) {
    UART0_REG(UART_REG_RXCTRL)  &= ~UART_RXWM(0xFFFF);
    UART0_REG(UART_REG_RXCTRL)  |=  UART_RXWM(wm);
    UART0_REG(UART_REG_IE)      |=  UART_IP_RXWM;
}

void eos_uart_rxwm_clear(void) {
    UART0_REG(UART_REG_IE)      &= ~UART_IP_RXWM;
}

int eos_uart_putc(int c, char b) {
    if (b) {
        while (UART0_REG(UART_REG_TXFIFO) & 0x80000000);
        UART0_REG(UART_REG_TXFIFO) = c & 0xff;
    } else {
        if (UART0_REG(UART_REG_TXFIFO) & 0x80000000) return EOS_ERR_FULL;
        UART0_REG(UART_REG_TXFIFO) = c & 0xff;
    }
    return EOS_OK;
}

int eos_uart_getc(char b) {
    volatile uint32_t r;

    if (b) {
        while ((r = UART0_REG(UART_REG_RXFIFO)) & 0x80000000);
    } else {
        r = UART0_REG(UART_REG_RXFIFO);
        if (r & 0x80000000) return EOS_ERR_EMPTY;
    }
    return r & 0xff;
}