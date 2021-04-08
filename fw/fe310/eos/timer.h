#include <stdint.h>

#define EOS_TIMER_ETYPE_UI      1
#define EOS_TIMER_ETYPE_ECP     2
#define EOS_TIMER_ETYPE_USER    4

#define EOS_TIMER_MAX_ETYPE     4

#define EOS_TIMER_NONE          0xffffffff
#define EOS_TIMER_RTC_FREQ      32768

typedef void (*eos_timer_handler_t) (unsigned char);

void eos_timer_init(void);
void eos_timer_set_handler(unsigned char evt, eos_timer_handler_t handler);

uint32_t eos_timer_get(unsigned char evt);
void eos_timer_set(uint32_t msec, unsigned char evt);
void eos_timer_clear(unsigned char evt);

void eos_time_sleep(uint32_t msec);
uint64_t eos_time_get_tick(void);
uint32_t eos_time_since(uint32_t start);
