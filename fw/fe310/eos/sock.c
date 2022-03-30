#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "eos.h"
#include "event.h"
#include "net.h"

#include "sock.h"

static eos_evt_handler_t evt_handler[EOS_SOCK_MAX_SOCK];

static void sock_handle_msg(unsigned char type, unsigned char *buffer, uint16_t len) {
    unsigned char sock;

    if ((buffer == NULL) || (len < 2)) {
        eos_net_bad_handler(type, buffer, len);
        return;
    }

    sock = buffer[1];
    if ((sock == 0) || (sock > EOS_SOCK_MAX_SOCK) || (evt_handler[sock - 1] == NULL)) {
        eos_net_bad_handler(type, buffer, len);
        return;
    }

    switch(buffer[0]) {
        case EOS_SOCK_MTYPE_PKT:
            evt_handler[sock - 1](type, buffer, len);
            break;
        default:
            eos_net_bad_handler(type, buffer, len);
            break;
    }
}

void eos_sock_netinit(void) {
    int i;

    for (i=0; i<EOS_SOCK_MAX_SOCK; i++) {
        evt_handler[i] = NULL;
    }
    eos_net_set_handler(EOS_NET_MTYPE_SOCK, sock_handle_msg);
}

void eos_sock_set_handler(unsigned char sock, eos_evt_handler_t handler) {
    if (sock && (sock <= EOS_SOCK_MAX_SOCK)) evt_handler[sock - 1] = handler;
}

eos_evt_handler_t eos_sock_get_handler(unsigned char sock) {
    if (sock && (sock <= EOS_SOCK_MAX_SOCK)) return evt_handler[sock - 1];
    return NULL;
}

int eos_sock_open_udp(eos_evt_handler_t handler, unsigned char *buffer) {
    unsigned char type;
    uint16_t len;
    int do_release;
    int rv, sock;

    do_release = 0;
    if (buffer == NULL) {
        buffer = eos_net_alloc();
        do_release = 1;
    }

    type = EOS_NET_MTYPE_SOCK;
    len = 1;
    buffer[0] = EOS_SOCK_MTYPE_OPEN_DGRAM;

    rv = eos_net_xchg(&type, buffer, &len);
    if (rv) return rv;

    if (type != EOS_NET_MTYPE_SOCK) return EOS_ERR_NET;
    if (len < 2) return EOS_ERR_SIZE;

    sock = buffer[1];
    if (sock == 0) return EOS_ERR_NET;

    if (do_release) {
        eos_net_free(buffer, 1);
    }
    eos_sock_set_handler(sock, handler);

    return sock;
}

void eos_sock_close(unsigned char sock, unsigned char *buffer) {
    int async;

    async = 0;
    if (buffer == NULL) {
        buffer = eos_net_alloc();
        async = 1;
    }
    buffer[0] = EOS_SOCK_MTYPE_CLOSE;
    buffer[1] = sock;
    _eos_net_send(EOS_NET_MTYPE_SOCK, buffer, 2, async, 1);
    eos_sock_set_handler(sock, NULL);
}

static int sock_send(unsigned char sock, unsigned char *msg, uint16_t msg_len, EOSNetAddr *addr, unsigned char *buffer) {
    buffer[0] = EOS_SOCK_MTYPE_PKT;
    buffer[1] = sock;
    buffer += 2;
    memcpy(buffer, addr->host, sizeof(addr->host));
    buffer += sizeof(addr->host);
    buffer[0] = addr->port >> 8;
    buffer[1] = addr->port;
    buffer += sizeof(addr->port);
    if (msg) {
        if (msg_len + EOS_SOCK_SIZE_UDP_HDR > EOS_NET_MTU) return EOS_ERR_SIZE;
        memcpy(buffer, msg, msg_len);
    }

    return EOS_OK;
}

int eos_sock_sendto(unsigned char sock, unsigned char *msg, size_t msg_len, EOSNetAddr *addr, unsigned char *buffer) {
    int rv;

    rv = sock_send(sock, msg, msg_len, addr, buffer);
    if (rv) return rv;

    return eos_net_send(EOS_NET_MTYPE_SOCK, buffer, msg_len + EOS_SOCK_SIZE_UDP_HDR);
}

int eos_sock_sendto_async(unsigned char sock, unsigned char *msg, size_t msg_len, EOSNetAddr *addr, unsigned char *buffer, unsigned char more) {
    int rv;

    rv = sock_send(sock, msg, msg_len, addr, buffer);
    if (rv) return rv;

    return eos_net_send_async(EOS_NET_MTYPE_SOCK, buffer, msg_len + EOS_SOCK_SIZE_UDP_HDR, more);
}

int eos_sock_recvfrom(unsigned char *buffer, uint16_t len, unsigned char *msg, size_t msg_size, EOSNetAddr *addr) {
    if (len < EOS_SOCK_SIZE_UDP_HDR) return EOS_ERR_SIZE;

    if (buffer[0] != EOS_SOCK_MTYPE_PKT) return EOS_ERR_NET;

    buffer += 2;
    memcpy(addr->host, buffer, sizeof(addr->host));
    buffer += sizeof(addr->host);
    addr->port  = (uint16_t)buffer[0] << 8;
    addr->port |= (uint16_t)buffer[1];
    buffer += sizeof(addr->port);
    if (msg) {
        if (msg_size < len - EOS_SOCK_SIZE_UDP_HDR) return EOS_ERR_SIZE;
        memcpy(msg, buffer, len - EOS_SOCK_SIZE_UDP_HDR);
    }

    return EOS_OK;
}
