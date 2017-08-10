#include "core.h"

#include <string.h>

#define NACK_RATE_UNIT   10000

static ssize_t _rbuf_send_flush(ECPConnection *conn, ECPTimerItem *ti) {
    unsigned char payload[ECP_SIZE_PLD(0)];

    ecp_pld_set_type(payload, ECP_MTYPE_RBFLUSH_REQ);
    return ecp_pld_send(conn, payload, sizeof(payload));
}

static ssize_t handle_ack(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size) {
    ECPRBSend *buf;
    ssize_t rsize = sizeof(ecp_seq_t)+sizeof(ecp_ack_t);
    ecp_seq_t seq_ack = 0;
    ecp_ack_t ack_map = 0;
    int do_flush = 0;
    int rv = ECP_OK;
    
    buf = conn->rbuf.send;
    if (buf == NULL) return ECP_ERR;
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

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&buf->mutex);
#endif

    ECPRBuffer *rbuf = &buf->rbuf;
    int idx = ecp_rbuf_msg_idx(rbuf, seq_ack);
    if (idx < 0) rv = idx;
    
    int is_reliable = buf->flags & ECP_RBUF_FLAG_RELIABLE;

    if (!rv) {
        seq_ack++;
        buf->in_transit -= seq_ack - rbuf->seq_start;

        if (ack_map != ECP_RBUF_ACK_FULL) {
            int i;
            int nack_first = 0;
            unsigned int msg_start;
            ecp_seq_t seq_start;
            ecp_win_t nack_cnt = 0;
            
            seq_ack -= ECP_RBUF_ACK_SIZE;

#ifdef ECP_WITH_PTHREAD
            pthread_mutex_unlock(&buf->mutex);
#endif
            
            for (i=0; i<ECP_RBUF_ACK_SIZE; i++) {
                if ((ack_map & ((ecp_ack_t)1 << (ECP_RBUF_ACK_SIZE - i - 1))) == 0) {
                    nack_cnt++;
                    if (buf->flags & ECP_RBUF_FLAG_RELIABLE) {
                        idx = ECP_RBUF_IDX_MASK(idx + 1 - ECP_RBUF_ACK_SIZE + i, rbuf->msg_size);
                        ecp_pkt_send(conn->sock, &conn->node.addr, rbuf->msg[idx].msg, rbuf->msg[idx].size);
                        if (!nack_first) {
                            nack_first = 1;
                            seq_start = seq_ack + i;
                            msg_start = idx;
                        }
                    }
                }
            }

#ifdef ECP_WITH_PTHREAD
            pthread_mutex_lock(&buf->mutex);
#endif

            buf->in_transit += nack_cnt;
            buf->nack_rate = (buf->nack_rate * 7 + ((ECP_RBUF_ACK_SIZE - nack_cnt) * NACK_RATE_UNIT) / ECP_RBUF_ACK_SIZE) / 8;
            if (buf->flags & ECP_RBUF_FLAG_RELIABLE) {
                rbuf->seq_start = seq_start;
                rbuf->msg_start = msg_start;
            } else {
                rbuf->seq_start = seq_ack + ECP_RBUF_ACK_SIZE;
            }
        } else {
            rbuf->seq_start = seq_ack;
            buf->nack_rate = (buf->nack_rate * 7) / 8;
            if (buf->flags & ECP_RBUF_FLAG_RELIABLE) {
                rbuf->msg_start = ECP_RBUF_IDX_MASK(idx + 1, rbuf->msg_size);
            }
        }
        if (buf->flush) {
            if (ECP_RBUF_SEQ_LT(buf->seq_flush, rbuf->seq_start)) buf->flush = 0;
            if (buf->flush) {
                do_flush = 1;
            }
        }
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&buf->mutex);
#endif

    if (rv) return rv;

    if (do_flush) ecp_timer_send(conn, _rbuf_send_flush, ECP_MTYPE_RBACK, 3, 500);
    return rsize;
}

int ecp_conn_rbuf_send_create(ECPConnection *conn, ECPRBSend *buf, ECPRBMessage *msg, unsigned int msg_size) {
    int rv;
    
    memset(buf, 0, sizeof(ECPRBRecv));
    rv = ecp_rbuf_init(&buf->rbuf, msg, msg_size);
    if (rv) return rv;

    conn->rbuf.send = buf;

    return ECP_OK;
}

int ecp_conn_rbuf_send_start(ECPConnection *conn) {
    ECPRBSend *buf = conn->rbuf.send;

    if (buf == NULL) return ECP_ERR;
    buf->rbuf.seq_start = conn->seq_out;

    return ECP_OK;
}

int ecp_conn_rbuf_flush(ECPConnection *conn) {
    ECPRBSend *buf = conn->rbuf.send;
    unsigned char payload[ECP_SIZE_PLD(0)];
    ecp_seq_t seq;

    if (buf == NULL) return ECP_ERR;

    // XXX flush seq
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&buf->mutex);
#endif
    if (buf->flush) {
        if (ECP_RBUF_SEQ_LT(buf->seq_flush, seq)) buf->seq_flush = seq;
    } else {
        buf->flush = 1;
        buf->seq_flush = seq;
    }
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&buf->mutex);
#endif

    ssize_t _rv = ecp_timer_send(conn, _rbuf_send_flush, ECP_MTYPE_RBACK, 3, 500);
    if (_rv < 0) return _rv;

    return ECP_OK;
}

