#include <stdint.h>
#include "event.h"

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
void eos_sock_set_handler(unsigned char sock, eos_evt_handler_t handler);
eos_evt_handler_t eos_sock_get_handler(unsigned char sock);

int eos_sock_open_udp(eos_evt_handler_t handler);
void eos_sock_close(unsigned char sock);
int eos_sock_sendto(unsigned char sock, unsigned char *buffer, uint16_t size, unsigned char more, EOSNetAddr *addr);
void eos_sock_getfrom(unsigned char *buffer, EOSNetAddr *addr);
