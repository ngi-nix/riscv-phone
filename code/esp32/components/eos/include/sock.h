#include <stdint.h>

#define EOS_SOCK_MTYPE_PKT          0
#define EOS_SOCK_MTYPE_OPEN_DGRAM   1
#define EOS_SOCK_MTYPE_CLOSE        127

#define EOS_SOCK_MAX_SOCK           2

#define EOS_SOCK_SIZE_UDP_HDR       8

#define EOS_IPv4_ADDR_SIZE          4

typedef struct EOSNetAddr {
    unsigned char host[EOS_IPv4_ADDR_SIZE];
    uint16_t port;
} EOSNetAddr;

void eos_sock_init(void);