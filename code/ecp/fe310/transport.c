#include <stdlib.h>

#include <core.h>
#include <tr.h>

#include <eos/eos.h>
#include <eos/net.h>

static unsigned char _flags = 0;

ECPSocket *_ecp_tr_sock = NULL;

static void packet_handler(unsigned char *buffer, uint16_t len) {
    ECPNetAddr addr;

    ECP2Buffer bufs;
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pld_buf[ECP_MAX_PLD];

    bufs.packet = &packet;
    bufs.payload = &payload;

    packet.buffer = buffer+EOS_SOCK_SIZE_UDP_HDR;
    packet.size = ECP_MAX_PKT;
    payload.buffer = pld_buf;
    payload.size = ECP_MAX_PLD;

    eos_sock_getfrom(buffer, &addr);
    ssize_t rv = ecp_pkt_handle(_ecp_tr_sock, &addr, NULL, &bufs, len-EOS_SOCK_SIZE_UDP_HDR);
#ifdef ECP_DEBUG
    if (rv < 0) {
        char b[16];
        puts("ERR:");
        puts(itoa(rv, b, 10));
        puts("\n");
    }
#endif
    if (bufs.packet->buffer) eos_net_free(buffer, 0);
    eos_net_release();
}

int ecp_tr_init(ECPContext *ctx) {
    return ECP_OK;
}

int ecp_tr_addr_eq(ECPNetAddr *addr1, ECPNetAddr *addr2) {
    if (addr1->port != addr2->port) return 0;
    if (memcmp(addr1->host, addr2->host, sizeof(addr1->host)) != 0) return 0;
    return 1;
}

int ecp_tr_addr_set(ECPNetAddr *addr, void *addr_s) {
    return ECP_ERR;
}

int ecp_tr_open(ECPSocket *sock, void *addr_s) {
    sock->sock = eos_sock_open_udp();
    if (sock->sock < 0) {
        sock->sock = 0;
        return ECP_ERR_SEND;
    }
    eos_sock_set_handler(sock->sock, packet_handler, 0);
    _ecp_tr_sock = sock;
    return ECP_OK;
}

void ecp_tr_close(ECPSocket *sock) {
    eos_sock_close(sock->sock);
    _ecp_tr_sock = NULL;
}

ssize_t ecp_tr_send(ECPSocket *sock, ECPBuffer *packet, size_t msg_size, ECPNetAddr *addr, unsigned char flags) {
    unsigned char *buf = NULL;
    int rv;

    flags |= _flags;
    if (packet && packet->buffer) {
        if (flags & ECP_SEND_FLAG_REPLY) {
            buf = packet->buffer-EOS_SOCK_SIZE_UDP_HDR;
            packet->buffer = NULL;
        } else {
            buf = eos_net_alloc();
            memcpy(buf+EOS_SOCK_SIZE_UDP_HDR, packet->buffer, msg_size);
        }
    }
    if (buf == NULL) return ECP_ERR;
    rv = eos_sock_sendto(sock->sock, buf, msg_size+EOS_SOCK_SIZE_UDP_HDR, flags & ECP_SEND_FLAG_MORE, addr);
    if (rv) return ECP_ERR_SEND;
    return msg_size;
}

ssize_t ecp_tr_recv(ECPSocket *sock, ECPBuffer *packet, ECPNetAddr *addr, int timeout) {
    return ECP_ERR;
}

void ecp_tr_release(ECPBuffer *packet, unsigned char more) {
    if (packet && packet->buffer) {
        eos_net_free(packet->buffer-EOS_SOCK_SIZE_UDP_HDR, more);
        packet->buffer = NULL;
    } else if (!more) {
        eos_net_release();
    }
}

void ecp_tr_flag_set(unsigned char flags) {
    _flags |= flags;
}

void ecp_tr_flag_clear(unsigned char flags) {
    _flags &= ~flags;
}
