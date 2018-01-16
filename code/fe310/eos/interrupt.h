#include <stdint.h>

// Structures for registering different interrupt handlers
// for different parts of the application.
typedef void (*eos_intr_fptr_t) (void);

void eos_intr_init(void);
void eos_intr_set_handler(uint8_t int_num, uint8_t priority, eos_intr_fptr_t handler);
