#include <stdint.h>

#define EOS_TIMER_ETYPE_ECP     1
#define EOS_TIMER_ETYPE_USER    2

#define EOS_TIMER_MAX_ETYPE     4

typedef void (*eos_timer_fptr_t) (unsigned char);

void eos_timer_init(void);
void eos_timer_set(uint32_t tick, unsigned char evt);
void eos_timer_set_handler(unsigned char evt, eos_timer_fptr_t handler, uint8_t flags);
