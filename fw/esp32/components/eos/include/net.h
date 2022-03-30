#include <stdint.h>

/* common */
#define EOS_NET_MTU                 1500
#define EOS_NET_SIZE_BUF            EOS_NET_MTU

#define EOS_NET_MTYPE_SOCK          1
#define EOS_NET_MTYPE_RNG         	3
#define EOS_NET_MTYPE_POWER         4

#define EOS_NET_MTYPE_WIFI          5
#define EOS_NET_MTYPE_CELL          6
#define EOS_NET_MTYPE_SIP           7
#define EOS_NET_MTYPE_APP           8

#define EOS_NET_MAX_MTYPE           8

#define EOS_NET_MTYPE_FLAG_ONEW     0x40
#define EOS_NET_MTYPE_FLAG_REPL     0x80
#define EOS_NET_MTYPE_FLAG_MASK     0xc0

/* esp32 specific */
#define EOS_NET_SIZE_BUFQ           4
#define EOS_NET_SIZE_SNDQ           4

typedef void (*eos_net_fptr_t) (unsigned char, unsigned char *, uint16_t);

void eos_net_init(void);

unsigned char *eos_net_alloc(void);
void eos_net_free(unsigned char *buf);
int eos_net_send(unsigned char mtype, unsigned char *buffer, uint16_t len);
void eos_net_reply(unsigned char mtype, unsigned char *buffer, uint16_t len);
void eos_net_set_handler(unsigned char mtype, eos_net_fptr_t handler);
void eos_net_sleep_done(uint8_t mode);
void eos_net_wake(uint8_t source, uint8_t mode);
