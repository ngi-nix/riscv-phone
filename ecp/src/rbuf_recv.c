#include "core.h"
#include "tr.h"
#include "tm.h"

#define ACK_RATE            8
#define ACK_MASK_FIRST      ((ecp_ack_t)1 << (ECP_SIZE_ACKB - 1))

static ssize_t msg_store(ECPConnection *conn, ecp_seq_t seq, unsigned char *msg, size_t msg_size, unsigned char mtype) {
    ECPRBRecv *buf = conn->rbuf.recv;
    ECPRBuffer *rbuf = &buf->rbuf;
    unsigned short idx;
    unsigned char flags;
    int skip;
    int rv;

    rv = _ecp_rbuf_msg_idx(rbuf, seq, &idx);
    if (rv) return rv;

    if (rbuf->arr.msg[idx].flags) return ECP_ERR_RBUF_DUP;

#ifdef ECP_WITH_MSGQ
    if (buf->flags & ECP_RBUF_FLAG_MSGQ) {
        ecp_seq_t seq_offset;

        pthread_mutex_lock(&buf->msgq.mutex);

        seq_offset = seq - buf->msgq.seq_start;
        if (seq_offset >= rbuf->arr_size) rv = ECP_ERR_FULL;

        pthread_mutex_unlock(&buf->msgq.mutex);

        if (rv) return rv;
    }
#endif

    skip = ecp_rbuf_skip(mtype);
    flags = ECP_RBUF_FLAG_IN_RBUF;
    if (skip) flags |= ECP_RBUF_FLAG_SKIP;
    rbuf->arr.msg[idx].flags = flags;

    if (skip) return 0;

    memcpy(rbuf->arr.msg[idx].buf, msg, msg_size);
    rbuf->arr.msg[idx].size = msg_size;
    if (ECP_SEQ_LT(rbuf->seq_max, seq)) rbuf->seq_max = seq;

    return msg_size;
}

static void msg_flush(ECPConnection *conn, ECP2Buffer *b) {
    ECPRBRecv *buf = conn->rbuf.recv;
    ECPRBuffer *rbuf = &buf->rbuf;
    ecp_seq_t seq;
    unsigned short idx;
    int i;

#ifdef ECP_WITH_MSGQ
    if (buf->flags & ECP_RBUF_FLAG_MSGQ) pthread_mutex_lock(&buf->msgq.mutex);
#endif

    seq = rbuf->seq_start;
    idx = rbuf->idx_start;

    unsigned short msg_cnt = rbuf->seq_max - rbuf->seq_start + 1;

    while (ECP_SEQ_LTE(seq, rbuf->seq_max)) {
        if (rbuf->arr.msg[idx].flags & ECP_RBUF_FLAG_IN_RBUF) {
            if (!(rbuf->arr.msg[idx].flags & ECP_RBUF_FLAG_SKIP)) {
                ecp_pts_t msg_pts;
                int rv;

                rv = ecp_msg_get_pts(rbuf->arr.msg[idx].buf, rbuf->arr.msg[idx].size, &msg_pts);
                if (!rv && buf->deliver_delay) {
                    ecp_cts_t now = ecp_tm_abstime_ms(0);

                    msg_pts += buf->deliver_delay;
                    if (ECP_PTS_LT(now, msg_pts)) {
                        if (!(rbuf->arr.msg[idx].flags & ECP_RBUF_FLAG_IN_TIMER)) {
                            ECPTimerItem ti;

                            rv = ecp_timer_item_init(&ti, conn, ECP_MTYPE_RBTIMER, 0, msg_pts - now);
                            if (!rv) rv = ecp_timer_push(&ti);
                            if (!rv) rbuf->arr.msg[idx].flags |= ECP_RBUF_FLAG_IN_TIMER;
                        }
                        break;
                    } else if (rbuf->arr.msg[idx].flags & ECP_RBUF_FLAG_IN_TIMER) {
                        rbuf->arr.msg[idx].flags &= ~ECP_RBUF_FLAG_IN_TIMER;
                    }
                }

#ifdef ECP_WITH_MSGQ
                if (buf->flags & ECP_RBUF_FLAG_MSGQ) {
                    unsigned char mtype;

                    rv = ecp_msg_get_type(rbuf->arr.msg[idx].buf, rbuf->arr.msg[idx].size, &mtype);
                    if (!rv) rv = ecp_conn_msgq_push(conn, seq, mtype & ECP_MTYPE_MASK);
                    if (rv) break;

                    rbuf->arr.msg[idx].flags |= ECP_RBUF_FLAG_IN_MSGQ;
                } else
#endif
                    ecp_conn_handle_msg(conn, seq, rbuf->arr.msg[idx].buf, rbuf->arr.msg[idx].size, b);
            } else {
                rbuf->arr.msg[idx].flags &= ~ECP_RBUF_FLAG_SKIP;
            }
            rbuf->arr.msg[idx].flags &= ~ECP_RBUF_FLAG_IN_RBUF;
            // if (rbuf->arr.msg[idx].flags == 0);
        } else {
            if (buf->flags & ECP_RBUF_FLAG_RELIABLE) break;
            if (!ECP_SEQ_LT(seq, rbuf->seq_max - buf->hole_max)) break;
        }
        seq++;
        idx = ECP_RBUF_IDX_MASK(idx + 1, rbuf->arr_size);
    }
    rbuf->seq_start = seq;
    rbuf->idx_start = idx;

#ifdef ECP_WITH_MSGQ
    if (buf->flags & ECP_RBUF_FLAG_MSGQ) pthread_mutex_unlock(&buf->msgq.mutex);
#endif

}

static int ack_send(ECPConnection *conn, ecp_seq_t seq_ack, ecp_seq_t ack_map) {
    ECPRBRecv *buf = conn->rbuf.recv;
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(sizeof(ecp_seq_t) + sizeof(ecp_ack_t), ECP_MTYPE_RBACK, conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(sizeof(ecp_seq_t) + sizeof(ecp_ack_t), ECP_MTYPE_RBACK, conn)];
    unsigned char *_buf;
    ssize_t rv;

    packet.buffer = pkt_buf;
    packet.size = ECP_SIZE_PKT_BUF(sizeof(ecp_seq_t) + sizeof(ecp_ack_t), ECP_MTYPE_RBACK, conn);
    payload.buffer = pld_buf;
    payload.size = ECP_SIZE_PLD_BUF(sizeof(ecp_seq_t) + sizeof(ecp_ack_t), ECP_MTYPE_RBACK, conn);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_RBACK);
    _buf = ecp_pld_get_buf(payload.buffer, payload.size);
    _buf[0] = (seq_ack & 0xFF000000) >> 24;
    _buf[1] = (seq_ack & 0x00FF0000) >> 16;
    _buf[2] = (seq_ack & 0x0000FF00) >> 8;
    _buf[3] = (seq_ack & 0x000000FF);
    _buf[4] = (ack_map & 0xFF000000) >> 24;
    _buf[5] = (ack_map & 0x00FF0000) >> 16;
    _buf[6] = (ack_map & 0x0000FF00) >> 8;
    _buf[7] = (ack_map & 0x000000FF);

    rv = ecp_pld_send(conn, &packet, &payload, ECP_SIZE_PLD(sizeof(ecp_seq_t) + sizeof(ecp_ack_t), ECP_MTYPE_RBACK), 0);
    if (rv < 0) return rv;

    buf->ack_pkt = 0;

    return ECP_OK;
}

static int ack_shift(ECPRBRecv *buf) {
    ECPRBuffer *rbuf = &buf->rbuf;
    unsigned short idx;
    int do_ack = 0;
    int in_rbuf = 0;
    int rv;

    if ((buf->flags & ECP_RBUF_FLAG_RELIABLE) && ((buf->ack_map & ACK_MASK_FIRST) == 0)) return 0;

    /* walks through messages that are not delivered yet, so no need for msgq mutex lock */
    while (ECP_SEQ_LT(buf->seq_ack, rbuf->seq_max)) {
        buf->seq_ack++;
        rv = _ecp_rbuf_msg_idx(rbuf, buf->seq_ack, &idx);
        if (!rv) {
            in_rbuf = rbuf->arr.msg[idx].flags & ECP_RBUF_FLAG_IN_RBUF;
        } else {
            in_rbuf = 1;
        }
        if (in_rbuf && (buf->ack_map == ECP_ACK_FULL)) continue;

        buf->ack_map = buf->ack_map << 1;
        if (in_rbuf) {
            buf->ack_map |= 1;
        } else if (!do_ack && ECP_SEQ_LT(buf->seq_ack, rbuf->seq_max - buf->hole_max)) {
            do_ack = 1;
        }

        if ((buf->flags & ECP_RBUF_FLAG_RELIABLE) && ((buf->ack_map & ACK_MASK_FIRST) == 0)) {
            do_ack = 1;
            break;
        }
    }

    return do_ack;
}

ssize_t ecp_rbuf_handle_flush(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    ECPRBRecv *buf = conn->rbuf.recv;

    if (buf == NULL) return ECP_ERR;
    if (size < 0) return size;

    ecp_tr_release(b->packet, 1);
    ack_send(conn, buf->seq_ack, buf->ack_map);
    return 0;
}

ssize_t ecp_rbuf_handle_timer(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    ECPRBRecv *buf = conn->rbuf.recv;

    if (buf == NULL) return ECP_ERR;

    msg_flush(conn, b);
    return 0;
}

int ecp_rbuf_recv_create(ECPConnection *conn, ECPRBRecv *buf, ECPRBMessage *msg, unsigned short msg_size) {
    ECPRBuffer *rbuf = &buf->rbuf;
    int rv;

    if (msg == NULL) return ECP_ERR;

    memset(buf, 0, sizeof(ECPRBRecv));
    memset(msg, 0, sizeof(ECPRBMessage) * msg_size);

    buf->ack_map = ECP_ACK_FULL;
    buf->ack_rate = ACK_RATE;
    rbuf->arr.msg = msg;
    rbuf->arr_size = msg_size;

#ifdef ECP_WITH_MSGQ
    rv = ecp_conn_msgq_create(&buf->msgq);
    if (rv) return rv;
#endif

    conn->rbuf.recv = buf;
    return ECP_OK;
}

void ecp_rbuf_recv_destroy(ECPConnection *conn) {
    ECPRBRecv *buf = conn->rbuf.recv;

    if (buf == NULL) return;

#ifdef ECP_WITH_MSGQ
    ecp_conn_msgq_destroy(&buf->msgq);
#endif
    conn->rbuf.recv = NULL;
}

int ecp_rbuf_recv_start(ECPConnection *conn, ecp_seq_t seq) {
    ECPRBRecv *buf = conn->rbuf.recv;
    ECPRBuffer *rbuf = &buf->rbuf;
    int rv;

    if (buf == NULL) return ECP_ERR;

    seq--;
    buf->seq_ack = seq;
    rv = _ecp_rbuf_start(rbuf, seq);
    if (rv) return rv;

#ifdef ECP_WITH_MSGQ
    if (buf->flags & ECP_RBUF_FLAG_MSGQ) {
        rv = ecp_conn_msgq_start(&buf->msgq, seq);
        if (rv) return rv;
    }
#endif

    return ECP_OK;
}

int ecp_rbuf_set_hole(ECPConnection *conn, unsigned short hole_max) {
    ECPRBRecv *buf = conn->rbuf.recv;

    buf->hole_max = hole_max;

    return ECP_OK;
}

int ecp_rbuf_set_delay(ECPConnection *conn, ecp_cts_t delay) {
    ECPRBRecv *buf = conn->rbuf.recv;

    buf->deliver_delay = delay;

    return ECP_OK;
}

ssize_t ecp_rbuf_store(ECPConnection *conn, ecp_seq_t seq, unsigned char *msg, size_t msg_size, ECP2Buffer *b) {
    ECPRBRecv *buf = conn->rbuf.recv;
    ECPRBuffer *rbuf = &buf->rbuf;
    unsigned char mtype;
    unsigned short ack_pkt = 0;
    int do_ack = 0;
    int _rv;
    ssize_t rv;

    _rv = ecp_msg_get_type(msg, msg_size, &mtype);
    if (_rv) return _rv;

    if (ECP_SEQ_LT(rbuf->seq_max, seq)) {
        ack_pkt = seq - rbuf->seq_max;
    }
    if (ECP_SEQ_LTE(seq, buf->seq_ack)) {
        ecp_seq_t seq_offset = buf->seq_ack - seq;
        if (seq_offset < ECP_SIZE_ACKB) {
            ecp_ack_t ack_bit = ((ecp_ack_t)1 << seq_offset);

            if (ack_bit & buf->ack_map) return ECP_ERR_RBUF_DUP;

            rv = msg_store(conn, seq, msg, msg_size, mtype);
            if (rv < 0) return rv;

            buf->ack_map |= ack_bit;
            do_ack = ack_shift(buf);
        } else {
            return ECP_ERR_RBUF_DUP;
        }
    } else {
        unsigned short msg_cnt = rbuf->seq_max - rbuf->seq_start + 1;

        if ((msg_cnt == 0) && (seq == (ecp_seq_t)(buf->seq_ack + 1))) {
            if ((buf->flags & ECP_RBUF_FLAG_MSGQ) || buf->deliver_delay) {
                rv = msg_store(conn, seq, msg, msg_size, mtype);
                if (rv < 0) return rv;
            } else {
                /* receive buffer is empty, so no need for msgq mutex lock */
                rv = 0;
                rbuf->seq_max++;
                rbuf->seq_start++;
                rbuf->idx_start = ECP_RBUF_IDX_MASK(rbuf->idx_start + 1, rbuf->arr_size);
            }
            buf->seq_ack++;
        } else {
            rv = msg_store(conn, seq, msg, msg_size, mtype);
            if (rv < 0) return rv;

            do_ack = ack_shift(buf);
        }
    }
    msg_flush(conn, b);
    if (ack_pkt) {
        buf->ack_pkt += ack_pkt;
        if (!do_ack && (buf->ack_pkt > buf->ack_rate)) do_ack = 1;
    }
    if (do_ack) {
        ecp_seq_t seq_ack = buf->seq_ack;
        ecp_seq_t ack_map = buf->ack_map;

        /* account for missing mackets within hole_max range */
        if (buf->hole_max && (buf->seq_ack == rbuf->seq_max)) {
            unsigned short h_bits = buf->hole_max + 1;
            ecp_seq_t h_mask = ~(~((ecp_seq_t)0) << h_bits);

            if ((ack_map & h_mask) != h_mask) {
                h_mask = ~(~((ecp_seq_t)0) >> h_bits);
                seq_ack -= h_bits;
                ack_map = (ack_map >> h_bits) | h_mask;
            }
        }
        _rv = ack_send(conn, seq_ack, ack_map);
        if (_rv) return _rv;
    }

    return rv;
}

ECPFragIter *ecp_rbuf_get_frag_iter(ECPConnection *conn) {
    if (conn->rbuf.recv) return conn->rbuf.recv->frag_iter;
    return NULL;
}
