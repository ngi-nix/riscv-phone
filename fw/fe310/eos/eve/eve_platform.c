#include <stdlib.h>

#include "platform.h"

#include "eos.h"
#include "interrupt.h"
#include "event.h"

#include "eve.h"
#include "eve_platform.h"


static void handle_time(unsigned char type) {
    eve_handle_time();
}

static void handle_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    eve_handle_touch();

    GPIO_REG(GPIO_LOW_IP) = (1 << EVE_PIN_INTR);
    GPIO_REG(GPIO_LOW_IE) |= (1 << EVE_PIN_INTR);
}

static void handle_intr(void) {
    GPIO_REG(GPIO_LOW_IE) &= ~(1 << EVE_PIN_INTR);
    eos_evtq_push_isr(EOS_EVT_UI | EVE_ETYPE_INTR, NULL, 0);
    return;
}

void eve_time_sleep(uint32_t ms) {
    eos_time_sleep(ms);
}

void eve_timer_set(uint32_t ms) {
    eos_timer_set(ms, EOS_TIMER_ETYPE_UI);
}

void eve_timer_clear(void) {
    eos_timer_clear(EOS_TIMER_ETYPE_UI);
}

uint64_t eve_time_get_tick(void) {
    return eos_time_get_tick();
}

void eve_platform_init(void) {
    eos_evtq_set_handler(EOS_EVT_UI, handle_evt);
    eos_timer_set_handler(EOS_TIMER_ETYPE_UI, handle_time);

    GPIO_REG(GPIO_INPUT_EN)     |=  (1 << EVE_PIN_INTR);
    GPIO_REG(GPIO_OUTPUT_EN)    &= ~(1 << EVE_PIN_INTR);
    GPIO_REG(GPIO_PULLUP_EN)    &= ~(1 << EVE_PIN_INTR);
    GPIO_REG(GPIO_OUTPUT_XOR)   &= ~(1 << EVE_PIN_INTR);

    GPIO_REG(GPIO_LOW_IE)       |=  (1 << EVE_PIN_INTR);
    eos_intr_set(INT_GPIO_BASE + EVE_PIN_INTR, IRQ_PRIORITY_UI, handle_intr);

    eos_spi_set_div(EOS_SPI_DEV_EVE, 4);
}

void eve_spi_start(void) {
    eos_spi_select(EOS_SPI_DEV_EVE);
}

void eve_spi_stop(void) {
    eos_spi_deselect();
}

#include <stdio.h>

void *eve_malloc(size_t size) {
    void *p = malloc(size);
    printf("MALLOC:%p %d\n", p, size);
    return p;
}

void eve_free(void *p) {
    printf("FREE:%p\n", p);
    free(p);
}
