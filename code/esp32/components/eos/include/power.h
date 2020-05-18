#include <stdint.h>

#define EOS_PWR_MTYPE_BUTTON    0

#define EOS_PWR_WAKE_BTN        1
#define EOS_PWR_WAKE_NET        2
#define EOS_PWR_WAKE_MSG        3
#define EOS_PWR_WAKE_UART       4

void eos_power_init(void);
void eos_power_sleep(void);
void eos_power_wake(uint8_t source);
void eos_power_net_ready(void);