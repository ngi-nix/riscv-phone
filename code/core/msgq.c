#include "core.h"

#ifdef ECP_WITH_PTHREAD

#include <sys/time.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define MSG_IDX_MASK(idx)    ((idx) & ((ECP_MAX_CONN_MSG) - 1))

static struct timespec *abstime_ts(struct timespec *ts, unsigned int msec) {
    struct timeval tv;
    uint64_t us_start;
    
    gettimeofday(&tv, NULL);
    us_start = tv.tv_sec * (uint64_t) 1000000 + tv.tv_usec;
    us_start += msec * 1000;
    ts->tv_sec = us_start / 1000000;
    ts->tv_nsec = (us_start % 1000000) * 1000;
    return ts;
}

int ecp_conn_msgq_create(ECPConnection *conn) {
    ECPRBRecv *buf = conn->rbuf.recv;
    ECPConnMsgQ *msgq = buf ? &buf->msgq : NULL;
    int i;
    int rv;

    if (msgq == NULL) return ECP_ERR;
    
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

void ecp_conn_msgq_destroy(ECPConnection *conn) {
    ECPRBRecv *buf = conn->rbuf.recv;
    ECPConnMsgQ *msgq = buf ? &buf->msgq : NULL;
    int i;
    
    if (msgq == NULL) return;

    for (i=0; i<ECP_MAX_MTYPE; i++) {
        pthread_cond_destroy(&msgq->cond[i]);
    }
    pthread_mutex_destroy(&msgq->mutex);
}

int ecp_conn_msgq_push(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype) {
    ECPRBRecv *buf = conn->rbuf.recv;
    ECPConnMsgQ *msgq = buf ? &buf->msgq : NULL;

    if (msgq == NULL) return ECP_ERR;
    
    if (msgq->idx_w[mtype] - msgq->idx_r[mtype] == ECP_MAX_CONN_MSG) return ECP_ERR_MAX_CONN_MSG;
    if (msgq->idx_w[mtype] == msgq->idx_r[mtype]) pthread_cond_signal(&msgq->cond[mtype]);
    
    msgq->seq_msg[mtype][MSG_IDX_MASK(msgq->idx_w[mtype])] = seq;
    msgq->idx_w[mtype]++;
    
    return ECP_OK;
}

ssize_t ecp_conn_msgq_pop(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, unsigned int timeout) {
    ECPRBRecv *buf = conn->rbuf.recv;
    ECPConnMsgQ *msgq = buf ? &buf->msgq : NULL;
    ssize_t rv = ECP_OK;
    ecp_seq_t seq;
    int idx;

    if (msgq == NULL) return ECP_ERR;
    if (mtype >= ECP_MAX_MTYPE) return ECP_ERR_MAX_MTYPE;

    if (msgq->idx_r[mtype] == msgq->idx_w[mtype]) {
        if (timeout == -1) {
            pthread_cond_wait(&msgq->cond[mtype], &msgq->mutex);
        } else {
            struct timespec ts;
            int _rv = pthread_cond_timedwait(&msgq->cond[mtype], &msgq->mutex, abstime_ts(&ts, timeout));
            if (_rv) rv = ECP_ERR_TIMEOUT;
        }
    }
    if (!rv) {
        seq = msgq->seq_msg[mtype][MSG_IDX_MASK(msgq->idx_r[mtype])];
        msgq->idx_r[mtype]++;
        idx = ecp_rbuf_msg_idx(&buf->rbuf, seq);
        if (idx < 0) rv = idx;
    }
    if (!rv) {
        buf->rbuf.msg[idx].flags &= ~ECP_RBUF_FLAG_RECEIVED;
        if (buf->rbuf.seq_start == seq) {
            int i, _idx = idx;
            ecp_seq_t msg_cnt = buf->rbuf.seq_max - buf->rbuf.seq_start + 1;

            for (i=0; i<msg_cnt; i++) {
                if (buf->rbuf.msg[_idx].flags & ECP_RBUF_FLAG_RECEIVED) break;
                _idx = ECP_RBUF_IDX_MASK(_idx + 1, buf->rbuf.msg_size);
            }
            buf->rbuf.seq_start += i;
            buf->rbuf.msg_start = _idx;
        }
        rv = buf->rbuf.msg[idx].size;
        if (rv >= 0) {
            rv = MIN(msg_size, rv);
            memcpy(msg, buf->rbuf.msg[idx].msg, rv);
        }
    }

    return rv;
}

#endif  /* ECP_WITH_PTHREAD */