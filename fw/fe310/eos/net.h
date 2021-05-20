#include <stdint.h>
#include "event.h"

/* common */
#define EOS_NET_SIZE_BUF            1500

#define EOS_NET_MTYPE_SOCK          1
#define EOS_NET_MTYPE_POWER         4

#define EOS_NET_MTYPE_WIFI          5
#define EOS_NET_MTYPE_CELL          6
#define EOS_NET_MTYPE_SIP           7
#define EOS_NET_MTYPE_APP           8

#define EOS_NET_MAX_MTYPE           8

#define EOS_NET_MTYPE_FLAG_ONEW     0x40
#define EOS_NET_MTYPE_FLAG_REPW     0x80
#define EOS_NET_MTYPE_FLAG_MASK     0xc0

/* fe310 specific */
#define EOS_NET_SIZE_BUFQ           2

void eos_net_init(void);
void eos_net_start(uint8_t wakeup_cause);
void eos_net_stop(void);
int eos_net_sleep(uint32_t timeout);

void eos_net_bad_handler(unsigned char type, unsigned char *buffer, uint16_t len);
void eos_net_set_handler(unsigned char type, eos_evt_handler_t handler);
void eos_net_acquire_for_evt(unsigned char type, char acq);

void eos_net_acquire(void);
void eos_net_release(void);
unsigned char *eos_net_alloc(void);
void eos_net_free(unsigned char *buffer, unsigned char more);
int eos_net_send(unsigned char type, unsigned char *buffer, uint16_t len, unsigned char more);
int eos_net_xchg(unsigned char *type, unsigned char *buffer, uint16_t *len);
