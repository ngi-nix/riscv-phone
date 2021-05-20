#include <stdint.h>

#include "evt_def.h"

typedef void (*eos_evt_handler_t) (unsigned char, unsigned char *, uint16_t);

void eos_evtq_init(void);
int eos_evtq_push(unsigned char type, unsigned char *buffer, uint16_t len);
int eos_evtq_push_isr(unsigned char type, unsigned char *buffer, uint16_t len);
void eos_evtq_pop(unsigned char *type, unsigned char **buffer, uint16_t *len);
void eos_evtq_pop_isr(unsigned char *type, unsigned char **buffer, uint16_t *len);
int eos_evtq_get(unsigned char type, unsigned char **buffer, uint16_t *len);
int eos_evtq_find(unsigned char type, unsigned char *selector, uint16_t sel_len, unsigned char **buffer, uint16_t *len);
void eos_evtq_wait(unsigned char type, unsigned char *selector, uint16_t sel_len, unsigned char **buffer, uint16_t *len);
void eos_evtq_flush(void);
void eos_evtq_flush_isr(void);
void eos_evtq_loop(void);

void eos_evtq_bad_handler(unsigned char type, unsigned char *buffer, uint16_t len);
void eos_evtq_set_handler(unsigned char type, eos_evt_handler_t handler);
eos_evt_handler_t eos_evtq_get_handler(unsigned char type);
