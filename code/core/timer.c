#include "core.h"

#include <string.h>

int ecp_timer_create(ECPTimer *timer) {
    int rv = ECP_OK;
    timer->head = -1;

#ifdef ECP_WITH_PTHREAD
    rv = pthread_mutex_init(&timer->mutex, NULL);
    if (rv) return ECP_ERR;
#endif
    
    return ECP_OK;
}

void ecp_timer_destroy(ECPTimer *timer) {
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_destroy(&timer->mutex);
#endif
}

int ecp_timer_item_init(ECPTimerItem *ti, ECPConnection *conn, unsigned char mtype, unsigned short cnt, unsigned int timeout) {
    if ((mtype & ECP_MTYPE_MASK) >= ECP_MAX_MTYPE) return ECP_ERR_MAX_MTYPE;
    
    if (ti == NULL) return ECP_ERR;
    if (conn == NULL) return ECP_ERR;
    
    ti->conn = conn;
    ti->mtype = mtype;
    ti->cnt = cnt-1;
    ti->timeout = timeout;
    ti->abstime = 0;
    ti->retry = NULL;
    ti->pld = NULL;
    ti->pld_size = 0;
    
    return ECP_OK;
}

int ecp_timer_push(ECPTimerItem *ti) {
    int i, is_reg, rv = ECP_OK;
    ECPConnection *conn = ti->conn;
    ECPContext *ctx = conn->sock->ctx;
    ECPTimer *timer = &conn->sock->timer;
    
    ti->abstime = ctx->tm.abstime_ms(ti->timeout);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&timer->mutex);
    pthread_mutex_lock(&conn->mutex);
#endif
    is_reg = ecp_conn_is_reg(conn);
    if (is_reg) conn->refcount++;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif
    
    if (timer->head == ECP_MAX_TIMER-1) rv = ECP_ERR_MAX_TIMER;
    if (!rv && !is_reg) rv = ECP_ERR_CLOSED;

    if (!rv) {
        for (i=timer->head; i>=0; i--) {
            if (timer->item[i].abstime >= ti->abstime) {
                if (i != timer->head) memmove(timer->item+i+2, timer->item+i+1, sizeof(ECPTimerItem) * (timer->head-i));
                timer->item[i+1] = *ti;
                timer->head++;
                break;
            }
        }
        if (i == -1) {
            if (timer->head != -1) memmove(timer->item+1, timer->item, sizeof(ECPTimerItem) * (timer->head+1));
            timer->item[0] = *ti;
            timer->head++;
        }
    }
    
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&timer->mutex);
#endif

    return rv;
}

void ecp_timer_pop(ECPConnection *conn, unsigned char mtype) {
    int i;
    ECPTimer *timer = &conn->sock->timer;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&timer->mutex);
#endif

    for (i=timer->head; i>=0; i--) {
        ECPConnection *curr_conn = timer->item[i].conn;
        if ((conn == curr_conn) && (mtype == timer->item[i].mtype)) {
            if (i != timer->head) {
                memmove(timer->item+i, timer->item+i+1, sizeof(ECPTimerItem) * (timer->head-i));
                memset(timer->item+timer->head, 0, sizeof(ECPTimerItem));
            } else {
                memset(timer->item+i, 0, sizeof(ECPTimerItem));
            }
#ifdef ECP_WITH_PTHREAD
            pthread_mutex_lock(&conn->mutex);
#endif            
            conn->refcount--;
#ifdef ECP_WITH_PTHREAD
            pthread_mutex_unlock(&conn->mutex);
#endif            
            timer->head--;
            break;
        }
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&timer->mutex);
#endif
    
}

void ecp_timer_remove(ECPConnection *conn) {
    int i;
    ECPTimer *timer = &conn->sock->timer;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&timer->mutex);
#endif

    for (i=timer->head; i>=0; i--) {
        ECPConnection *curr_conn = timer->item[i].conn;
        if (conn == curr_conn) {
            if (i != timer->head) {
                memmove(timer->item+i, timer->item+i+1, sizeof(ECPTimerItem) * (timer->head-i));
                memset(timer->item+timer->head, 0, sizeof(ECPTimerItem));
            } else {
                memset(timer->item+i, 0, sizeof(ECPTimerItem));
            }
#ifdef ECP_WITH_PTHREAD
            pthread_mutex_lock(&conn->mutex);
#endif
            conn->refcount--;
#ifdef ECP_WITH_PTHREAD
            pthread_mutex_unlock(&conn->mutex);
#endif
            timer->head--;
        }
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&timer->mutex);
#endif
    
}

unsigned int ecp_timer_exe(ECPSocket *sock) {
    int i;
    unsigned int ret = 0;
    ECPTimer *timer = &sock->timer;
    ECPTimerItem to_exec[ECP_MAX_TIMER];
    int to_exec_size = 0;
    unsigned int now = sock->ctx->tm.abstime_ms(0);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&timer->mutex);
#endif

    for (i=timer->head; i>=0; i--) {
        unsigned int abstime = timer->item[i].abstime;

        if (abstime > now) {
            ret = abstime - now;
            break;
        }
        to_exec[to_exec_size] = timer->item[i];
        to_exec_size++;
    }
    if (i != timer->head) {
        memset(timer->item+i+1, 0, sizeof(ECPTimerItem) * (timer->head-i));
        timer->head = i;
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&timer->mutex);
#endif

    for (i=to_exec_size-1; i>=0; i--) {
        int rv = ECP_OK;
        ECPConnection *conn = to_exec[i].conn;
        unsigned char mtype = to_exec[i].mtype;
        unsigned char *pld = to_exec[i].pld;
        unsigned char pld_size = to_exec[i].pld_size;
        ecp_timer_retry_t *retry = to_exec[i].retry;
        ecp_conn_handler_msg_t *handler = conn->sock->ctx->handler[conn->type] ? conn->sock->ctx->handler[conn->type]->msg[mtype & ECP_MTYPE_MASK] : NULL;
        
        if (to_exec[i].cnt) {
            ssize_t _rv = 0;
            to_exec[i].cnt--;
            if (retry) {
                _rv = retry(conn, to_exec+i);
                if (_rv < 0) rv = _rv;
            } else {
                _rv = ecp_pld_send_wtimer(conn, to_exec+i, pld, pld_size);
                if (_rv < 0) rv = _rv;
            }
            if (rv && (rv != ECP_ERR_CLOSED) && handler) handler(conn, 0, mtype, NULL, rv);
        } else if (handler) {
            handler(conn, 0, mtype, NULL, ECP_ERR_TIMEOUT);
        }

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&conn->mutex);
#endif
        conn->refcount--;
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn->mutex);
#endif

    }

    return ret;
}

ssize_t ecp_timer_send(ECPConnection *conn, ecp_timer_retry_t *send_f, unsigned char mtype, unsigned short cnt, unsigned int timeout) {
    int rv = ECP_OK;
    ECPTimerItem ti;

    rv = ecp_timer_item_init(&ti, conn, mtype, cnt, timeout);
    if (rv) return rv;

    ti.retry = send_f;
    return send_f(conn, &ti);
}
