#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "tm.h"

#include "rbuf.h"

#ifdef ECP_WITH_MSGQ
#include "msgq.h"
#endif

#define ACK_RATE            8
#define ACK_MASK_FIRST      ((ecp_ack_t)1 << (ECP_SIZE_ACKB - 1))

static ssize_t msg_store(ECPRBConn *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *pld, size_t pld_size) {
    ECPConnection *_conn = ecp_rbuf_get_conn(conn);
    ECPRBRecv *buf = conn->recv;
    ECPRBuffer *rbuf = &buf->rbuf;
    unsigned short idx;
    unsigned char flags;
    int skip;
    int rv;

    rv = _ecp_rbuf_msg_idx(rbuf, seq, &idx);
    if (rv) return rv;

    if (rbuf->arr.pld[idx].flags) return ECP_ERR_RBUF_DUP;

#ifdef ECP_WITH_MSGQ
    if (buf->msgq) {
        ecp_seq_t seq_offset;

#ifndef ECP_WITH_RTHD
        pthread_mutex_lock(&buf->mutex);
#endif

        seq_offset = seq - buf->msgq->seq_start;
        if (seq_offset >= rbuf->arr_size) rv = ECP_ERR_FULL;

#ifndef ECP_WITH_RTHD
        pthread_mutex_unlock(&buf->mutex);
#endif

        if (rv) return rv;
    }
#endif

    skip = ecp_rbuf_skip(mtype);
    flags = ECP_RBUF_FLAG_IN_RBUF;
    if (skip) flags |= ECP_RBUF_FLAG_SKIP;
    rbuf->arr.pld[idx].flags = flags;

    if ((mtype == ECP_MTYPE_RBNOP) && pld) {
        return ecp_pld_handle_one(_conn, seq, pld, pld_size, NULL);
    } else if (skip) {
        return 0;
    }

    if (pld && pld_size) memcpy(rbuf->arr.pld[idx].buf, pld, pld_size);
    rbuf->arr.pld[idx].size = pld_size;
    if (ECP_SEQ_LT(rbuf->seq_max, seq)) rbuf->seq_max = seq;

    return pld_size;
}

static void msg_flush(ECPRBConn *conn) {
    ECPConnection *_conn = ecp_rbuf_get_conn(conn);
    ECPRBRecv *buf = conn->recv;
    ECPRBuffer *rbuf = &buf->rbuf;
    ecp_seq_t seq;
    unsigned short idx;
    int i;

#ifdef ECP_WITH_MSGQ
#ifndef ECP_WITH_RTHD
    if (buf->msgq) pthread_mutex_lock(&buf->mutex);
#endif
#endif

    seq = rbuf->seq_start;
    idx = rbuf->idx_start;

    while (ECP_SEQ_LTE(seq, rbuf->seq_max)) {
        if (rbuf->arr.pld[idx].flags & ECP_RBUF_FLAG_IN_RBUF) {
            if (!(rbuf->arr.pld[idx].flags & ECP_RBUF_FLAG_SKIP)) {
                ecp_pts_t msg_pts;
                int rv;

                rv = ecp_pld_get_pts(rbuf->arr.pld[idx].buf, rbuf->arr.pld[idx].size, &msg_pts);
                if (!rv && buf->deliver_delay) {
                    ecp_sts_t now = ecp_tm_abstime_ms(0);

                    msg_pts += buf->deliver_delay;
                    if (ECP_PTS_LT(now, msg_pts)) {
                        if (!(rbuf->arr.pld[idx].flags & ECP_RBUF_FLAG_IN_TIMER)) {
                            ECPTimerItem ti;

                            ecp_timer_item_init(&ti, _conn, ECP_MTYPE_RBTIMER, NULL, 0, msg_pts - now);
                            rv = ecp_timer_push(&ti);
                            if (!rv) rbuf->arr.pld[idx].flags |= ECP_RBUF_FLAG_IN_TIMER;
                        }
                        break;
                    } else if (rbuf->arr.pld[idx].flags & ECP_RBUF_FLAG_IN_TIMER) {
                        rbuf->arr.pld[idx].flags &= ~ECP_RBUF_FLAG_IN_TIMER;
                    }
                }

#ifdef ECP_WITH_MSGQ
                if (buf->msgq) {
                    unsigned char mtype;

                    rv = ecp_pld_get_type(rbuf->arr.pld[idx].buf, rbuf->arr.pld[idx].size, &mtype);
                    if (!rv) rv = ecp_msgq_push(conn, seq, mtype & ECP_MTYPE_MASK);
                    if (rv) break;

                    rbuf->arr.pld[idx].flags |= ECP_RBUF_FLAG_IN_MSGQ;
                } else
#endif
                    ecp_pld_handle(_conn, seq, rbuf->arr.pld[idx].buf, rbuf->arr.pld[idx].size, NULL);
            } else {
                rbuf->arr.pld[idx].flags &= ~ECP_RBUF_FLAG_SKIP;
            }
            rbuf->arr.pld[idx].flags &= ~ECP_RBUF_FLAG_IN_RBUF;
            // if (rbuf->arr.pld[idx].flags == 0);
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
#ifndef ECP_WITH_RTHD
    if (buf->msgq) pthread_mutex_unlock(&buf->mutex);
#endif
#endif
}

static int ack_shift(ECPRBConn *conn) {
    ECPRBRecv *buf = conn->recv;
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
            in_rbuf = rbuf->arr.pld[idx].flags & ECP_RBUF_FLAG_IN_RBUF;
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

static int ack_send(ECPRBConn *conn, ecp_seq_t seq_ack, ecp_seq_t ack_map) {
    ECPConnection *_conn = ecp_rbuf_get_conn(conn);
    ECPRBRecv *buf = conn->recv;
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(sizeof(ecp_seq_t) + sizeof(ecp_ack_t), ECP_MTYPE_RBACK, _conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(sizeof(ecp_seq_t) + sizeof(ecp_ack_t), ECP_MTYPE_RBACK, _conn)];
    unsigned char *_buf;
    ssize_t rv;

    packet.buffer = pkt_buf;
    packet.size = sizeof(pkt_buf);
    payload.buffer = pld_buf;
    payload.size = sizeof(pld_buf);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_RBACK);
    _buf = ecp_pld_get_msg(payload.buffer, payload.size);
    _buf[0] = seq_ack >> 24;
    _buf[1] = seq_ack >> 16;
    _buf[2] = seq_ack >> 8;
    _buf[3] = seq_ack;
    _buf[4] = ack_map >> 24;
    _buf[5] = ack_map >> 16;
    _buf[6] = ack_map >> 8;
    _buf[7] = ack_map;

    rv = ecp_pld_send(_conn, &packet, &payload, ECP_SIZE_PLD(sizeof(ecp_seq_t) + sizeof(ecp_ack_t), ECP_MTYPE_RBACK), 0);
    if (rv < 0) return rv;

    buf->ack_pkt = 0;

    return ECP_OK;
}

ssize_t ecp_rbuf_handle_nop(ECPRBConn *conn, unsigned char *msg, size_t msg_size) {
    ECPRBRecv *buf = conn->recv;
    ECPRBuffer *rbuf = &buf->rbuf;
    ecp_seq_t seq_ack;
    ecp_ack_t ack_map;
    ecp_ack_t ack_mask = (ecp_ack_t)1 << (ECP_SIZE_ACKB - 1);
    size_t rsize = sizeof(ecp_seq_t) + sizeof(ecp_ack_t);
    int i;

    if (msg_size < rsize) return ECP_ERR_SIZE;

    seq_ack = \
        ((ecp_seq_t)msg[0] << 24) | \
        ((ecp_seq_t)msg[1] << 16) | \
        ((ecp_seq_t)msg[2] << 8)  | \
        ((ecp_seq_t)msg[3]);
    ack_map = \
        ((ecp_ack_t)msg[4] << 24) | \
        ((ecp_ack_t)msg[5] << 16) | \
        ((ecp_ack_t)msg[6] << 8)  | \
        ((ecp_ack_t)msg[7]);

    seq_ack -= (ECP_SIZE_ACKB - 1);
    for (i=0; i<ECP_SIZE_ACKB; i++) {
        if (ack_map & ack_mask) {
            msg_store(conn, seq_ack, ECP_MTYPE_RBNOP, NULL, 0);
        }
        seq_ack++;
        ack_mask = ack_mask >> 1;
    }

    return rsize;
}

ssize_t ecp_rbuf_handle_flush(ECPRBConn *conn) {
    ECPRBRecv *buf = conn->recv;

#ifdef ECP_WITH_RTHD
    pthread_mutex_lock(&buf->mutex);
#endif

    ack_send(conn, buf->seq_ack, buf->ack_map);

#ifdef ECP_WITH_RTHD
    pthread_mutex_unlock(&buf->mutex);
#endif

    return 0;
}

void ecp_rbuf_handle_timer(ECPRBConn *conn) {
    ECPRBRecv *buf = conn->recv;

#ifdef ECP_WITH_RTHD
    pthread_mutex_lock(&buf->mutex);
#endif

    msg_flush(conn);

#ifdef ECP_WITH_RTHD
    pthread_mutex_unlock(&buf->mutex);
#endif
}

int ecp_rbrecv_create(ECPRBConn *conn, ECPRBRecv *buf, ECPRBPayload *pld, unsigned short pld_size) {
    ECPRBuffer *rbuf = &buf->rbuf;
    int rv;

    memset(buf, 0, sizeof(ECPRBRecv));
    memset(pld, 0, sizeof(ECPRBPayload) * pld_size);

    buf->ack_map = ECP_ACK_FULL;
    buf->ack_rate = ACK_RATE;
    rbuf->arr.pld = pld;
    rbuf->arr_size = pld_size;

#ifdef ECP_WITH_PTHREAD
    rv = pthread_mutex_init(&buf->mutex, NULL);
    if (rv) return ECP_ERR;
#endif

    conn->recv = buf;
    return ECP_OK;
}

void ecp_rbrecv_destroy(ECPRBConn *conn) {
    ECPRBRecv *buf = conn->recv;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_destroy(&buf->mutex);
#endif

#ifdef ECP_WITH_MSGQ
    if (buf->msgq) ecp_msgq_destroy(conn);
#endif

    conn->recv = NULL;
}

void ecp_rbrecv_start(ECPRBConn *conn, ecp_seq_t seq) {
    ECPRBRecv *buf = conn->recv;
    ECPRBuffer *rbuf = &buf->rbuf;

#ifdef ECP_WITH_RTHD
    pthread_mutex_lock(&buf->mutex);
#endif

    buf->start = 1;
    buf->seq_ack = seq;
    _ecp_rbuf_start(rbuf, seq);

#ifdef ECP_WITH_MSGQ
    if (buf->msgq) ecp_msgq_start(conn, seq);
#endif

#ifdef ECP_WITH_RTHD
    pthread_mutex_unlock(&buf->mutex);
#endif
}

int ecp_rbuf_set_hole(ECPRBConn *conn, unsigned short hole_max) {
    ECPRBRecv *buf = conn->recv;

#ifdef ECP_WITH_RTHD
    pthread_mutex_lock(&buf->mutex);
#endif

    buf->hole_max = hole_max;

#ifdef ECP_WITH_RTHD
    pthread_mutex_unlock(&buf->mutex);
#endif

    return ECP_OK;
}

int ecp_rbuf_set_delay(ECPRBConn *conn, ecp_sts_t delay) {
    ECPRBRecv *buf = conn->recv;

#ifdef ECP_WITH_RTHD
    pthread_mutex_lock(&buf->mutex);
#endif

    buf->deliver_delay = delay;

#ifdef ECP_WITH_RTHD
    pthread_mutex_unlock(&buf->mutex);
#endif

    return ECP_OK;
}

ssize_t ecp_rbuf_store(ECPRBConn *conn, ecp_seq_t seq, unsigned char *pld, size_t pld_size) {
    ECPRBRecv *buf = conn->recv;
    ECPRBuffer *rbuf = &buf->rbuf;
    unsigned char mtype;
    unsigned short ack_pkt = 0;
    int do_ack = 0;
    int _rv;
    ssize_t rv;

    _rv = ecp_pld_get_type(pld, pld_size, &mtype);
    if (_rv) return _rv;

#ifdef ECP_WITH_RTHD
    pthread_mutex_lock(&buf->mutex);
#endif

    if (!buf->start) {
        rv = 0;
        goto rbuf_store_fin;
    }

    if (ECP_SEQ_LT(rbuf->seq_max, seq)) {
        ack_pkt = seq - rbuf->seq_max;
    }
    if (ECP_SEQ_LTE(seq, buf->seq_ack)) {
        ecp_seq_t seq_offset = buf->seq_ack - seq;
        if (seq_offset < ECP_SIZE_ACKB) {
            ecp_ack_t ack_bit = ((ecp_ack_t)1 << seq_offset);

            if (ack_bit & buf->ack_map) {
                rv = ECP_ERR_RBUF_DUP;
                goto rbuf_store_fin;
            }

            rv = msg_store(conn, seq, mtype, pld, pld_size);
            if (rv < 0) goto rbuf_store_fin;

            buf->ack_map |= ack_bit;
            /* reliable transport can prevent seq_ack from reaching seq_max */
            if (ECP_SEQ_LT(buf->seq_ack, rbuf->seq_max)) {
                do_ack = ack_shift(conn);
            }
        } else {
            rv = ECP_ERR_RBUF_DUP;
            goto rbuf_store_fin;
        }
    } else {
        unsigned short msg_cnt = rbuf->seq_max - rbuf->seq_start + 1;

        if ((msg_cnt == 0) && (seq == (ecp_seq_t)(buf->seq_ack + 1))) {
            int deliver = 1;
#ifdef ECP_WITH_MSGQ
            if (buf->msgq) deliver = 0;
#endif
            if (buf->deliver_delay) deliver = 0;
            if (deliver) {
                /* receive buffer is empty, so no need for msgq mutex lock */
                rv = 0;
                rbuf->seq_max++;
                rbuf->seq_start++;
                rbuf->idx_start = ECP_RBUF_IDX_MASK(rbuf->idx_start + 1, rbuf->arr_size);
            } else {
                rv = msg_store(conn, seq, mtype, pld, pld_size);
                if (rv < 0) goto rbuf_store_fin;
            }
            buf->seq_ack++;
        } else {
            rv = msg_store(conn, seq, mtype, pld, pld_size);
            if (rv < 0) goto rbuf_store_fin;

            do_ack = ack_shift(conn);
        }
    }
    msg_flush(conn);
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
        if (_rv) {
            rv = _rv;
            goto rbuf_store_fin;
        }
    }

rbuf_store_fin:

#ifdef ECP_WITH_RTHD
    pthread_mutex_unlock(&buf->mutex);
#endif

    return rv;
}
