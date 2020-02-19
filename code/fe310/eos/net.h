#include <stdint.h>
#include "event.h"

#define EOS_NET_SIZE_BUF        1500
#define EOS_NET_SIZE_BUFQ       2

#define EOS_NET_MTYPE_SOCK      1
#define EOS_NET_MTYPE_AUDIO     2

#define EOS_NET_MTYPE_WIFI      5
#define EOS_NET_MTYPE_CELL      6
#define EOS_NET_MTYPE_SIP       7
#define EOS_NET_MTYPE_APP       8

#define EOS_NET_MAX_MTYPE       8

void eos_net_init(void);
void eos_net_start(void);
void eos_net_stop(void);
void eos_net_set_handler(unsigned char type, eos_evt_handler_t handler);

void eos_net_acquire(void);
void eos_net_release(void);
unsigned char *eos_net_alloc(void);
void eos_net_free(unsigned char *buffer, unsigned char more);
int eos_net_send(unsigned char type, unsigned char *buffer, uint16_t len, unsigned char more);
