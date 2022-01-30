#include "core.h"

#ifdef ECP_WITH_MSGQ

#include <sys/time.h>

#define MSG_IDX_MASK(idx)    ((idx) & ((ECP_MSGQ_MAX_MSG) - 1))

static struct timespec *abstime_ts(struct timespec *ts, ecp_cts_t msec) {
    struct timeval tv;
    uint64_t us_start;

    gettimeofday(&tv, NULL);
    us_start = tv.tv_sec * (uint64_t) 1000000 + tv.tv_usec;
    us_start += msec * 1000;
    ts->tv_sec = us_start / 1000000;
    ts->tv_nsec = (us_start % 1000000) * 1000;
    return ts;
}

int ecp_conn_msgq_create(ECPConnMsgQ *msgq) {
    int i;
    int rv;

    memset(msgq, 0, sizeof(ECPConnMsgQ));

    rv = pthread_mutex_init(&msgq->mutex, NULL);
    if (rv) return ECP_ERR;

    for (i=0; i<ECP_MAX_MTYPE; i++) {
        rv = pthread_cond_init(&msgq->cond[i], NULL);
        if (rv) {
            int j;

            for (j=0; j<i; j++) {
                pthread_cond_destroy(&msgq->cond[j]);
            }
            pthread_mutex_destroy(&msgq->mutex);
            return ECP_ERR;
        }
    }

    return ECP_OK;
}

void ecp_conn_msgq_destroy(ECPConnMsgQ *msgq) {
    int i;

    for (i=0; i<ECP_MAX_MTYPE; i++) {
        pthread_cond_destroy(&msgq->cond[i]);
    }
    pthread_mutex_destroy(&msgq->mutex);
}

int ecp_conn_msgq_start(ECPConnMsgQ *msgq, ecp_seq_t seq) {
    msgq->seq_max = seq;
    msgq->seq_start = seq + 1;

    return ECP_OK;
}

int ecp_conn_msgq_push(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype) {
    ECPRBRecv *buf = conn->rbuf.recv;
    ECPConnMsgQ *msgq = &buf->msgq;

    if (mtype >= ECP_MAX_MTYPE) return ECP_ERR_MAX_MTYPE;

    if ((unsigned short)(msgq->idx_w[mtype] - msgq->idx_r[mtype]) == ECP_MSGQ_MAX_MSG) return ECP_ERR_FULL;
    if (msgq->idx_w[mtype] == msgq->idx_r[mtype]) pthread_cond_signal(&msgq->cond[mtype]);

    msgq->seq_msg[mtype][MSG_IDX_MASK(msgq->idx_w[mtype])] = seq;
    msgq->idx_w[mtype]++;

    if (ECP_SEQ_LT(msgq->seq_max, seq)) msgq->seq_max = seq;

    return ECP_OK;
}

ssize_t ecp_conn_msgq_pop(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, ecp_cts_t timeout) {
    ECPRBRecv *buf = conn->rbuf.recv;
    ECPConnMsgQ *msgq = &buf->msgq;
    ECPRBuffer *rbuf = &buf->rbuf;
    ecp_seq_t seq;
    unsigned char *msg_buf;
    unsigned char *content;
    unsigned short idx;
    int _rv;
    ssize_t rv;

    if (mtype >= ECP_MAX_MTYPE) return ECP_ERR_MAX_MTYPE;

    if (msgq->idx_r[mtype] == msgq->idx_w[mtype]) {
        if (timeout == -1) {
            pthread_cond_wait(&msgq->cond[mtype], &msgq->mutex);
        } else {
            struct timespec ts;

            _rv = pthread_cond_timedwait(&msgq->cond[mtype], &msgq->mutex, abstime_ts(&ts, timeout));
            if (_rv) return ECP_ERR_TIMEOUT;
        }
    }
    seq = msgq->seq_msg[mtype][MSG_IDX_MASK(msgq->idx_r[mtype])];

    _rv = _ecp_rbuf_msg_idx(rbuf, seq, &idx);
    if (_rv) return ECP_ERR;

    msg_buf = rbuf->arr.msg[idx].buf;
    rv = rbuf->arr.msg[idx].size;

    content = ecp_msg_get_content(msg_buf, rv);
    if (content == NULL) {
        rv = ECP_ERR;
        goto msgq_pop_fin;
    }

    rv -= content - msg_buf;
    if (msg_size < rv) {
        rv = ECP_ERR_FULL;
        goto msgq_pop_fin;
    }

    memcpy(msg, content, rv);

    rbuf->arr.msg[idx].flags &= ~ECP_RBUF_FLAG_IN_MSGQ;
    // if (rbuf->arr.msg[idx].flags == 0);

msgq_pop_fin:
    msgq->idx_r[mtype]++;
    if (msgq->seq_start == seq) {
        int i;
        unsigned short msg_cnt = msgq->seq_max - msgq->seq_start + 1;

        for (i=0; i<msg_cnt; i++) {
            if (rbuf->arr.msg[idx].flags & ECP_RBUF_FLAG_IN_MSGQ) break;
            idx = ECP_RBUF_IDX_MASK(idx + 1, rbuf->arr_size);
        }
        msgq->seq_start += i;
    }

    return rv;
}

#endif  /* ECP_WITH_MSGQ */