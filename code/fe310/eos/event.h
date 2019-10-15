#include <stdint.h>

#include "evt_def.h"

typedef void (*eos_evt_fptr_t) (unsigned char, unsigned char *, uint16_t);

void eos_evtq_init(void);
int eos_evtq_push(unsigned char type, unsigned char *buffer, uint16_t len);
void eos_evtq_pop(unsigned char *type, unsigned char **buffer, uint16_t *len);
void eos_evtq_bad_handler(unsigned char type, unsigned char *buffer, uint16_t len);
void eos_evtq_set_handler(unsigned char type, eos_evt_fptr_t handler);
void eos_evtq_set_flags(unsigned char type, uint8_t flags);
void eos_evtq_wait(unsigned char type, unsigned char *selector, uint16_t sel_len, unsigned char **buffer, uint16_t *len);
void eos_evtq_loop(void);

