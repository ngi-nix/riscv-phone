#include <stdlib.h>
#include <stdint.h>

#include "encoding.h"
#include "platform.h"

#include "eos.h"
#include "event.h"
#include "timer.h"
#include "net.h"
#include "eve/eve.h"

#include "power.h"

#define PWR_RTC_SCALE   15
#define PWR_RTC_SFREQ   (EOS_TIMER_RTC_FREQ >> PWR_RTC_SCALE)

static eos_evt_handler_t evt_handler[EOS_PWR_MAX_MTYPE];
static unsigned char power_btn_down;

static void power_handle_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    if ((buffer == NULL) || (len < 1)) {
        eos_net_bad_handler(type, buffer, len);
        return;
    }

    unsigned char mtype = buffer[0];
    if (mtype < EOS_PWR_MAX_MTYPE) {
        evt_handler[mtype](type, buffer, len);
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

void eos_power_init(void) {
    int i;

    for (i=0; i<EOS_PWR_MAX_MTYPE; i++) {
        evt_handler[i] = eos_net_bad_handler;
    }
    eos_net_set_handler(EOS_NET_MTYPE_POWER, power_handle_evt);
    eos_power_set_handler(EOS_PWR_MTYPE_BUTTON, power_handle_btn);

    AON_REG(AON_PMUKEY) = 0x51F15E;
    AON_REG(AON_PMUIE) = 0x5;

    AON_REG(AON_RTCCMP) = 0xFFFFFFFF;
    AON_REG(AON_RTCCFG) = PWR_RTC_SCALE;
    AON_REG(AON_RTCHI) = 0;
    AON_REG(AON_RTCLO) = 0;
}

uint8_t eos_power_cause_wake(void) {
    return AON_REG(AON_PMUCAUSE) & 0xff;
}

uint8_t eos_power_cause_rst(void) {
    return (AON_REG(AON_PMUCAUSE) >> 8) & 0xff;
}

void eos_power_sleep(void) {
    eos_spi_dev_start(EOS_DEV_DISP);
    eve_sleep();
    eos_spi_dev_stop();
    eos_net_sleep(1000);

    AON_REG(AON_PMUKEY) = 0x51F15E;
    AON_REG(AON_PMUSLEEP) = 1;
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
    if (handler == NULL) handler = eos_net_bad_handler;
    if (mtype < EOS_PWR_MAX_MTYPE) evt_handler[mtype] = handler;
}
