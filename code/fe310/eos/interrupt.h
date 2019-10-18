#include <stdint.h>

// Structures for registering different interrupt handlers
// for different parts of the application.
typedef void (*eos_intr_fptr_t) (void);

void eos_intr_init(void);
void eos_intr_set(uint8_t int_num, uint8_t priority, eos_intr_fptr_t handler);
void eos_intr_set_handler(uint8_t int_num, eos_intr_fptr_t handler);
void eos_intr_set_priority(uint8_t int_num, uint8_t priority);
void eos_intr_enable(uint8_t int_num);
void eos_intr_disable(uint8_t int_num);
void eos_intr_mask(uint8_t priority);