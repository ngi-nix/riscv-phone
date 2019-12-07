#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "eos.h"
#include "event.h"
#include "net.h"

#include "sock.h"

static eos_evt_fptr_t evt_handler[EOS_SOCK_MAX_SOCK];
static uint16_t evt_handler_flags_buf_free = 0;
static uint16_t evt_handler_flags_buf_acq = 0;

static void sock_handler_evt(unsigned char type, unsigned char *buffer, uint16_t len) {
    if ((buffer == NULL) || (len < 2)) {
        eos_evtq_bad_handler(type, buffer, len);
        eos_net_free(buffer, 0);
        return;
    }

    if (buffer[0] == EOS_SOCK_MTYPE_PKT) {
        uint8_t sock = buffer[1];
        if (sock && (sock <= EOS_SOCK_MAX_SOCK)) {
            sock--;
        } else {
            eos_evtq_bad_handler(type, buffer, len);
            eos_net_free(buffer, 0);
            return;
        }
        _eos_net_handle(type, buffer, len, sock, evt_handler, &evt_handler_flags_buf_free, &evt_handler_flags_buf_acq);
    }
}

void eos_sock_init(void) {
    int i;

    for (i=0; i<EOS_SOCK_MAX_SOCK; i++) {
        evt_handler[i] = eos_evtq_bad_handler;
    }
    eos_net_set_handler(EOS_NET_MTYPE_SOCK, sock_handler_evt, 0);
}

void eos_sock_set_handler(int sock, eos_evt_fptr_t handler, uint8_t flags) {
    if (sock && (sock <= EOS_SOCK_MAX_SOCK)) {
        sock--;
    } else {
        return;
    }
    _eos_net_set_handler(sock, handler, evt_handler, flags, &evt_handler_flags_buf_free, &evt_handler_flags_buf_acq);
}

int eos_sock_open_udp(void) {
    unsigned char type = EOS_SOCK_MTYPE_OPEN_DGRAM;
    unsigned char *buffer = eos_net_alloc();
    uint16_t buf_size;
    int rv, sock;

    buffer[0] = type;
    rv = eos_net_send(EOS_NET_MTYPE_SOCK, buffer, 1, 0);
    if (rv) return rv;

    eos_evtq_get(EOS_NET_MTYPE_SOCK, &type, 1, &buffer, &buf_size);
    if (buf_size < 2) {
        eos_net_free(buffer, 0);
        return EOS_ERR_NET;
    }

    sock = buffer[1];
    eos_net_free(buffer, 1);

    if (sock == 0) return EOS_ERR_NET;
    return sock;
}

void eos_sock_close(int sock) {
    unsigned char *buffer = eos_net_alloc();
    buffer[0] = EOS_SOCK_MTYPE_CLOSE;
    buffer[1] = sock;
    eos_net_send(EOS_NET_MTYPE_SOCK, buffer, 2, 1);
}

int eos_sock_sendto(int sock, unsigned char *buffer, uint16_t size, unsigned char more, EOSNetAddr *addr) {
    unsigned char type = EOS_NET_MTYPE_SOCK;

    buffer[0] = EOS_SOCK_MTYPE_PKT;
    buffer[1] = sock;
    memcpy(buffer+2, addr->host, sizeof(addr->host));
    memcpy(buffer+2+sizeof(addr->host), &addr->port, sizeof(addr->port));
    return eos_net_send(type, buffer, size, more);
}

void eos_sock_getfrom(unsigned char *buffer, EOSNetAddr *addr) {
    memcpy(addr->host, buffer+2, sizeof(addr->host));
    memcpy(&addr->port, buffer+2+sizeof(addr->host), sizeof(addr->port));
}


