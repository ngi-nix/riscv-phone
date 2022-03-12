#define ECP_IPv4_ADDR_SIZE          4

struct ECPNetAddr {
    unsigned char host[ECP_IPv4_ADDR_SIZE];
    uint16_t port;
};

typedef struct ECPNetAddr ecp_tr_addr_t;
typedef int ecp_tr_sock_t;
