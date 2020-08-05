#define ECP_IPv4_ADDR_SIZE          4

typedef int ECPNetSock;
typedef struct ECPNetAddr {
    unsigned char host[ECP_IPv4_ADDR_SIZE];
    uint16_t port;
} ECPNetAddr;

