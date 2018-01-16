#include <stdint.h>

#define EOS_IPv4_ADDR_SIZE          4

typedef struct EOSNetAddr {
    unsigned char host[EOS_IPv4_ADDR_SIZE];
    uint16_t port;
} EOSNetAddr;

void eos_net_init(void);
void eos_net_connect(char *ssid, char *password);
void eos_net_disconnect(void);
ssize_t eos_net_send(void *msg, size_t msg_size, EOSNetAddr *addr);
