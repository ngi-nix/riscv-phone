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

int ecp_rbuf_info_init(ECPRBInfo *rbuf_info) {
    memset(rbuf_info, 0, sizeof(ECPRBInfo));

    return ECP_OK;
}

int ecp_rbuf_pkt_prep(ECPRBSend *buf, ecp_seq_t seq, unsigned char mtype, ECPRBInfo *rbuf_info) {
    if (buf == NULL) return ECP_ERR;
    
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&buf->mutex);
#endif
    int idx = ecp_rbuf_msg_idx(&buf->rbuf, seq);
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&buf->mutex);
#endif

    if (idx < 0) return idx;
    
    rbuf_info->seq = seq;
    rbuf_info->idx = idx;
    rbuf_info->mtype = mtype;
    buf->rbuf.msg[idx].size = 0;
    buf->rbuf.msg[idx].flags = 0;

    return ECP_OK; 
}

ssize_t ecp_rbuf_pkt_send(ECPRBSend *buf, ECPSocket *sock, ECPNetAddr *addr, ECPTimerItem *ti, unsigned char *packet, size_t pkt_size, ECPRBInfo *rbuf_info) {
    unsigned char flags = 0;
    int do_send = 1;
    ssize_t rv = 0;
    ecp_seq_t seq = rbuf_info->seq;
    unsigned int idx = rbuf_info->idx;
    unsigned char mtype = rbuf_info->mtype & ECP_MTYPE_MASK;

    if (mtype < ECP_MAX_MTYPE_SYS) flags |= ECP_RBUF_FLAG_SYS;

    rv = ecp_rbuf_msg_store(&buf->rbuf, seq, idx, packet, pkt_size, 0, flags);
    if (rv < 0) return rv;

    if (buf->flags & ECP_RBUF_FLAG_CCONTROL) {
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&buf->mutex);
#endif

        if (ECP_RBUF_SEQ_LT(buf->rbuf.seq_max, seq)) buf->rbuf.seq_max = seq;

        if (buf->cnt_cc || (buf->in_transit >= buf->win_size)) {
            if (!buf->cnt_cc) buf->seq_cc = seq;
            buf->cnt_cc++;
            buf->rbuf.msg[idx].flags |= ECP_RBUF_FLAG_IN_CCONTROL;
            do_send = 0;
        } else {
            buf->in_transit++;
        }
        
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&buf->mutex);
#endif
    }

    if (do_send) {
        int _rv;
        if (ti) {
            _rv = ecp_timer_push(ti);
            if (_rv) return _rv;
        }
        rv = ecp_pkt_send(sock, addr, packet, pkt_size);
    }
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

