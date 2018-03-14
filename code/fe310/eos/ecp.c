#include <stddef.h>
#include <string.h>

#include "encoding.h"
#include "platform.h"

#include "event.h"
#include "timer.h"
#include "net.h"

#include "ecp.h"

static ECPSocket *_sock = NULL;

static void timer_handler(unsigned char cmd, unsigned char *buffer, uint16_t len) {
    ecp_cts_t next = ecp_timer_exe(_sock);
    if (next) {
        uint32_t tick = next * (uint64_t)RTC_FREQ / 1000;
        eos_timer_set(tick, 1);
    }
}

static void packet_handler(unsigned char cmd, unsigned char *buffer, uint16_t len) {
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
    if (rv < 0) DPRINT(rv, "ERR:packet_handler - RV:%d\n", rv);
    if (bufs.packet->buffer) eos_net_free(buffer, 0);
    eos_net_release(0);
}

int ecp_init(ECPContext *ctx) {
    int rv;
    
    rv = ecp_ctx_create_vconn(ctx);
    if (rv) return rv;
    
    eos_evtq_set_handler(EOS_EVT_TIMER, timer_handler, EOS_EVT_FLAG_WRAP);
    eos_net_set_handler(EOS_NET_CMD_PKT, packet_handler, 0);
    return ECP_OK;
}

void ecp_sock_set(ECPSocket *s) {
    _sock = s;
}