#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <core.h>
#include <tr.h>

#include <eos/eos.h>
#include <eos/net.h>

ECPSocket *_ecp_tr_sock = NULL;
unsigned char pld_buf[ECP_MAX_PLD];

static void packet_handler(unsigned char type, unsigned char *buffer, uint16_t len) {
    ECP2Buffer bufs;
    ECPBuffer packet;
    ECPBuffer payload;
    ecp_tr_addr_t addr;
    ssize_t rv;

    bufs.packet = &packet;
    bufs.payload = &payload;

    packet.buffer = buffer+EOS_SOCK_SIZE_UDP_HDR;
    packet.size = EOS_NET_MTU-EOS_SOCK_SIZE_UDP_HDR;
    payload.buffer = pld_buf;
    payload.size = ECP_MAX_PLD;

    if (len < EOS_SOCK_SIZE_UDP_HDR) {
        eos_net_free(buffer, 0);
        return;
    }

    eos_sock_recvfrom(buffer, len, NULL, 0, &addr);

    rv = ecp_pkt_handle(_ecp_tr_sock, NULL, &addr, &bufs, len-EOS_SOCK_SIZE_UDP_HDR);
#ifdef ECP_DEBUG
    if (rv < 0) {
        printf("RCV ERR:%d\n", rv);
    }
#endif
    if (bufs.packet->buffer) {
        eos_net_free(buffer, 0);
    } else {
        eos_net_release();
    }
}

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
    return ECP_ERR;
}

int ecp_tr_open(ECPSocket *sock, void *addr_s) {
    sock->sock = eos_sock_open_udp(packet_handler, NULL);
    if (sock->sock < 0) {
        sock->sock = 0;
        return ECP_ERR;
    }
    _ecp_tr_sock = sock;

    return ECP_OK;
}

void ecp_tr_close(ECPSocket *sock) {
    eos_sock_close(sock->sock, NULL);
    _ecp_tr_sock = NULL;
}

ssize_t ecp_tr_send(ECPSocket *sock, ECPBuffer *packet, size_t pkt_size, ecp_tr_addr_t *addr, unsigned char flags) {
    unsigned char *buf = NULL;
    int reply;
    int rv;

    reply = 0;
    if (flags & ECP_SEND_FLAG_REPLY) {
        if (flags & ECP_SEND_FLAG_MORE) return ECP_ERR;
        if (packet->buffer) {
            buf = packet->buffer-EOS_SOCK_SIZE_UDP_HDR;
            packet->buffer = NULL;
            reply = 1;
        }
    } else {
        buf = eos_net_alloc();
    }
    if (buf == NULL) return ECP_ERR;

    rv = eos_sock_sendto_async(sock->sock, reply ? NULL : packet->buffer, pkt_size, addr, buf, !!(flags & ECP_SEND_FLAG_MORE));
    if (rv) return ECP_ERR_SEND;

    return pkt_size;
}

ssize_t ecp_tr_recv(ECPSocket *sock, ECPBuffer *packet, ecp_tr_addr_t *addr, int timeout) {
    return ECP_ERR;
}

void ecp_tr_release(ECPBuffer *packet, unsigned char more) {
    if (packet->buffer) {
        eos_net_free(packet->buffer-EOS_SOCK_SIZE_UDP_HDR, more);
        packet->buffer = NULL;
    } else if (!more) {
        eos_net_release();
    }
}
