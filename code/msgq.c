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
    
    for (i=0; i<ECP_MAX_PTYPE; i++) {
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
    
    for (i=0; i<ECP_MAX_PTYPE; i++) {
        pthread_cond_destroy(&conn->msgq.cond[i]);
    }
}

ssize_t ecp_conn_msgq_push(ECPConnection *conn, unsigned char *payload, size_t payload_size) {
    ECPConnMsgQ *msgq = &conn->msgq;
    unsigned short msg_idx = msgq->empty_idx;
    unsigned short i;
    unsigned short done = 0;
    unsigned char ptype;
    
    if (payload_size == 0) return ECP_OK;

    ptype = *payload;
    payload++;
    payload_size--;

    if (ptype >= ECP_MAX_PTYPE) return ECP_ERR_MAX_PTYPE;
    if (payload_size < ECP_MIN_MSG) return ECP_ERR_MIN_MSG;
    
    for (i=0; i<ECP_MAX_CONN_MSG; i++) {
        if (!msgq->occupied[msg_idx]) {
            ECPMessage *msg = &msgq->msg[msg_idx];
            if (payload_size > 0) memcpy(msg->payload, payload, payload_size);
            msg->size = payload_size;
            if (msgq->r_idx[ptype] == msgq->w_idx[ptype]) pthread_cond_signal(&msgq->cond[ptype]);
            msgq->msg_idx[ptype][msgq->w_idx[ptype]] = msg_idx;
            msgq->w_idx[ptype] = MSG_NEXT(msgq->w_idx[ptype], ECP_MAX_CONN_MSG+1);

            msgq->empty_idx = MSG_NEXT(msg_idx, ECP_MAX_CONN_MSG);
            msgq->occupied[msg_idx] = 1;
            done = 1;
            break;
        }
        msg_idx = MSG_NEXT(msg_idx, ECP_MAX_CONN_MSG);
    }
    if (done) {
        return payload_size+1;
    } else {
        return ECP_ERR_MAX_CONN_MSG;
    }
}

ssize_t ecp_conn_msgq_pop(ECPConnection *conn, unsigned char ptype, unsigned char *payload, size_t payload_size, unsigned int timeout) {
    ECPConnMsgQ *msgq = &conn->msgq;
    ECPMessage *msg;
    ssize_t rv = ECP_OK;
    unsigned short msg_idx;

    if (ptype >= ECP_MAX_PTYPE) return ECP_ERR_MAX_PTYPE;

    if (msgq->r_idx[ptype] == msgq->w_idx[ptype]) {
        if (timeout == -1) {
            pthread_cond_wait(&msgq->cond[ptype], &conn->mutex);
        } else {
            struct timespec ts;
            int _rv = pthread_cond_timedwait(&msgq->cond[ptype], &conn->mutex, abstime_ts(&ts, timeout));
            if (_rv) rv = ECP_ERR_TIMEOUT;
        }
    }
    if (!rv) {
        msg_idx = msgq->msg_idx[ptype][msgq->r_idx[ptype]];
        msgq->r_idx[ptype] = MSG_NEXT(msgq->r_idx[ptype], ECP_MAX_CONN_MSG+1);
        msgq->occupied[msg_idx] = 0;
        msg = &msgq->msg[msg_idx];
        rv = msg->size;
        if (rv >= 0) {
            rv = MIN(payload_size, rv);
            memcpy(payload, msg->payload, rv);
        }
    }
    return rv;
}

#endif  /* ECP_WITH_PTHREAD */