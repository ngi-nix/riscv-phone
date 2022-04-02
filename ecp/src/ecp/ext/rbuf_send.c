#include <stdlib.h>
#include <string.h>

#include <core.h>

#include "rbuf.h"

#define NACK_RATE_UNIT      10000

#define MIN(X, Y)           (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)           (((X) > (Y)) ? (X) : (Y))

static void cc_flush(ECPRBConn *conn, unsigned char flags) {
    ECPConnection *_conn = ecp_rbuf_get_conn(conn);
    ECPRBSend *buf = conn->send;
    ECPRBuffer *rbuf = &buf->rbuf;
    unsigned short idx;
    int rv;

    rv = _ecp_rbuf_msg_idx(rbuf, buf->seq_cc, &idx);
    if (rv) return;

    while (ECP_SEQ_LTE(buf->seq_cc, rbuf->seq_max)) {
        ECPBuffer packet;

        if ((buf->cnt_cc == 0) || (buf->in_transit >= buf->win_size)) break;

        if ((rbuf->arr.pkt[idx].flags & ECP_RBUF_FLAG_IN_RBUF) && !(rbuf->arr.pkt[idx].flags & ECP_RBUF_FLAG_SKIP)) {
#ifdef ECP_WITH_PTHREAD
            pthread_mutex_unlock(&buf->mutex);
#endif

            packet.buffer = rbuf->arr.pkt[idx].buf;
            packet.size = ECP_MAX_PKT;
            ecp_pkt_send(_conn->sock, &packet, rbuf->arr.pkt[idx].size, flags, NULL, &_conn->remote.addr);

#ifdef ECP_WITH_PTHREAD
            pthread_mutex_lock(&buf->mutex);
#endif

            buf->cnt_cc--;
            buf->in_transit++;
        }
        if (!(buf->flags & ECP_RBUF_FLAG_RELIABLE)) {
            rbuf->arr.pkt[idx].flags = 0;
        }
        buf->seq_cc++;
        idx = ECP_RBUF_IDX_MASK(idx + 1, rbuf->arr_size);
    }
    if (!(buf->flags & ECP_RBUF_FLAG_RELIABLE)) {
        rbuf->seq_start = buf->seq_cc;
        rbuf->idx_start = idx;
    }
}

static ssize_t _rbuf_send_flush(ECPConnection *_conn, ECPTimerItem *ti) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(0, ECP_MTYPE_RBFLUSH, _conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(0, ECP_MTYPE_RBFLUSH, _conn)];

    packet.buffer = pkt_buf;
    packet.size = sizeof(pkt_buf);
    payload.buffer = pld_buf;
    payload.size = sizeof(pld_buf);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_RBFLUSH);

    return ecp_pld_send_wtimer(_conn, &packet, &payload, ECP_SIZE_PLD(0, ECP_MTYPE_RBFLUSH), 0, ti);
}

ssize_t ecp_rbuf_send_flush(ECPRBConn *conn) {
    ECPConnection *_conn = ecp_rbuf_get_conn(conn);

    return ecp_timer_send(_conn, _rbuf_send_flush, ECP_MTYPE_RBACK, 3, 500);
}

ssize_t ecp_rbuf_handle_ack(ECPRBConn *conn, unsigned char *msg, size_t msg_size) {
    ECPConnection *_conn = ecp_rbuf_get_conn(conn);
    ECPRBSend *buf = conn->send;
    ECPRBuffer *rbuf = &buf->rbuf;
    ssize_t rsize = sizeof(ecp_seq_t) + sizeof(ecp_ack_t);
    ecp_seq_t seq_start;
    ecp_seq_t seq_max;
    ecp_seq_t seq_ack;
    ecp_ack_t ack_map;
    unsigned short idx;
    unsigned short msg_cnt;
    int do_flush = 0;
    int i;
    int rv;

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

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&buf->mutex);
#endif

    seq_max = (buf->cnt_cc ? buf->seq_cc - 1 : rbuf->seq_max);
    if (ECP_SEQ_LT(seq_max, seq_ack)) return ECP_ERR;

    if (buf->flags & ECP_RBUF_FLAG_RELIABLE) {
        rv = _ecp_rbuf_msg_idx(rbuf, seq_ack, NULL);
        if (rv) goto handle_ack_fin;
    }

    if (buf->flags & ECP_RBUF_FLAG_CCONTROL) {
        buf->in_transit = seq_max - seq_ack;
    }

    if (ack_map != ECP_ACK_FULL) {
        ecp_ack_t ack_mask = (ecp_ack_t)1 << (ECP_SIZE_ACKB - 1);
        ecp_ack_t ack_map_nop = 0;
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
                        ack_map_nop |= ack_mask;
                    } else {
                        ECPBuffer packet;

                        packet.buffer = rbuf->arr.pkt[idx].buf;
                        packet.size = ECP_MAX_PKT;
                        ecp_pkt_send(_conn->sock, &packet, rbuf->arr.pkt[idx].size, ECP_SEND_FLAG_MORE, NULL, &_conn->remote.addr);
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

        if ((buf->flags & ECP_RBUF_FLAG_RELIABLE) && ack_map_nop) {
            ECPBuffer packet;
            ECPBuffer payload;
            unsigned char pkt_buf[ECP_SIZE_PKT_BUF(sizeof(ecp_seq_t) + sizeof(ecp_ack_t), ECP_MTYPE_RBNOP, _conn)];
            unsigned char pld_buf[ECP_SIZE_PLD_BUF(sizeof(ecp_seq_t) + sizeof(ecp_ack_t), ECP_MTYPE_RBNOP, _conn)];
            unsigned char *_buf;

            packet.buffer = pkt_buf;
            packet.size = sizeof(pkt_buf);
            payload.buffer = pld_buf;
            payload.size = sizeof(pld_buf);

            ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_RBNOP);
            _buf = ecp_pld_get_msg(payload.buffer, payload.size);

            seq_ack--;
            _buf[0] = seq_ack >> 24;
            _buf[1] = seq_ack >> 16;
            _buf[2] = seq_ack >> 8;
            _buf[3] = seq_ack;
            _buf[4] = ack_map_nop >> 24;
            _buf[5] = ack_map_nop >> 16;
            _buf[6] = ack_map_nop >> 8;
            _buf[7] = ack_map_nop;

            ecp_pld_send(_conn, &packet, &payload, ECP_SIZE_PLD(sizeof(ecp_seq_t) + sizeof(ecp_ack_t), ECP_MTYPE_RBNOP), ECP_SEND_FLAG_MORE);
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

        _rv = ecp_rbuf_send_flush(conn);
        if (_rv < 0) rv = _rv;
    }

    if (rv) return rv;
    return rsize;
}

int ecp_rbsend_create(ECPRBConn *conn, ECPRBSend *buf, ECPRBPacket *pkt, unsigned short pkt_size) {
    ECPRBuffer *rbuf = &buf->rbuf;
    int rv;

    memset(buf, 0, sizeof(ECPRBRecv));
    memset(pkt, 0, sizeof(ECPRBPacket) * pkt_size);

    rbuf->arr.pkt = pkt;
    rbuf->arr_size = pkt_size;

#ifdef ECP_WITH_PTHREAD
    rv = pthread_mutex_init(&buf->mutex, NULL);
    if (rv) return ECP_ERR;
#endif

    conn->send = buf;
    return ECP_OK;
}

void ecp_rbsend_destroy(ECPRBConn *conn) {
    ECPRBSend *buf = conn->send;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_destroy(&buf->mutex);
#endif

    conn->send = NULL;
}

void ecp_rbsend_start(ECPRBConn *conn, ecp_seq_t seq) {
    ECPRBSend *buf = conn->send;
    ECPRBuffer *rbuf = &buf->rbuf;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&buf->mutex);
#endif

    buf->start = 1;
    buf->seq_nack = seq;
    _ecp_rbuf_start(rbuf, seq);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&buf->mutex);
#endif
}

int ecp_rbuf_set_wsize(ECPRBConn *conn, ecp_win_t size) {
    ECPRBSend *buf = conn->send;

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

int ecp_rbuf_flush(ECPRBConn *conn) {
    ECPConnection *_conn = ecp_rbuf_get_conn(conn);
    ECPRBSend *buf = conn->send;
    ecp_seq_t seq;
    ssize_t rv;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&_conn->mutex);
#endif
    seq = (ecp_seq_t)(_conn->nonce_out) - 1;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&_conn->mutex);
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

    rv = ecp_rbuf_send_flush(conn);
    if (rv < 0) return rv;

    return ECP_OK;
}

ssize_t ecp_rbuf_pld_send(ECPRBConn *conn, ECPBuffer *payload, size_t pld_size, ECPBuffer *packet, size_t pkt_size, ECPTimerItem *ti) {
    ECPRBSend *buf = conn->send;
    ECPRBuffer *rbuf = &buf->rbuf;
    unsigned char mtype;
    int rb_rel;
    int rb_cc;
    int do_send;
    int do_skip;
    int _rv = ECP_OK;
    ssize_t rv = 0;

    _rv = ecp_pld_get_type(payload->buffer, pld_size, &mtype);
    if (_rv) return _rv;

    do_send = 1;
    do_skip = ecp_rbuf_skip(mtype);
    if (ti && !do_skip) return ECP_ERR_RBUF_TIMER;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&buf->mutex);
#endif

    if (!buf->start) {
        rv = 0;
        goto pld_send_fin;
    }

    rb_rel = (buf->flags & ECP_RBUF_FLAG_RELIABLE);
    rb_cc = ((buf->flags & ECP_RBUF_FLAG_CCONTROL) && (buf->cnt_cc || (buf->in_transit >= buf->win_size)));

    if (rb_rel || rb_cc) {
        ecp_seq_t seq;
        unsigned short idx;
        unsigned char _flags;

        _rv = ecp_pkt_get_seq(packet->buffer, pkt_size, &seq);
        if (_rv) {
            rv = _rv;
            goto pld_send_fin;
        }

        _rv = _ecp_rbuf_msg_idx(rbuf, seq, &idx);
        if (_rv) rv = ECP_ERR_RBUF_DUP;
        if (!rv && rbuf->arr.pkt[idx].flags) rv = ECP_ERR_RBUF_DUP;
        if (rv) goto pld_send_fin;

        _flags = ECP_RBUF_FLAG_IN_RBUF;
        if (do_skip) {
            _flags |= ECP_RBUF_FLAG_SKIP;
        } else {
            do_send = 0;
        }

        rbuf->arr.pkt[idx].flags = _flags;
        if (!do_send) {
            memcpy(rbuf->arr.pkt[idx].buf, packet->buffer, pkt_size);
            rbuf->arr.pkt[idx].size = pkt_size;
            rv = pld_size;
        }

        if (ECP_SEQ_LT(rbuf->seq_max, seq)) rbuf->seq_max = seq;

        if (rb_cc && !do_send) {
            if (buf->cnt_cc == 0) buf->seq_cc = seq;
            buf->cnt_cc++;
        }
    }

    if ((buf->flags & ECP_RBUF_FLAG_CCONTROL) && !do_skip && do_send) {
        buf->in_transit++;
    }

pld_send_fin:

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&buf->mutex);
#endif

    return rv;
}
