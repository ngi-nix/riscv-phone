#include <stdint.h>
#include "event.h"

#define EOS_PWR_MTYPE_BUTTON    0

#define EOS_PWR_MAX_MTYPE       2

#define EOS_PWR_WAKE_RESET      0
#define EOS_PWR_WAKE_RTC        1
#define EOS_PWR_WAKE_BTN        2

#define EOS_PWR_RST_PWRON       0
#define EOS_PWR_RST_EXT         1
#define EOS_PWR_RST_WDOG        2

void eos_power_init(void);
uint8_t eos_power_cause_wake(void);
uint8_t eos_power_cause_rst(void);
void eos_power_sleep(void);
void eos_power_wake_at(uint32_t msec);
void eos_power_wake_disable(void);
void eos_power_set_handler(unsigned char mtype, eos_evt_handler_t handler);