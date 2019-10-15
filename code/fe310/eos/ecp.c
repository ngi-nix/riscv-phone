#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "encoding.h"
#include "platform.h"

#include "event.h"
#include "timer.h"
#include "net.h"

#include "ecp.h"

static ECPSocket *_sock = NULL;

static void timer_handler(unsigned char type) {
    ecp_cts_t next = ecp_timer_exe(_sock);
    if (next) {
        volatile uint64_t *mtime = (uint64_t *) (CLINT_CTRL_ADDR + CLINT_MTIME);
        uint64_t tick = *mtime + next * (uint64_t)RTC_FREQ / 1000;
        eos_timer_set(tick, EOS_TIMER_ETYPE_ECP, 0);
    }
}

static void packet_handler(unsigned char type, unsigned char *buffer, uint16_t len) {
    ECPNetAddr addr;
    size_t addr_len = sizeof(addr.host) + sizeof(addr.port);
    
    ECP2Buffer bufs;
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pld_buf[ECP_MAX_PLD];
    
    bufs.packet = &packet;
    bufs.payload = &payload;

    packet.buffer = buffer+addr_len;
    packet.size = ECP_MAX_PKT;
    payload.buffer = pld_buf;
    payload.size = ECP_MAX_PLD;

    memcpy(addr.host, buffer, sizeof(addr.host));
    memcpy(&addr.port, buffer+sizeof(addr.host), sizeof(addr.port));
    ssize_t rv = ecp_pkt_handle(_sock, &addr, NULL, &bufs, len-addr_len);
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

int ecp_init(ECPContext *ctx) {
    int rv;
    
    rv = ecp_ctx_create_vconn(ctx);
    if (rv) return rv;
    
    eos_timer_set_handler(EOS_TIMER_ETYPE_ECP, timer_handler, EOS_EVT_FLAG_NET_BUF_ACQ);
    /* XXX */
    // eos_net_set_handler(EOS_NET_DATA_PKT, packet_handler, 0);
    return ECP_OK;
}

void ecp_sock_set(ECPSocket *s) {
    _sock = s;
}