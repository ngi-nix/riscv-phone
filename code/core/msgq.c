#include "core.h"

#ifdef ECP_WITH_PTHREAD

#include <sys/time.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define MSG_NEXT(msgi, max_msgs)    ((msgi + 1) % max_msgs)

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
    int i;
    
    for (i=0; i<ECP_MAX_MTYPE; i++) {
        int rv = pthread_cond_init(&conn->msgq.cond[i], NULL);
        if (rv) {
            int j;

            for (j=0; j<i; j++) {
                pthread_cond_destroy(&conn->msgq.cond[j]);
            }
            return ECP_ERR;
        }
    }

    return ECP_OK;
}

void ecp_conn_msgq_destroy(ECPConnection *conn) {
    int i;
    
    for (i=0; i<ECP_MAX_MTYPE; i++) {
        pthread_cond_destroy(&conn->msgq.cond[i]);
    }
}

ssize_t ecp_conn_msgq_push(ECPConnection *conn, unsigned char *msg, size_t msg_size) {
    ECPConnMsgQ *msgq = &conn->msgq;
    unsigned short msg_idx = msgq->empty_idx;
    unsigned short i;
    unsigned short done = 0;
    unsigned char mtype;
    
    if (msg_size == 0) return ECP_OK;

    mtype = *msg;
    msg++;
    msg_size--;

    if (mtype >= ECP_MAX_MTYPE) return ECP_ERR_MAX_MTYPE;
    if (msg_size >= ECP_MAX_MSG) return ECP_ERR_MAX_MSG;
    if (msg_size < ECP_MIN_MSG) return ECP_ERR_MIN_MSG;
    
    for (i=0; i<ECP_MAX_CONN_MSG; i++) {
        if (!msgq->occupied[msg_idx]) {
            ECPMessage *message = &msgq->msg[msg_idx];
            if (msg_size > 0) memcpy(message->msg, msg, msg_size);
            message->size = msg_size;
            if (msgq->r_idx[mtype] == msgq->w_idx[mtype]) pthread_cond_signal(&msgq->cond[mtype]);
            msgq->msg_idx[mtype][msgq->w_idx[mtype]] = msg_idx;
            msgq->w_idx[mtype] = MSG_NEXT(msgq->w_idx[mtype], ECP_MAX_CONN_MSG+1);

            msgq->empty_idx = MSG_NEXT(msg_idx, ECP_MAX_CONN_MSG);
            msgq->occupied[msg_idx] = 1;
            done = 1;
            break;
        }
        msg_idx = MSG_NEXT(msg_idx, ECP_MAX_CONN_MSG);
    }
    if (done) {
        return msg_size+1;
    } else {
        return ECP_ERR_MAX_CONN_MSG;
    }
}

ssize_t ecp_conn_msgq_pop(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, unsigned int timeout) {
    ECPConnMsgQ *msgq = &conn->msgq;
    ECPMessage *message;
    ssize_t rv = ECP_OK;
    unsigned short msg_idx;

    if (mtype >= ECP_MAX_MTYPE) return ECP_ERR_MAX_MTYPE;

    if (msgq->r_idx[mtype] == msgq->w_idx[mtype]) {
        if (timeout == -1) {
            pthread_cond_wait(&msgq->cond[mtype], &conn->mutex);
        } else {
            struct timespec ts;
            int _rv = pthread_cond_timedwait(&msgq->cond[mtype], &conn->mutex, abstime_ts(&ts, timeout));
            if (_rv) rv = ECP_ERR_TIMEOUT;
        }
    }
    if (!rv) {
        msg_idx = msgq->msg_idx[mtype][msgq->r_idx[mtype]];
        msgq->r_idx[mtype] = MSG_NEXT(msgq->r_idx[mtype], ECP_MAX_CONN_MSG+1);
        msgq->occupied[msg_idx] = 0;
        message = &msgq->msg[msg_idx];
        rv = message->size;
        if (rv >= 0) {
            rv = MIN(msg_size, rv);
            memcpy(msg, message->msg, rv);
        }
    }
    return rv;
}

#endif  /* ECP_WITH_PTHREAD */