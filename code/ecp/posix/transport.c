#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#include <core.h>

#define ADDR_S_MAX  32

static int t_addr_eq(ECPNetAddr *addr1, ECPNetAddr *addr2) {
    if (addr1->port != addr2->port) return 0;
    if (memcmp(addr1->host, addr2->host, sizeof(addr1->host)) != 0) return 0;
    return 1;
}

static int t_addr_set(ECPNetAddr *addr, void *addr_s) {
    int rv;
    char addr_c[ADDR_S_MAX];
    char *colon = NULL;
    char *endptr = NULL;
	uint16_t hport;
    
    memset(addr_c, 0, sizeof(addr_c));
    strncpy(addr_c, addr_s, ADDR_S_MAX-1);
    colon = strchr(addr_c, ':');
    if (colon == NULL) return -1;
    *colon = '\0';
    colon++;
    if (*colon == '\0') return -1;
    rv = inet_pton(AF_INET, addr_c, addr->host);
    if (rv != 1) return -1;
    hport = strtol(colon, &endptr, 10);
    if (*endptr != '\0') return -1;
    addr->port = htons(hport);

    return 0;
}

static int t_open(int *sock, void *addr_s) {
    struct sockaddr_in _myaddr;

    memset((char *)&_myaddr, 0, sizeof(_myaddr));
    _myaddr.sin_family = AF_INET;
    if (addr_s) {
        ECPNetAddr addr;
        int rv = t_addr_set(&addr, addr_s);
        if (rv) return rv;

        memcpy((void *)&_myaddr.sin_addr, addr.host, sizeof(addr.host));
        _myaddr.sin_port = addr.port;
    } else {
        _myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        _myaddr.sin_port = htons(0);
    }

    *sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (*sock < 0) return *sock;

    int rv = bind(*sock, (struct sockaddr *)&_myaddr, sizeof(_myaddr));
    if (rv < 0) {
        close(*sock);
        return rv;
    }
    return 0;
}

static void t_close(int *sock) {
    close(*sock);
}

static ssize_t t_send(int *sock, ECPBuffer *packet, size_t msg_size, ECPNetAddr *addr, unsigned char flags) {
    struct sockaddr_in servaddr;

    memset((void *)&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = addr->port;
    memcpy((void *)&servaddr.sin_addr, addr->host, sizeof(addr->host));
    return sendto(*sock, packet->buffer, msg_size, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
}

static ssize_t t_recv(int *sock, ECPBuffer *packet, ECPNetAddr *addr, int timeout) {
    struct sockaddr_in servaddr;
    socklen_t addrlen = sizeof(servaddr);
    struct pollfd fds[] = {
        {*sock, POLLIN, 0}
    };

    int rv = poll(fds, 1, timeout);
    memset((void *)&servaddr, 0, sizeof(servaddr));
    if (rv == 1) {
        ssize_t recvlen = recvfrom(*sock, packet->buffer, packet->size, 0, (struct sockaddr *)&servaddr, &addrlen);
        if (recvlen < 0) return recvlen;
        if (recvlen < ECP_MIN_PKT) return ECP_ERR_RECV;

        if (addr) {
            addr->port = servaddr.sin_port;
            memcpy(addr->host, (void *)&servaddr.sin_addr, sizeof(addr->host));
        }
        return recvlen;
    }
    return ECP_ERR_TIMEOUT;
}

int ecp_transport_init(ECPTransportIface *t) {
    t->init = 1;
    t->open = t_open;
    t->close = t_close;
    t->send = t_send;
    t->recv = t_recv;
    t->addr_eq = t_addr_eq;
    t->addr_set = t_addr_set;
    return ECP_OK;
}
