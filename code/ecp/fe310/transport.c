#include <stddef.h>
#include <string.h>

#include <eos/eos.h>
#include <eos/net.h>

#include <core.h>

static int t_addr_eq(ECPNetAddr *addr1, ECPNetAddr *addr2) {
    if (addr1->port != addr2->port) return 0;
    if (memcmp(addr1->host, addr2->host, sizeof(addr1->host)) != 0) return 0;
    return 1;
}

static int t_open(int *sock, void *addr_s) {
    *sock = 0;
    return ECP_OK;
}

static void t_close(int *sock) {
}

static ssize_t t_send(int *sock, ECPBuffer *packet, size_t msg_size, ECPNetAddr *addr, unsigned char flags) {
    unsigned char *buf = NULL;
    size_t addr_len = sizeof(addr->host) + sizeof(addr->port);
    uint16_t buf_size = msg_size + addr_len;
    unsigned char cmd = EOS_NET_CMD_PKT;
    int rv;

    if (flags & ECP_SEND_FLAG_MORE) {
        cmd |= EOS_NET_CMD_FLAG_ONEW;
    }
    if (flags & ECP_SEND_FLAG_REPLY) {
        if (packet && packet->buffer) buf = packet->buffer-addr_len;
    } else {
        buf = eos_net_alloc();
        memcpy(buf+addr_len, packet->buffer, msg_size);
    }
    if (buf == NULL) return ECP_ERR;
    memcpy(buf, addr->host, sizeof(addr->host));
    memcpy(buf+sizeof(addr->host), &addr->port, sizeof(addr->port));
    rv = eos_net_send(cmd, buf, buf_size);
    if (rv) return ECP_ERR_SEND;
    return msg_size;
}

static void t_buf_free(ECP2Buffer *b, unsigned char flags) {
    size_t addr_len = ECP_IPv4_ADDR_SIZE + sizeof(uint16_t);
    if (b && b->packet && b->packet->buffer) eos_net_free(b->packet->buffer-addr_len, flags & ECP_SEND_FLAG_MORE);
}

static void t_buf_flag_set(ECP2Buffer *b, unsigned char flags) {
    size_t addr_len = ECP_IPv4_ADDR_SIZE + sizeof(uint16_t);
    if (flags & ECP_SEND_FLAG_MORE) {
        if (b && b->packet && b->packet->buffer) eos_net_reserve(b->packet->buffer-addr_len);
    }
}

static void t_buf_flag_clear(ECP2Buffer *b, unsigned char flags) {
    if (flags & ECP_SEND_FLAG_MORE) {
        eos_net_release(1);
    }
}

int ecp_transport_init(ECPTransportIface *t) {
    t->init = 1;
    t->open = t_open;
    t->close = t_close;
    t->send = t_send;
    t->addr_eq = t_addr_eq;
    t->buf_free = t_buf_free;
    t->buf_flag_set = t_buf_flag_set;
    t->buf_flag_clear = t_buf_flag_clear;
    return ECP_OK;
}
