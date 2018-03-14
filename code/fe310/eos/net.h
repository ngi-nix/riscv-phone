#include <stdint.h>
#include "event.h"

#define EOS_NET_CMD_FLAG_ONEW   0x10

#define EOS_NET_CMD_CONNECT     1
#define EOS_NET_CMD_DISCONNECT  2
#define EOS_NET_CMD_SCAN        3
#define EOS_NET_CMD_PKT         4

#define EOS_NET_MAX_CMD         4

void eos_net_init(void);
void eos_net_start(uint32_t sckdiv);
void eos_net_stop(void);
void eos_net_set_handler(unsigned char cmd, eos_evt_fptr_t handler, uint8_t flags);
int eos_net_reserve(unsigned char *buffer);
int eos_net_acquire(unsigned char reserved);
int eos_net_release(unsigned char reserved);
unsigned char *eos_net_alloc(void);
int eos_net_free(unsigned char *buffer, unsigned char reserve_next);
int eos_net_send(unsigned char cmd, unsigned char *buffer, uint16_t len);
