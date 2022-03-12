#include <sys/time.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "rbuf.h"
#include "msgq.h"

#define MSGQ_IDX_MASK(idx)    ((idx) & ((ECP_MSGQ_MAX_MSG) - 1))

static struct timespec *abstime_ts(struct timespec *ts, ecp_sts_t msec) {
    struct timeval tv;
    uint64_t us_start;

    gettimeofday(&tv, NULL);
    us_start = tv.tv_sec * (uint64_t) 1000000 + tv.tv_usec;
    us_start += msec * 1000;
    ts->tv_sec = us_start / 1000000;
    ts->tv_nsec = (us_start % 1000000) * 1000;
    return ts;
}

int ecp_msgq_create(ECPRBConn *conn, ECPMsgQ *msgq) {
    int i;
    int rv;

    if (conn->recv == NULL) return ECP_ERR;

    memset(msgq, 0, sizeof(ECPMsgQ));

    for (i=0; i<ECP_MSGQ_MAX_MTYPE; i++) {
        rv = pthread_cond_init(&msgq->cond[i], NULL);
        if (rv) {
            int j;

            for (j=0; j<i; j++) {
                pthread_cond_destroy(&msgq->cond[j]);
            }
            return ECP_ERR;
        }
    }
    conn->recv->msgq = msgq;

    return ECP_OK;
}

void ecp_msgq_destroy(ECPRBConn *conn) {
    ECPMsgQ *msgq = conn->recv->msgq;
    int i;

    for (i=0; i<ECP_MSGQ_MAX_MTYPE; i++) {
        pthread_cond_destroy(&msgq->cond[i]);
    }

    conn->recv->msgq = NULL;
}

void ecp_msgq_start(ECPRBConn *conn, ecp_seq_t seq) {
    ECPMsgQ *msgq = conn->recv->msgq;

    msgq->seq_max = seq;
    msgq->seq_start = seq + 1;
}

int ecp_msgq_push(ECPRBConn *conn, ecp_seq_t seq, unsigned char mtype) {
    ECPRBRecv *buf = conn->recv;
    ECPMsgQ *msgq = buf->msgq;

    if (mtype >= ECP_MSGQ_MAX_MTYPE) return ECP_ERR_MTYPE;

    if ((unsigned short)(msgq->idx_w[mtype] - msgq->idx_r[mtype]) == ECP_MSGQ_MAX_MSG) return ECP_ERR_FULL;
    if (msgq->idx_w[mtype] == msgq->idx_r[mtype]) pthread_cond_signal(&msgq->cond[mtype]);

    msgq->seq_msg[mtype][MSGQ_IDX_MASK(msgq->idx_w[mtype])] = seq;
    msgq->idx_w[mtype]++;

    if (ECP_SEQ_LT(msgq->seq_max, seq)) msgq->seq_max = seq;

    return ECP_OK;
}

ssize_t ecp_msgq_pop(ECPRBConn *conn, unsigned char mtype, unsigned char *msg, size_t _msg_size, ecp_sts_t timeout) {
    ECPRBRecv *buf = conn->recv;
    ECPMsgQ *msgq = buf->msgq;
    ECPRBuffer *rbuf = &buf->rbuf;
    unsigned char *pld_buf;
    unsigned char *msg_buf;
    size_t pld_size, hdr_size, msg_size;
    ecp_seq_t seq;
    unsigned short idx;
    int rv;

    if (mtype >= ECP_MSGQ_MAX_MTYPE) return ECP_ERR_MTYPE;

    if (msgq->idx_r[mtype] == msgq->idx_w[mtype]) {
        if (timeout == -1) {
            pthread_cond_wait(&msgq->cond[mtype], &buf->mutex);
        } else {
            struct timespec ts;

            rv = pthread_cond_timedwait(&msgq->cond[mtype], &buf->mutex, abstime_ts(&ts, timeout));
            if (rv) return ECP_ERR_TIMEOUT;
        }
    }
    seq = msgq->seq_msg[mtype][MSGQ_IDX_MASK(msgq->idx_r[mtype])];

    rv = _ecp_rbuf_msg_idx(rbuf, seq, &idx);
    if (rv) return ECP_ERR;

    pld_buf = rbuf->arr.pld[idx].buf;
    pld_size = rbuf->arr.pld[idx].size;

    msg_buf = ecp_pld_get_msg(pld_buf, pld_size);
    if (msg_buf == NULL) return ECP_ERR;
    hdr_size = msg_buf - pld_buf;
    msg_size = pld_size - hdr_size;

    rbuf->arr.pld[idx].flags &= ~ECP_RBUF_FLAG_IN_MSGQ;
    // if (rbuf->arr.pld[idx].flags == 0);

    msgq->idx_r[mtype]++;
    if (msgq->seq_start == seq) {
        int i;
        unsigned short msg_cnt = msgq->seq_max - msgq->seq_start + 1;

        for (i=0; i<msg_cnt; i++) {
            if (rbuf->arr.pld[idx].flags & ECP_RBUF_FLAG_IN_MSGQ) break;
            idx = ECP_RBUF_IDX_MASK(idx + 1, rbuf->arr_size);
        }
        msgq->seq_start += i;
    }

    if (_msg_size < msg_size) return ECP_ERR_FULL;
    if (msg_size) memcpy(msg, msg_buf, msg_size);
    return msg_size;
}

ssize_t ecp_msg_receive(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, ecp_sts_t timeout) {
    ECPRBConn *_conn;
    ssize_t rv;

    _conn = ecp_rbuf_get_rbconn(conn);
    if (_conn == NULL) return ECP_ERR;

    pthread_mutex_lock(&_conn->recv->mutex);
    rv = ecp_msgq_pop(_conn, mtype, msg, msg_size, timeout);
    pthread_mutex_unlock(&_conn->recv->mutex);

    return rv;
}
