#include <core.h>
#include <tr.h>

#include <eos/eos.h>
#include <eos/net.h>

static unsigned char _flags = 0;

int ecp_tr_addr_eq(ECPNetAddr *addr1, ECPNetAddr *addr2) {
    if (addr1->port != addr2->port) return 0;
    if (memcmp(addr1->host, addr2->host, sizeof(addr1->host)) != 0) return 0;
    return 1;
}

int ecp_tr_addr_set(ECPNetAddr *addr, void *addr_s) {
    return ECP_ERR;
}

int ecp_tr_open(int *sock, void *addr_s) {
    *sock = 0;
    return ECP_OK;
}

void ecp_tr_close(int *sock) {
}

ssize_t ecp_tr_send(int *sock, ECPBuffer *packet, size_t msg_size, ECPNetAddr *addr, unsigned char flags) {
    unsigned char *buf = NULL;
    size_t addr_len = sizeof(addr->host) + sizeof(addr->port);
    uint16_t buf_size = msg_size + addr_len;
    unsigned char type = EOS_NET_MTYPE_SOCK;
    int rv;

    flags |= _flags;
    if (flags & ECP_SEND_FLAG_MORE) {
        type |= EOS_NET_MTYPE_FLAG_ONEW;
    }
    if (flags & ECP_SEND_FLAG_REPLY) {
        if (packet && packet->buffer) {
            buf = packet->buffer-addr_len;
            packet->buffer = NULL;
        }
    } else {
        buf = eos_net_alloc();
        memcpy(buf+addr_len, packet->buffer, msg_size);
    }
    if (buf == NULL) return ECP_ERR;
    memcpy(buf, addr->host, sizeof(addr->host));
    memcpy(buf+sizeof(addr->host), &addr->port, sizeof(addr->port));
    rv = eos_net_send(type, buf, buf_size);
    if (rv) return ECP_ERR_SEND;
    return msg_size;
}

ssize_t ecp_tr_recv(int *sock, ECPBuffer *packet, ECPNetAddr *addr, int timeout) {
    return ECP_ERR;
}

void ecp_tr_buf_free(ECP2Buffer *b, unsigned char flags) {
    size_t addr_len = ECP_IPv4_ADDR_SIZE + sizeof(uint16_t);
    if (b && b->packet && b->packet->buffer) {
        eos_net_free(b->packet->buffer-addr_len, flags & ECP_SEND_FLAG_MORE);
        b->packet->buffer = NULL;
    }
}

void ecp_tr_buf_flag_set(ECP2Buffer *b, unsigned char flags) {
    _flags |= flags;
    if (flags & ECP_SEND_FLAG_MORE) ecp_tr_buf_free(b, flags);
}

void ecp_tr_buf_flag_clear(ECP2Buffer *b, unsigned char flags) {
    _flags &= ~flags;
    if (flags & ECP_SEND_FLAG_MORE) eos_net_release();
}
