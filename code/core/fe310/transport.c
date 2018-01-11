#include <stddef.h>
#include <string.h>

#include <core.h>

#include "eos.h"
#include "net.h"

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

static ssize_t t_send(int *sock, void *msg, size_t msg_size, ECPNetAddr *addr) {
    unsigned char *buf = msg;
    size_t addr_len = sizeof(addr->host) + sizeof(addr->port);
    uint16_t buf_size = msg_size + addr_len;
    int rv;

    buf -= addr_len;
    rv = eos_net_send(EOS_NET_CMD_PKT, buf, buf_size);
    if (rv) return ECP_ERR_SEND;
    return msg_size;
}

int ecp_transport_init(ECPTransportIface *t) {
    t->init = 1;
    t->open = t_open;
    t->close = t_close;
    t->send = t_send;
    t->recv = NULL;
    t->addr_eq = t_addr_eq;
    t->addr_set = NULL;
    return ECP_OK;
}
