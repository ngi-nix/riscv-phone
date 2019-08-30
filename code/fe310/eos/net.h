#include <stdint.h>
#include "event.h"

#include "net_def.h"

void eos_net_init(void);
void eos_net_start(uint32_t sckdiv);
void eos_net_stop(void);
void eos_net_set_handler(unsigned char cmd, eos_evt_fptr_t handler, uint8_t flags);
int eos_net_acquire(unsigned char reserved);
int eos_net_release(void);
unsigned char *eos_net_alloc(void);
int eos_net_free(unsigned char *buffer, unsigned char more);
int eos_net_send(unsigned char cmd, unsigned char *buffer, uint16_t len);
