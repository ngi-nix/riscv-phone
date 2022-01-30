#include "core.h"
#include "tr.h"

#define NACK_RATE_UNIT      10000

#define MIN(X, Y)           (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)           (((X) > (Y)) ? (X) : (Y))

static ssize_t flush_send(ECPConnection *conn, ECPTimerItem *ti) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(0, ECP_MTYPE_RBFLUSH, conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(0, ECP_MTYPE_RBFLUSH, conn)];

    packet.buffer = pkt_buf;
    packet.size = ECP_SIZE_PKT_BUF(0, ECP_MTYPE_RBFLUSH, conn);
    payload.buffer = pld_buf;
    payload.size = ECP_SIZE_PLD_BUF(0, ECP_MTYPE_RBFLUSH, conn);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_RBFLUSH);
    if (ti == NULL) {
        ECPTimerItem _ti;
        int rv;

        rv = ecp_timer_item_init(&_ti, conn, ECP_MTYPE_RBACK, 3, 500);
        if (rv) return rv;

        _ti.retry = flush_send;
        rv = ecp_timer_push(&_ti);
        if (rv) return rv;
    }
    return ecp_pld_send(conn, &packet, &payload, ECP_SIZE_PLD(0, ECP_MTYPE_RBFLUSH), 0);
}

static void cc_flush(ECPConnection *conn, unsigned char flags) {
    ECPRBSend *buf = conn->rbuf.send;
    ECPRBuffer *rbuf = &buf->rbuf;
    unsigned short idx;
    int rv;

    rv = _ecp_rbuf_msg_idx(rbuf, buf->seq_cc, &idx);
    if (rv) return;

    while (ECP_SEQ_LTE(buf->seq_cc, rbuf->seq_max)) {
        ECPRBTimerItem *ti;
        ECPBuffer packet;

        if ((buf->cnt_cc == 0) || (buf->in_transit >= buf->win_size)) break;

        if ((rbuf->arr.pkt[idx].flags & ECP_RBUF_FLAG_IN_RBUF) && !(rbuf->arr.pkt[idx].flags & ECP_RBUF_FLAG_SKIP)) {
#ifdef ECP_WITH_PTHREAD
            pthread_mutex_unlock(&buf->mutex);
#endif

            ti = rbuf->arr.pkt[idx].timer;
            if (ti) ecp_timer_push(&ti->item);
            packet.buffer = rbuf->arr.pkt[idx].buf;
            packet.size = ECP_MAX_PKT;
            ecp_pkt_send(conn->sock, &conn->node.addr, &packet, rbuf->arr.pkt[idx].size, flags);

#ifdef ECP_WITH_PTHREAD
            pthread_mutex_lock(&buf->mutex);
#endif

            if (ti) {
#ifdef ECP_WITH_PTHREAD
                pthread_mutex_lock(&buf->timer->mutex);
#endif
                ti->empty = 1;
#ifdef ECP_WITH_PTHREAD
                pthread_mutex_lock(&buf->timer->mutex);
#endif
            }

            buf->cnt_cc--;
            buf->in_transit++;
        }
        if (!(buf->flags & ECP_RBUF_FLAG_RELIABLE)) {
            rbuf->arr.pkt[idx].flags = 0;
            // if (rbuf->arr.pkt[idx].flags == 0);
        }
        buf->seq_cc++;
        idx = ECP_RBUF_IDX_MASK(idx + 1, rbuf->arr_size);
    }
    if (!(buf->flags & ECP_RBUF_FLAG_RELIABLE)) {
        rbuf->seq_start = buf->seq_cc;
        rbuf->idx_start = idx;
    }
}

ssize_t ecp_rbuf_handle_ack(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    ECPRBSend *buf;
    ECPRBuffer *rbuf;
    ssize_t rsize = sizeof(ecp_seq_t)+sizeof(ecp_ack_t);
    ecp_seq_t seq_ack = 0;
    ecp_ack_t ack_map = 0;
    ecp_seq_t seq_start;
    ecp_seq_t seq_max;
    unsigned short idx;
    unsigned short msg_cnt;
    int do_flush = 0;
    int i;
    int rv;

    buf = conn->rbuf.send;
    if (buf == NULL) return size;

    rbuf = &buf->rbuf;
    if (size < 0) return size;
    if (size < rsize) return ECP_ERR;

    seq_ack = \
        (msg[0] << 24) | \
        (msg[1] << 16) | \
        (msg[2] << 8)  | \
        (msg[3]);
    ack_map = \
        (msg[4] << 24) | \
        (msg[5] << 16) | \
        (msg[6] << 8)  | \
        (msg[7]);

    ecp_tr_release(b->packet, 1);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&buf->mutex);
#endif

    seq_max = (buf->cnt_cc ? buf->seq_cc - 1 : rbuf->seq_max);
    if (ECP_SEQ_LT(seq_max, seq_ack)) return ECP_ERR;

    if (buf->flags & ECP_RBUF_FLAG_RELIABLE) {
        rv = _ecp_rbuf_msg_idx(rbuf, seq_ack, &idx);
        if (rv) goto handle_ack_fin;
    }

    if (buf->flags & ECP_RBUF_FLAG_CCONTROL) {
        buf->in_transit = seq_max - seq_ack;
    }

    if (ack_map != ECP_ACK_FULL) {
        ecp_ack_t ack_mask = (ecp_ack_t)1 << (ECP_SIZE_ACKB - 1);
        unsigned short nack_cnt = 0;
        int nack_first = 0;

        seq_ack -= (ECP_SIZE_ACKB - 1);
        if (buf->flags & ECP_RBUF_FLAG_RELIABLE) {
            rv = _ecp_rbuf_msg_idx(rbuf, seq_ack, &idx);
            if (rv) goto handle_ack_fin;
        }

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&buf->mutex);
#endif

        for (i=0; i<ECP_SIZE_ACKB; i++) {
            if ((ack_map & ack_mask) == 0) {
                if (ECP_SEQ_LT(buf->seq_nack, seq_ack)) {
                    nack_cnt++;
                    buf->seq_nack = seq_ack;
                }

                if (buf->flags & ECP_RBUF_FLAG_RELIABLE) {
                    if (!(rbuf->arr.pkt[idx].flags & ECP_RBUF_FLAG_IN_RBUF) || (rbuf->arr.pkt[idx].flags & ECP_RBUF_FLAG_SKIP)) {
                        ECPBuffer packet;
                        ECPBuffer payload;
                        unsigned char pkt_buf[ECP_SIZE_PKT_BUF(0, ECP_MTYPE_NOP, conn)];
                        unsigned char pld_buf[ECP_SIZE_PLD_BUF(0, ECP_MTYPE_NOP, conn)];

                        packet.buffer = pkt_buf;
                        packet.size = ECP_SIZE_PKT_BUF(0, ECP_MTYPE_NOP, conn);
                        payload.buffer = pld_buf;
                        payload.size = ECP_SIZE_PLD_BUF(0, ECP_MTYPE_NOP, conn);

                        ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_NOP);
                        ecp_rbuf_pld_send(conn, &packet, &payload, ECP_SIZE_PLD(0, ECP_MTYPE_NOP), ECP_SEND_FLAG_MORE, seq_ack);
                    } else {
                        ECPBuffer packet;

                        packet.buffer = rbuf->arr.pkt[idx].buf;
                        packet.size = ECP_MAX_PKT;
                        ecp_pkt_send(conn->sock, &conn->node.addr, &packet, rbuf->arr.pkt[idx].size, ECP_SEND_FLAG_MORE);
                        if (buf->flags & ECP_RBUF_FLAG_CCONTROL) {
                            buf->in_transit++;
                        }
                    }
                    if (!nack_first) {
                        nack_first = 1;
                        seq_start = seq_ack;
                    }
                }
            }
            seq_ack++;
            ack_mask = ack_mask >> 1;
            if (buf->flags & ECP_RBUF_FLAG_RELIABLE) idx = ECP_RBUF_IDX_MASK(idx + 1, rbuf->arr_size);
        }

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&buf->mutex);
#endif

        buf->nack_rate = (buf->nack_rate * 7 + ((ECP_SIZE_ACKB - nack_cnt) * NACK_RATE_UNIT) / ECP_SIZE_ACKB) / 8;
    } else {
        buf->nack_rate = (buf->nack_rate * 7) / 8;
        seq_start = seq_ack + 1;
    }

    if (buf->flags & ECP_RBUF_FLAG_RELIABLE) {
        msg_cnt = seq_start - rbuf->seq_start;
        idx = rbuf->idx_start;
        for (i=0; i<msg_cnt; i++) {
            rbuf->arr.pkt[idx].flags = 0;
            // if (rbuf->arr.pkt[idx].flags == 0);
            idx = ECP_RBUF_IDX_MASK(idx + 1, rbuf->arr_size);
        }
        rbuf->seq_start = seq_start;
        rbuf->idx_start = idx;
    }
    if (buf->flush) {
        if (ECP_SEQ_LT(buf->seq_flush, rbuf->seq_start)) buf->flush = 0;
        if (buf->flush) do_flush = 1;
    }
    if (buf->cnt_cc) cc_flush(conn, ECP_SEND_FLAG_MORE);

handle_ack_fin:

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&buf->mutex);
#endif

    if (!rv && do_flush) {
        ssize_t _rv;

        _rv = flush_send(conn, NULL);
        if (_rv < 0) rv = _rv;
    } else {
        // ecp_tr_nomore();
    }

    ecp_tr_release(b->packet, 0);

    if (rv) return rv;
    return rsize;
}

int ecp_rbuf_send_create(ECPConnection *conn, ECPRBSend *buf, ECPRBPacket *pkt, unsigned short pkt_size) {
    ECPRBuffer *rbuf = &buf->rbuf;
    int rv;

    if (pkt == NULL) return ECP_ERR;

    memset(buf, 0, sizeof(ECPRBRecv));
    memset(pkt, 0, sizeof(ECPRBPacket) * pkt_size);

    rbuf->arr.pkt = pkt;
    rbuf->arr_size = pkt_size;

#ifdef ECP_WITH_PTHREAD
    rv = pthread_mutex_init(&buf->mutex, NULL);
    if (rv) return ECP_ERR;
#endif

    conn->rbuf.send = buf;
    return ECP_OK;
}

void ecp_rbuf_send_destroy(ECPConnection *conn) {
    ECPRBSend *buf = conn->rbuf.send;

    if (buf == NULL) return;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_destroy(&buf->mutex);
#endif

    conn->rbuf.send = NULL;
}

int ecp_rbuf_send_start(ECPConnection *conn) {
    ECPRBSend *buf = conn->rbuf.send;
    ECPRBuffer *rbuf = &buf->rbuf;

    if (buf == NULL) return ECP_ERR;

    buf->seq_nack = conn->seq_out;
    return _ecp_rbuf_start(rbuf, conn->seq_out);
}

int ecp_rbuf_set_wsize(ECPConnection *conn, ecp_win_t size) {
    ECPRBSend *buf = conn->rbuf.send;

    if (buf == NULL) return ECP_ERR;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&buf->mutex);
#endif

    buf->win_size = size;
    if (buf->cnt_cc) cc_flush(conn, 0);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&buf->mutex);
#endif

    return ECP_OK;
}

int ecp_rbuf_flush(ECPConnection *conn) {
    ECPRBSend *buf = conn->rbuf.send;
    ecp_seq_t seq;
    ssize_t rv;

    if (buf == NULL) return ECP_ERR;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif
    seq = conn->seq_out;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&buf->mutex);
#endif

    if (buf->flush) {
        if (ECP_SEQ_LT(buf->seq_flush, seq)) buf->seq_flush = seq;
    } else {
        buf->flush = 1;
        buf->seq_flush = seq;
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&buf->mutex);
#endif

    rv = flush_send(conn, NULL);
    if (rv < 0) return rv;

    return ECP_OK;
}
