#include <stdint.h>

#include "evt_def.h"

typedef void (*eos_evt_fptr_t) (unsigned char, unsigned char *, uint16_t);

void eos_evtq_init(void);
int eos_evtq_push(unsigned char cmd, unsigned char *buffer, uint16_t len);
void eos_evtq_pop(unsigned char *cmd, unsigned char **buffer, uint16_t *len);
void eos_evtq_bad_handler(unsigned char cmd, unsigned char *buffer, uint16_t len);
void eos_evtq_handler_wrapper(unsigned char cmd, unsigned char *buffer, uint16_t len, uint16_t *flags_acq, uint16_t flag, eos_evt_fptr_t f);
void eos_evtq_handle(unsigned char cmd, unsigned char *buffer, uint16_t len);
void eos_evtq_loop(void);
void eos_evtq_set_handler(unsigned char cmd, eos_evt_fptr_t handler, uint8_t flags);
