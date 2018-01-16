#include <stdint.h>

typedef uint32_t (*eos_timer_fptr_t) (void);

void eos_timer_init(void);
void eos_timer_set(uint32_t tick, unsigned char is_evt);
void eos_timer_set_handler(eos_timer_fptr_t handler);
