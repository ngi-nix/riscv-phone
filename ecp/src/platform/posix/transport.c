#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#include <core.h>
#include <tr.h>

#define MAX_ADDR_STR    32

int ecp_tr_init(ECPContext *ctx) {
    return ECP_OK;
}

unsigned int ecp_tr_addr_hash(ecp_tr_addr_t *addr) {
    unsigned int ret = *((unsigned int *)addr->host);
    return ret ^ ((unsigned int)addr->port << 16);
}

int ecp_tr_addr_eq(ecp_tr_addr_t *addr1, ecp_tr_addr_t *addr2) {
    if (addr1->port != addr2->port) return 0;
    if (memcmp(addr1->host, addr2->host, sizeof(addr1->host)) != 0) return 0;
    return 1;
}

int ecp_tr_addr_set(ecp_tr_addr_t *addr, void *addr_s) {
    int rv;
    char addr_c[MAX_ADDR_STR];
    char *colon = NULL;
    char *endptr = NULL;
	uint16_t hport;

    memset(addr_c, 0, sizeof(addr_c));
    strncpy(addr_c, addr_s, sizeof(addr_c)-1);
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

int ecp_tr_open(ECPSocket *sock, void *addr_s) {
    struct sockaddr_in _myaddr;
    int rv;

    memset((char *)&_myaddr, 0, sizeof(_myaddr));
    _myaddr.sin_family = AF_INET;
    if (addr_s) {
        ecp_tr_addr_t addr;

        rv = ecp_tr_addr_set(&addr, addr_s);
        if (rv) return rv;

        memcpy((void *)&_myaddr.sin_addr, addr.host, sizeof(addr.host));
        _myaddr.sin_port = addr.port;
    } else {
        _myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        _myaddr.sin_port = htons(0);
    }

    sock->sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock->sock < 0) return sock->sock;

    rv = bind(sock->sock, (struct sockaddr *)&_myaddr, sizeof(_myaddr));
    if (rv < 0) {
        close(sock->sock);
        return rv;
    }

    return ECP_OK;
}

void ecp_tr_close(ECPSocket *sock) {
    close(sock->sock);
}

ssize_t ecp_tr_send(ECPSocket *sock, ECPBuffer *packet, size_t pkt_size, ecp_tr_addr_t *addr, unsigned char flags) {
    struct sockaddr_in servaddr;

    memset((void *)&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = addr->port;
    memcpy((void *)&servaddr.sin_addr, addr->host, sizeof(addr->host));
    return sendto(sock->sock, packet->buffer, pkt_size, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
}

ssize_t ecp_tr_recv(ECPSocket *sock, ECPBuffer *packet, ecp_tr_addr_t *addr, int timeout) {
    struct sockaddr_in servaddr;
    socklen_t addrlen = sizeof(servaddr);
    struct pollfd fds[] = {
        {sock->sock, POLLIN, 0}
    };
    int rv;

    rv = poll(fds, 1, timeout);
    memset((void *)&servaddr, 0, sizeof(servaddr));
    if (rv == 1) {
        ssize_t recvlen = recvfrom(sock->sock, packet->buffer, packet->size, 0, (struct sockaddr *)&servaddr, &addrlen);
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

void ecp_tr_release(ECPBuffer *packet, unsigned char more) {}
