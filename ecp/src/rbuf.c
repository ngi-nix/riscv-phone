#include "core.h"

int _ecp_rbuf_init(ECPRBuffer *rbuf, ECPRBMessage *msg, unsigned int msg_size) {
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

int _ecp_rbuf_start(ECPRBuffer *rbuf, ecp_seq_t seq) {
    rbuf->seq_max = seq;
    rbuf->seq_start = seq + 1;

    return ECP_OK;
}

int _ecp_rbuf_msg_idx(ECPRBuffer *rbuf, ecp_seq_t seq) {
    ecp_seq_t seq_offset = seq - rbuf->seq_start;

    // This also checks for seq_start <= seq if seq type range >> rbuf->msg_size
    if (seq_offset < rbuf->msg_size) return ECP_RBUF_IDX_MASK(rbuf->msg_start + seq_offset, rbuf->msg_size);
    return ECP_ERR_RBUF_FULL;
}

ssize_t _ecp_rbuf_msg_store(ECPRBuffer *rbuf, ecp_seq_t seq, int idx, unsigned char *msg, size_t msg_size, unsigned char test_flags, unsigned char set_flags) {
    idx = idx < 0 ? _ecp_rbuf_msg_idx(rbuf, seq) : idx;
    if (idx < 0) return idx;

    if (rbuf->msg == NULL) return 0;
    if (test_flags && (test_flags & rbuf->msg[idx].flags)) return ECP_ERR_RBUF_DUP;

    if (msg_size) memcpy(rbuf->msg[idx].msg, msg, msg_size);
    rbuf->msg[idx].size = msg_size;
    rbuf->msg[idx].flags = set_flags;

    return msg_size;
}

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

ssize_t ecp_rbuf_pld_send(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pld_size, unsigned char flags, ecp_seq_t seq) {
    ECPSocket *sock = conn->sock;
    ECPContext *ctx = sock->ctx;
    ECPNetAddr addr;
    ECPSeqItem seq_item;
    ssize_t rv;
    int _rv;

    _rv = ecp_seq_item_init(&seq_item);
    if (_rv) return _rv;

    seq_item.seq = seq;
    seq_item.seq_w = 1;
    seq_item.rb_pass = 1;

    rv = ecp_pack_conn(conn, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, &seq_item, payload, pld_size, &addr);
    if (rv < 0) return rv;

    rv = ecp_pkt_send(sock, &addr, packet, rv, flags);
    if (rv < 0) return rv;

    return rv;
}

int ecp_rbuf_handle_seq(ECPConnection *conn, unsigned char mtype) {
    if (conn->rbuf.recv || (mtype == ECP_MTYPE_RBACK) || (mtype == ECP_MTYPE_RBFLUSH)) return 1;
    return 0;
}

int ecp_rbuf_set_seq(ECPConnection *conn, ECPSeqItem *si, unsigned char *payload, size_t pld_size) {
    ECPRBSend *buf;
    unsigned char mtype;
    int idx;
    int rv;

    if (si->rb_pass) return ECP_OK;

    buf = conn->rbuf.send;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&buf->mutex);
#endif
    idx = _ecp_rbuf_msg_idx(&buf->rbuf, si->seq);
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&buf->mutex);
#endif

    if (idx < 0) return idx;

    rv = ecp_pld_get_type(payload, pld_size, &mtype);
    if (rv) return rv;

    si->rb_mtype = mtype;
    si->rb_idx = idx;
    buf->rbuf.msg[idx].size = 0;
    buf->rbuf.msg[idx].flags = 0;

    return ECP_OK;
}

ssize_t ecp_rbuf_pkt_send(ECPConnection *conn, ECPSocket *sock, ECPNetAddr *addr, ECPBuffer *packet, size_t pkt_size, unsigned char flags, ECPTimerItem *ti, ECPSeqItem *si) {
    ECPRBSend *buf;
    int do_send = 1;
    ssize_t rv = 0;

    buf = conn->rbuf.send;
    if (!si->rb_pass) {
        unsigned char flags = 0;
        ecp_seq_t seq = si->seq;
        unsigned int idx = si->rb_idx;
        unsigned char mtype = si->rb_mtype & ECP_MTYPE_MASK;

        if (mtype < ECP_MAX_MTYPE_SYS) flags |= ECP_RBUF_FLAG_SYS;

        rv = _ecp_rbuf_msg_store(&buf->rbuf, seq, idx, packet->buffer, pkt_size, 0, flags);
        if (rv < 0) return rv;

        if (buf->flags & ECP_RBUF_FLAG_CCONTROL) {
            int _rv = ECP_OK;

#ifdef ECP_WITH_PTHREAD
            pthread_mutex_lock(&buf->mutex);
#endif

            if (ECP_SEQ_LT(buf->rbuf.seq_max, seq)) buf->rbuf.seq_max = seq;

            if (buf->cnt_cc || (buf->in_transit >= buf->win_size)) {
                if (!buf->cnt_cc) buf->seq_cc = seq;
                buf->cnt_cc++;
                buf->rbuf.msg[idx].flags |= ECP_RBUF_FLAG_IN_CCONTROL;
                do_send = 0;
                if (ti) {
                    ECPRBTimer *timer = &buf->timer;
                    ECPRBTimerItem *item = &timer->item[timer->idx_w];

                    if (!item->occupied) {
                        item->occupied = 1;
                        item->item = *ti;
                        buf->rbuf.msg[idx].idx_t = timer->idx_w;
                        timer->idx_w = (timer->idx_w) % ECP_MAX_TIMER;
                    } else {
                        _rv = ECP_ERR_MAX_TIMER;
                    }
                } else {
                    buf->rbuf.msg[idx].idx_t = -1;
                }
            } else {
                buf->in_transit++;
            }

#ifdef ECP_WITH_PTHREAD
            pthread_mutex_unlock(&buf->mutex);
#endif

            if (_rv) return _rv;
        }
    }

    if (do_send) {
        if (ti) {
            int _rv;

            _rv = ecp_timer_push(ti);
            if (_rv) return _rv;
        }
        rv = ecp_pkt_send(sock, addr, packet, pkt_size, flags);
    }
    return rv;
}
