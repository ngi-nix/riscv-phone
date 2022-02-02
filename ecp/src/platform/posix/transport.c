#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#include <core.h>
#include <tr.h>

#define ADDR_S_MAX  32

int ecp_tr_init(ECPContext *ctx) {
    return ECP_OK;
}

int ecp_tr_addr_eq(ECPNetAddr *addr1, ECPNetAddr *addr2) {
    if (addr1->port != addr2->port) return 0;
    if (memcmp(addr1->host, addr2->host, sizeof(addr1->host)) != 0) return 0;
    return 1;
}

int ecp_tr_addr_set(ECPNetAddr *addr, void *addr_s) {
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

int ecp_tr_open(ECPSocket *sock, void *addr_s) {
    struct sockaddr_in _myaddr;

    memset((char *)&_myaddr, 0, sizeof(_myaddr));
    _myaddr.sin_family = AF_INET;
    if (addr_s) {
        ECPNetAddr addr;
        int rv = ecp_tr_addr_set(&addr, addr_s);
        if (rv) return rv;

        memcpy((void *)&_myaddr.sin_addr, addr.host, sizeof(addr.host));
        _myaddr.sin_port = addr.port;
    } else {
        _myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        _myaddr.sin_port = htons(0);
    }

    sock->sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock->sock < 0) return sock->sock;

    int rv = bind(sock->sock, (struct sockaddr *)&_myaddr, sizeof(_myaddr));
    if (rv < 0) {
        close(sock->sock);
        return rv;
    }
    return ECP_OK;
}

void ecp_tr_close(ECPSocket *sock) {
    close(sock->sock);
}

ssize_t ecp_tr_send(ECPSocket *sock, ECPBuffer *packet, size_t msg_size, ECPNetAddr *addr, unsigned char flags) {
    struct sockaddr_in servaddr;

    memset((void *)&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = addr->port;
    memcpy((void *)&servaddr.sin_addr, addr->host, sizeof(addr->host));
    return sendto(sock->sock, packet->buffer, msg_size, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
}

ssize_t ecp_tr_recv(ECPSocket *sock, ECPBuffer *packet, ECPNetAddr *addr, int timeout) {
    struct sockaddr_in servaddr;
    socklen_t addrlen = sizeof(servaddr);
    struct pollfd fds[] = {
        {sock->sock, POLLIN, 0}
    };

    int rv = poll(fds, 1, timeout);
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
void ecp_tr_flag_set(unsigned char flags) {}
void ecp_tr_flag_clear(unsigned char flags) {}

