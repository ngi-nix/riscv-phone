#include <stdlib.h>
#include <stdint.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"
#include "event.h"
#include "timer.h"
#include "spi.h"
#include "spi_dev.h"
#include "net.h"
#include "lcd.h"
#include "eve/eve.h"

#include "power.h"

#define PWR_RTC_SCALE   15
#define PWR_RTC_SFREQ   (EOS_TIMER_RTC_FREQ >> PWR_RTC_SCALE)

static eos_evt_handler_t evt_handler[EOS_PWR_MAX_MTYPE];
static unsigned char power_btn_down;

static void power_handle_msg(unsigned char type, unsigned char *buffer, uint16_t len) {
    unsigned char mtype;

    if ((buffer == NULL) || (len < 1)) {
        eos_net_bad_handler(type, buffer, len);
        return;
    }

    mtype = buffer[0];
    if ((mtype < EOS_PWR_MAX_MTYPE) && evt_handler[mtype]) {
        evt_handler[mtype](mtype, buffer, len);
    } else {
        eos_net_bad_handler(type, buffer, len);
    }
}

static void power_handle_btn(unsigned char type, unsigned char *buffer, uint16_t len) {
    unsigned char level = buffer[1];

    eos_net_free(buffer, 0);
    if (!level) {
        power_btn_down = 1;
        return;
    }
    if (!power_btn_down) return;

    eos_power_sleep();
}

int eos_power_init(uint8_t wakeup_cause) {
    int i;

    for (i=0; i<EOS_PWR_MAX_MTYPE; i++) {
        evt_handler[i] = NULL;
    }
    eos_net_set_handler(EOS_NET_MTYPE_POWER, power_handle_msg);
    eos_power_set_handler(EOS_PWR_MTYPE_BUTTON, power_handle_btn);

    AON_REG(AON_PMUKEY) = 0x51F15E;
    AON_REG(AON_PMUIE) = 0x5;

    AON_REG(AON_RTCCMP) = 0xFFFFFFFF;
    AON_REG(AON_RTCCFG) = PWR_RTC_SCALE;
    AON_REG(AON_RTCHI) = 0;
    AON_REG(AON_RTCLO) = 0;

    return EOS_OK;
}

uint8_t eos_power_wakeup_cause(void) {
    return AON_REG(AON_PMUCAUSE) & 0xff;
}

uint8_t eos_power_reset_cause(void) {
    return (AON_REG(AON_PMUCAUSE) >> 8) & 0xff;
}

int eos_power_sleep(void) {
    int rv;

    rv = eos_lcd_sleep();
    if (rv) return rv;

    eos_spi_select(EOS_SPI_DEV_EVE);
    eve_sleep();
    eos_spi_deselect();

    rv = eos_net_sleep(1000);
    if (rv) return rv;

    AON_REG(AON_PMUKEY) = 0x51F15E;
    AON_REG(AON_PMUSLEEP) = 1;

    return EOS_OK;
}

void eos_power_wake_at(uint32_t msec) {
    uint32_t pmuie;

    AON_REG(AON_RTCCFG) |= AON_RTCCFG_ENALWAYS;
    AON_REG(AON_RTCCMP) = msec * PWR_RTC_SFREQ / 1000;

    pmuie = AON_REG(AON_PMUIE) | 0x2;
    AON_REG(AON_PMUKEY) = 0x51F15E;
    AON_REG(AON_PMUIE) = pmuie;
}

void eos_power_wake_disable(void) {
    uint32_t pmuie;

    AON_REG(AON_RTCCMP) = 0xFFFFFFFF;
    AON_REG(AON_RTCCFG) &= ~AON_RTCCFG_ENALWAYS;
    AON_REG(AON_RTCHI) = 0;
    AON_REG(AON_RTCLO) = 0;

    pmuie = AON_REG(AON_PMUIE) & ~0x2;
    AON_REG(AON_PMUKEY) = 0x51F15E;
    AON_REG(AON_PMUIE) = pmuie;
}

void eos_power_set_handler(unsigned char mtype, eos_evt_handler_t handler) {
    if (mtype < EOS_PWR_MAX_MTYPE) evt_handler[mtype] = handler;
}

eos_evt_handler_t eos_power_get_handler(unsigned char mtype) {
    if (mtype < EOS_PWR_MAX_MTYPE) return evt_handler[mtype];
    return NULL;
}
