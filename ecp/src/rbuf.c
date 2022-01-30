#include "core.h"

int _ecp_rbuf_start(ECPRBuffer *rbuf, ecp_seq_t seq) {
    rbuf->seq_max = seq;
    rbuf->seq_start = seq + 1;

    return ECP_OK;
}

int _ecp_rbuf_msg_idx(ECPRBuffer *rbuf, ecp_seq_t seq, unsigned short *idx) {
    ecp_seq_t seq_offset = seq - rbuf->seq_start;

    /* This also checks for seq_start <= seq if seq type range >> rbuf->arr_size */
    if (seq_offset >= rbuf->arr_size) return ECP_ERR_FULL;

    *idx = ECP_RBUF_IDX_MASK(rbuf->idx_start + seq_offset, rbuf->arr_size);
    return ECP_OK;
}

int ecp_rbuf_create(ECPConnection *conn, ECPRBSend *buf_s, ECPRBPacket *msg_s, unsigned int msg_s_size, ECPRBRecv *buf_r, ECPRBMessage *msg_r, unsigned int msg_r_size) {
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

int ecp_rbuf_handle_seq(ECPConnection *conn, unsigned char mtype) {
    if (conn->rbuf.recv || (mtype == ECP_MTYPE_RBACK) || (mtype == ECP_MTYPE_RBFLUSH)) return 1;
    return 0;
}

int ecp_rbuf_set_seq(ECPConnection *conn, ECPSeqItem *si, unsigned char *payload, size_t pld_size) {
    ECPRBSend *buf = conn->rbuf.send;
    ECPRBuffer *rbuf = &buf->rbuf;
    unsigned char mtype;
    unsigned short idx;
    int rv;

    if (si->rb_pass) return ECP_OK;

    buf = conn->rbuf.send;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&buf->mutex);
#endif
    rv = _ecp_rbuf_msg_idx(rbuf, si->seq, &idx);
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&buf->mutex);
#endif
    if (rv) return rv;

    rv = ecp_pld_get_type(payload, pld_size, &mtype);
    if (rv) return rv;

    si->rb_mtype = mtype;
    si->rb_idx = idx;
    rbuf->arr.pkt[idx].size = 0;
    rbuf->arr.pkt[idx].flags = 0;

    return ECP_OK;
}

ssize_t ecp_rbuf_pld_send(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pld_size, unsigned char flags, ecp_seq_t seq) {
    ECPSocket *sock = conn->sock;
    ECPContext *ctx = sock->ctx;
    ECPNetAddr addr;
    ECPSeqItem seq_item;
    int _rv;
    ssize_t rv;

    _rv = ecp_seq_item_init(&seq_item);
    if (_rv) return _rv;

    seq_item.seq = seq;
    seq_item.seq_w = 1;
    seq_item.rb_pass = 1;

    rv = ecp_pack_conn(conn, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, payload, pld_size, &addr, &seq_item);
    if (rv < 0) return rv;

    rv = ecp_pkt_send(sock, &addr, packet, rv, flags);
    if (rv < 0) return rv;

    return rv;
}

ssize_t ecp_rbuf_pkt_send(ECPConnection *conn, ECPSocket *sock, ECPNetAddr *addr, ECPBuffer *packet, size_t pkt_size, unsigned char flags, ECPTimerItem *ti, ECPSeqItem *si) {
    ECPRBSend *buf = conn->rbuf.send;
    ECPRBuffer *rbuf = &buf->rbuf;
    ecp_seq_t seq = si->seq;
    unsigned short idx = si->rb_idx;
    unsigned char mtype = si->rb_mtype & ECP_MTYPE_MASK;
    unsigned char _flags;
    int rb_rel;
    int rb_cc;
    int do_send;
    int do_skip;
    int _rv = ECP_OK;
    ssize_t rv = 0;

    if (pkt_size == 0) return ECP_ERR;

    do_send = 1;
    do_skip = ecp_rbuf_skip(mtype);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&buf->mutex);
#endif

    rb_rel = (buf->flags & ECP_RBUF_FLAG_RELIABLE);
    rb_cc = ((buf->flags & ECP_RBUF_FLAG_CCONTROL) && (buf->cnt_cc || (buf->in_transit >= buf->win_size)));

    if (rb_rel || rb_cc) {
        ECPRBTimer *rb_timer = NULL;
        ECPRBTimerItem *rb_ti = NULL;

        _flags = ECP_RBUF_FLAG_IN_RBUF;
        if (do_skip) {
            _flags |= ECP_RBUF_FLAG_SKIP;
        } else {
            do_send = 0;
        }

        if (rbuf->arr.pkt[idx].flags) _rv = ECP_ERR_RBUF_DUP;

        if (!_rv && !do_send && ti) {
            rb_timer = buf->timer;

#ifdef ECP_WITH_PTHREAD
            pthread_mutex_lock(&rb_timer->mutex);
#endif

            rb_ti = &rb_timer->item[rb_timer->idx_w];
            if (rb_ti->empty) {
                rb_ti->empty = 0;
                rb_ti->item = *ti;
                rb_timer->idx_w = (rb_timer->idx_w + 1) % ECP_MAX_TIMER;
            } else {
                _rv = ECP_ERR_MAX_TIMER;
            }

#ifdef ECP_WITH_PTHREAD
            pthread_mutex_unlock(&rb_timer->mutex);
#endif
        }

        if (_rv) {
            rv = _rv;
            goto pkt_send_fin;
        }

        if (!do_send) {
            memcpy(rbuf->arr.pkt[idx].buf, packet->buffer, pkt_size);
            rbuf->arr.pkt[idx].size = pkt_size;
            rbuf->arr.pkt[idx].timer = rb_ti;
            rv = pkt_size;
        }
        rbuf->arr.pkt[idx].flags = _flags;

        if (ECP_SEQ_LT(rbuf->seq_max, seq)) rbuf->seq_max = seq;

        if (rb_cc && !do_send) {
            if (buf->cnt_cc == 0) buf->seq_cc = seq;
            buf->cnt_cc++;
        }
    }

    if ((buf->flags & ECP_RBUF_FLAG_CCONTROL) && !do_skip && do_send) {
        buf->in_transit++;
    }

pkt_send_fin:

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&buf->mutex);
#endif

    if (rv < 0) return rv;

    if (do_send) {
        if (ti) {
            _rv = ecp_timer_push(ti);
            if (_rv) return _rv;
        }
        rv = ecp_pkt_send(sock, addr, packet, pkt_size, flags);
    }
    return rv;
}
