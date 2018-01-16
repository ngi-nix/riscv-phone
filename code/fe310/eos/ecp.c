#include <stddef.h>
#include <string.h>

#include "encoding.h"
#include "platform.h"

#include "event.h"
#include "timer.h"
#include "net.h"

#include "ecp.h"

static ECPSocket *_sock = NULL;

static unsigned char net_buf_reserved = 0;
static void timer_handler(unsigned char cmd, unsigned char *buffer, uint16_t len) {
    int ok = eos_net_acquire(net_buf_reserved);
    if (ok) {
        ecp_cts_t next = ecp_timer_exe(_sock);
        if (next) {
            uint32_t tick = next * (uint64_t)RTC_FREQ / 1000;
            eos_timer_set(tick, 1);
        }
        eos_net_release(1);
        net_buf_reserved = 0;
    } else {
        net_buf_reserved = 1;
        eos_evtq_push(EOS_EVT_TIMER, NULL, 0);
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
    if (bufs.packet->buffer) eos_net_free(buffer, 0);
    eos_net_release(0);
}

int ecp_init(ECPContext *ctx) {
    int rv;
    
    rv = ecp_ctx_create(ctx);
    if (rv) return rv;
    
    eos_evtq_set_handler(EOS_EVT_TIMER, timer_handler);
    eos_evtq_set_handler(EOS_EVT_MASK_NET | EOS_NET_CMD_PKT, packet_handler);

    return ECP_OK;
}