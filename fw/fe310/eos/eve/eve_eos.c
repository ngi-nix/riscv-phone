#include <stdlib.h>

#include "platform.h"

#include "eos.h"
#include "interrupt.h"
#include "event.h"
#include "pwr.h"

#include "board.h"

#include "eve.h"
#include "eve_eos.h"

static int _run;

static void handle_time(unsigned char type) {
    if (_run) {
        eve_spi_start();
        eve_handle_time();
        eve_spi_stop();
    }
}

static void handle_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    if (_run) {
        eve_spi_start();
        eve_handle_intr();
        eve_spi_stop();

        GPIO_REG(GPIO_LOW_IE) |= (1 << EVE_PIN_INTR);
    }
}

static void handle_intr(void) {
    GPIO_REG(GPIO_LOW_IE) &= ~(1 << EVE_PIN_INTR);
    GPIO_REG(GPIO_LOW_IP) = (1 << EVE_PIN_INTR);
    eos_evtq_push_isr(EOS_EVT_UI | EVE_ETYPE_INTR, NULL, 0);
}


static void _start(void) {
    eve_start();

    GPIO_REG(GPIO_INPUT_EN)     |=  (1 << EVE_PIN_INTR);
    GPIO_REG(GPIO_OUTPUT_EN)    &= ~(1 << EVE_PIN_INTR);

    GPIO_REG(GPIO_LOW_IE)       |=  (1 << EVE_PIN_INTR);

    eos_intr_enable(INT_GPIO_BASE + EVE_PIN_INTR);
    _run = 1;
}

static void _stop(void) {
    _run = 0;
    eos_intr_disable(INT_GPIO_BASE + EVE_PIN_INTR);

    GPIO_REG(GPIO_LOW_IE)       &= ~(1 << EVE_PIN_INTR);

    eve_stop();
}

int eos_eve_init(uint8_t wakeup_cause, uint8_t gpio_dir, int touch_calibrate, uint32_t *touch_matrix) {
    int rst = (wakeup_cause == EOS_PWR_WAKE_RST);
    int rv = EVE_OK;

    eve_spi_start();
    if (rst) {
        rv = eve_init(gpio_dir, touch_calibrate, touch_matrix);
    } else {
        eve_active();
    }
    eve_spi_stop();

    if (rv) return EOS_ERR;

    eos_evtq_set_handler(EOS_EVT_UI, handle_evt);
    eos_timer_set_handler(EOS_TIMER_ETYPE_UI, handle_time);
    eos_intr_set(INT_GPIO_BASE + EVE_PIN_INTR, IRQ_PRIORITY_UI, handle_intr);
    eos_intr_disable(INT_GPIO_BASE + EVE_PIN_INTR);

    return EOS_OK;
}

int eos_eve_run(uint8_t wakeup_cause) {
    eve_spi_start();
    _start();
    eve_start_clk();
    eve_spi_stop();

    return EOS_OK;
}

void eos_eve_start(void) {
    eve_spi_start();
    _start();
    eve_spi_stop();
}

void eos_eve_stop(void) {
    eve_spi_start();
    _stop();
    eve_spi_stop();
}
