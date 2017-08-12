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

int ecp_rbuf_msg_idx(ECPRBuffer *rbuf, ecp_seq_t seq) {
    ecp_seq_t seq_offset = seq - rbuf->seq_start;

    // This also checks for seq_start <= seq if seq type range >> rbuf->msg_size
    if (seq_offset < rbuf->msg_size) return ECP_RBUF_IDX_MASK(rbuf->msg_start + seq_offset, rbuf->msg_size);
    return ECP_ERR_RBUF_IDX;
}

ssize_t ecp_rbuf_msg_store(ECPRBuffer *rbuf, ecp_seq_t seq, int idx, unsigned char *msg, size_t msg_size, unsigned char test_flags, unsigned char set_flags) {
    idx = idx < 0 ? ecp_rbuf_msg_idx(rbuf, seq) : idx;
    if (idx < 0) return idx;

    if (rbuf->msg == NULL) return 0;
    if (test_flags && (test_flags & rbuf->msg[idx].flags)) return ECP_ERR_RBUF_FLAG;
    
    if (!(set_flags & ECP_RBUF_FLAG_DELIVERED)) {
        memcpy(rbuf->msg[idx].msg, msg, msg_size);
        rbuf->msg[idx].size = msg_size;
    }
    rbuf->msg[idx].flags = set_flags;

    return msg_size;
}

int ecp_conn_rbuf_start(ECPConnection *conn, ecp_seq_t seq) {
    int rv = ecp_conn_rbuf_send_start(conn);
    if (rv) return rv;

    if (!conn->out) {
        rv = ecp_conn_rbuf_recv_start(conn, seq);
        if (rv) return rv;
    }
    
    return ECP_OK;
}

ssize_t ecp_conn_rbuf_pkt_send(ECPConnection *conn, ECPNetAddr *addr, unsigned char *packet, size_t pkt_size, ecp_seq_t seq, int idx) {
    int do_send;
    ECPRBSend *buf = conn->rbuf.send;
    
    ssize_t rv = ecp_rbuf_msg_store(&buf->rbuf, seq, idx, packet, pkt_size, 0, 0);
    if (rv < 0) return rv;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&buf->mutex);
#endif
    if (buf->in_transit < buf->win_size) {
        buf->in_transit++;
        do_send = 1;
    }
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&buf->mutex);
#endif

    if (do_send) rv = ecp_pkt_send(conn->sock, addr, packet, pkt_size);
    return rv;
}

