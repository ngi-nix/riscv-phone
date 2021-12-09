#include <stdint.h>
#include "event.h"

#define EOS_PWR_MTYPE_BUTTON    1

#define EOS_PWR_MAX_MTYPE       2

#define EOS_PWR_WAKE_RST        0
#define EOS_PWR_WAKE_RTC        1
#define EOS_PWR_WAKE_BTN        2

#define EOS_PWR_RST_PWRON       0
#define EOS_PWR_RST_EXT         1
#define EOS_PWR_RST_WDOG        2

int eos_pwr_init(uint8_t wakeup_cause);
uint8_t eos_pwr_wakeup_cause(void);
uint8_t eos_pwr_reset_cause(void);
int eos_pwr_sleep(void);
void eos_pwr_wake_at(uint32_t msec);
void eos_pwr_wake_disable(void);

void eos_pwr_netinit(void);
void eos_pwr_set_handler(unsigned char mtype, eos_evt_handler_t handler);
eos_evt_handler_t eos_pwr_get_handler(unsigned char mtype);