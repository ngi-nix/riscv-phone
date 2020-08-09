#include "core.h"

int ecp_rbuf_create(ECPConnection *conn, ECPRBSend *buf_s, ECPRBMessage *msg_s, unsigned int msg_s_size, ECPRBRecv *buf_r, ECPRBMessage *msg_r, unsigned int msg_r_size) {
    int rv;
    
    if (buf_s) {
        rv = ecp_rbuf_send_create(conn, buf_s, msg_s, msg_s_size);
        if (rv) return rv;
        
        rv = ecp_rbuf_send_start(conn);
        if (rv) {
            ecp_rbuf_send_destroy(conn);
            return rv;
        }
    }
    
    if (buf_r) {
        rv = ecp_rbuf_recv_create(conn, buf_r, msg_r, msg_r_size);
        if (rv) {
            if (buf_s) ecp_rbuf_send_destroy(conn);
            return rv;
        }
    }
    
    return ECP_OK;
}

void ecp_rbuf_destroy(ECPConnection *conn) {
    ecp_rbuf_send_destroy(conn);
    ecp_rbuf_recv_destroy(conn);
}

int ecp_rbuf_init(ECPRBuffer *rbuf, ECPRBMessage *msg, unsigned int msg_size) {
    rbuf->msg = msg;
    if (msg_size) {
        if (msg == NULL) return ECP_ERR;
        rbuf->msg_size = msg_size;
        memset(rbuf->msg, 0, sizeof(ECPRBMessage) * msg_size);
    } else {
        rbuf->msg_size = ECP_SEQ_HALF;
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

ssize_t ecp_rbuf_pld_send(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pld_size, unsigned char flags, ecp_seq_t seq) {
    ECPSocket *sock = conn->sock;
    ECPContext *ctx = sock->ctx;
    ECPNetAddr addr;
    ECPSeqItem seq_item;
    ssize_t rv;
    
    int _rv = ecp_seq_item_init(&seq_item);
    if (_rv) return _rv;
    
    seq_item.seq = seq;
    seq_item.seq_w = 1;
    seq_item.rb_pass = 1;

    rv = ecp_pack(conn, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, payload, pld_size, &seq_item, &addr);
    if (rv < 0) return rv;

    rv = ecp_pkt_send(sock, &addr, packet, rv, flags);
    if (rv < 0) return rv;

    return rv;
}
