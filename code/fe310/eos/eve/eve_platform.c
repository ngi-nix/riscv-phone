#include <stdlib.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"
#include "interrupt.h"
#include "event.h"
#include "timer.h"
#include "spi.h"
#include "eve.h"
#include "eve_platform.h"

#include "irq_def.h"

#define EVE_PIN_INTR            0

static void handle_time(unsigned char type) {
    eos_spi_dev_start(EOS_DEV_DISP);
    eve_handle_time();
    eos_spi_dev_stop();
}

static void handle_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    uint8_t flags;

    eos_spi_dev_start(EOS_DEV_DISP);
    eve_handle_touch();
    eos_spi_dev_stop();

    GPIO_REG(GPIO_LOW_IP) = (1 << EVE_PIN_INTR);
    GPIO_REG(GPIO_LOW_IE) |= (1 << EVE_PIN_INTR);
}

static void handle_intr(void) {
    GPIO_REG(GPIO_LOW_IE) &= ~(1 << EVE_PIN_INTR);
    eos_evtq_push_isr(EOS_EVT_UI | EVE_ETYPE_INTR, NULL, 0);
    return;
}

void eve_sleep(uint32_t ms) {
    eos_timer_sleep(ms);
}

void eve_timer_set(uint32_t ms) {
    eos_timer_set(ms, EOS_TIMER_ETYPE_UI, 0);
}

void eve_timer_clear(void) {
    eos_timer_clear(EOS_TIMER_ETYPE_UI);
}

uint64_t eve_timer_get_tick(void) {
    volatile uint64_t *mtime = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIME);
    return *mtime;
}

void eve_init_platform(void) {
    eos_evtq_set_handler(EOS_EVT_UI, handle_evt);
    eos_timer_set_handler(EOS_TIMER_ETYPE_UI, handle_time);

    GPIO_REG(GPIO_INPUT_EN)     |=  (1 << EVE_PIN_INTR);
    GPIO_REG(GPIO_OUTPUT_EN)    &= ~(1 << EVE_PIN_INTR);
    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << EVE_PIN_INTR);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << EVE_PIN_INTR);

    GPIO_REG(GPIO_LOW_IE)       |=  (1 << EVE_PIN_INTR);
    eos_intr_set(INT_GPIO_BASE + EVE_PIN_INTR, IRQ_PRIORITY_UI, handle_intr);

    eos_spi_dev_set_div(EOS_DEV_DISP, 4);
}