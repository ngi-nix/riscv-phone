#include <stdint.h>

#define EOS_NET_MTYPE_SOCK          1
#define EOS_NET_MTYPE_AUDIO         2

#define EOS_NET_MTYPE_WIFI          5
#define EOS_NET_MTYPE_CELL          6
#define EOS_NET_MTYPE_SIP           7
#define EOS_NET_MTYPE_APP           8

#define EOS_NET_MAX_MTYPE           8

#define EOS_NET_MTYPE_FLAG_ONEW     0x10

#define EOS_NET_SIZE_BUF            1500
#define EOS_NET_SIZE_BUFQ           64

#define EOS_NET_FLAG_BUF_FREE       0x01

typedef void (*eos_net_fptr_t) (unsigned char, unsigned char *, uint16_t);

void eos_net_init(void);
int eos_net_send(unsigned char mtype, unsigned char *buffer, uint16_t len, uint8_t flags);
void eos_net_set_handler(unsigned char mtype, eos_net_fptr_t handler);