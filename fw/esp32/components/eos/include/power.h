#include <stdint.h>

#define EOS_PWR_MTYPE_BUTTON    1

#define EOS_PWR_WAKE_RST        0
#define EOS_PWR_WAKE_BTN        1
#define EOS_PWR_WAKE_NET        2
#define EOS_PWR_WAKE_MSG        3
#define EOS_PWR_WAKE_UART       4

#define EOS_PWR_SMODE_LIGHT     1
#define EOS_PWR_SMODE_DEEP      2

void eos_power_init(void);

void eos_power_wait4init(void);
uint8_t eos_power_wakeup_cause(void);
void eos_power_sleep(void);
void eos_power_wake(uint8_t source);
void eos_power_net_ready(void);