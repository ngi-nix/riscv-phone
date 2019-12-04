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

typedef void (*eos_sock_fptr_t) (unsigned char *, uint16_t);

void eos_sock_init(void);
int eos_sock_open_udp(void);
void eos_sock_close(int sock);
int eos_sock_sendto(int sock, unsigned char *buffer, uint16_t size, unsigned char more, EOSNetAddr *addr);
void eos_sock_getfrom(unsigned char *buffer, EOSNetAddr *addr);
void eos_sock_set_handler(int sock, eos_sock_fptr_t handler, uint8_t flags);
