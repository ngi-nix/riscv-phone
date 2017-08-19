#include "core.h"

#include <string.h>

int ecp_rbuf_init(ECPRBuffer *rbuf, ECPRBMessage *msg, unsigned int msg_size) {
    rbuf->msg = msg;
    if (msg_size) {
        if (msg == NULL) return ECP_ERR;
        rbuf->msg_size = msg_size;
        memset(rbuf->msg, 0, sizeof(ECPRBMessage) * msg_size);
    } else {
        rbuf->msg_size = ECP_RBUF_SEQ_HALF;
    }

    return ECP_OK;
}

int ecp_rbuf_start(ECPRBuffer *rbuf, ecp_seq_t seq) {
    rbuf->seq_max = seq;
    rbuf->seq_start = seq + 1;

    return ECP_OK;
}

int ecp_rbuf_msg_idx(ECPRBuffer *rbuf, ecp_seq_t seq) {
    ecp_seq_t seq_offset = seq - rbuf->seq_start;

    // This also checks for seq_start <= seq if seq type range >> rbuf->msg_size
    if (seq_offset < rbuf->msg_size) return ECP_RBUF_IDX_MASK(rbuf->msg_start + seq_offset, rbuf->msg_size);
    return ECP_ERR_RBUF_FULL;
}

ssize_t ecp_rbuf_msg_store(ECPRBuffer *rbuf, ecp_seq_t seq, int idx, unsigned char *msg, size_t msg_size, unsigned char test_flags, unsigned char set_flags) {
    idx = idx < 0 ? ecp_rbuf_msg_idx(rbuf, seq) : idx;
    if (idx < 0) return idx;

    if (rbuf->msg == NULL) return 0;
    if (test_flags && (test_flags & rbuf->msg[idx].flags)) return ECP_ERR_RBUF_DUP;
    
    if (msg_size) memcpy(rbuf->msg[idx].msg, msg, msg_size);
    rbuf->msg[idx].size = msg_size;
    rbuf->msg[idx].flags = set_flags;

    return msg_size;
}

int ecp_rbuf_info_init(ECPRBInfo *rbuf_info) {
    memset(rbuf_info, 0, sizeof(ECPRBInfo));

    return ECP_OK;
}

ssize_t ecp_rbuf_pld_send(ECPConnection *conn, unsigned char *payload, size_t payload_size, ecp_seq_t seq) {
    unsigned char packet[ECP_MAX_PKT];
    ECPSocket *sock = conn->sock;
    ECPContext *ctx = sock->ctx;
    ECPNetAddr addr;
    ECPRBInfo rbuf_info;
    ssize_t rv;
    
    int _rv = ecp_rbuf_info_init(&rbuf_info);
    if (_rv) return _rv;
    
    rbuf_info.seq = seq;
    rbuf_info.seq_force = 1;

    rv = ctx->pack(conn, packet, ECP_MAX_PKT, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, payload, payload_size, &addr, &rbuf_info);
    if (rv < 0) return rv;

    rv = ecp_pkt_send(sock, &addr, packet, rv);
    if (rv < 0) return rv;

    return rv;
}

int ecp_rbuf_conn_create(ECPConnection *conn, ECPRBSend *buf_s, ECPRBMessage *msg_s, unsigned int msg_s_size, ECPRBRecv *buf_r, ECPRBMessage *msg_r, unsigned int msg_r_size) {
    int rv;
    
    rv = ecp_rbuf_send_create(conn, buf_s, msg_s, msg_s_size);
    if (rv) return rv;
    
    rv = ecp_rbuf_recv_create(conn, buf_r, msg_r, msg_r_size);
    if (rv) {
        ecp_rbuf_send_destroy(conn);
        return rv;
    }
    
    return ECP_OK;
}

void ecp_rbuf_conn_destroy(ECPConnection *conn) {
    ecp_rbuf_send_destroy(conn);
    ecp_rbuf_recv_destroy(conn);
}

int ecp_rbuf_conn_start(ECPConnection *conn, ecp_seq_t seq) {
    int rv = ecp_rbuf_send_start(conn);
    if (rv) return rv;

    if (!conn->out) {
        rv = ecp_rbuf_recv_start(conn, seq);
        if (rv) return rv;
    }
    
    return ECP_OK;
}

