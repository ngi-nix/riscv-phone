#include "core.h"

#include <string.h>

int ecp_timer_create(ECPTimer *timer) {
    timer->head = -1;

#ifdef ECP_WITH_PTHREAD
    int rv = pthread_mutex_init(&timer->mutex, NULL);
    if (rv) return ECP_ERR;
#endif
    
    return ECP_OK;
}

void ecp_timer_destroy(ECPTimer *timer) {
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_destroy(&timer->mutex);
#endif
}

int ecp_timer_item_init(ECPTimerItem *ti, ECPConnection *conn, unsigned char ptype, unsigned short cnt, unsigned int timeout) {
    unsigned int abstime = conn->sock->ctx->tm.abstime_ms(timeout);

    if (ptype >= ECP_MAX_PTYPE) return ECP_ERR_MAX_PTYPE;
    
    ti->conn = conn;
    ti->ptype = ptype;
    ti->cnt = cnt;
    ti->timeout = timeout;
    ti->abstime = abstime;
    ti->retry = NULL;
    ti->pld = NULL;
    ti->pld_size = 0;
    
    return ECP_OK;
}

int ecp_timer_push(ECPConnection *conn, ECPTimerItem *ti) {
    int i, is_reg, rv = ECP_OK;
    ECPTimer *timer = &conn->sock->timer;

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

void ecp_timer_pop(ECPConnection *conn, unsigned char ptype) {
    int i;
    ECPTimer *timer = &conn->sock->timer;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&timer->mutex);
#endif

    for (i=timer->head; i>=0; i--) {
        ECPConnection *curr_conn = timer->item[i].conn;
        if ((conn == curr_conn) && (ptype == timer->item[i].ptype)) {
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
    pthread_mutex_lock(&conn->mutex);
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
            conn->refcount--;
            timer->head--;
        }
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
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
        ECPSocket *sock = conn->sock;
        unsigned char ptype = to_exec[i].ptype;
        unsigned char *pld = to_exec[i].pld;
        unsigned char pld_size = to_exec[i].pld_size;
        ecp_timer_retry_t *retry = to_exec[i].retry;
        ecp_conn_handler_t *handler = (ptype < ECP_MAX_PTYPE_SYS) && sock->handler[ptype] ? sock->handler[ptype] : (conn->handler ? conn->handler->f[ptype] : NULL);

        if (to_exec[i].cnt) {
            to_exec[i].cnt--;
            to_exec[i].abstime = now + to_exec[i].timeout;
            if (retry) {
                rv = retry(conn, to_exec+i);
            } else {
                ssize_t _rv = pld ? ecp_pld_send(conn, pld, pld_size) : ecp_send(conn, ptype, NULL, 0);
                if (_rv < 0) rv = _rv;
            }
            if (!rv) rv = ecp_timer_push(conn, to_exec+i);
            if (rv && (rv != ECP_ERR_CLOSED) && handler) handler(conn, ptype, NULL, rv);
        } else if (handler) {
            handler(conn, ptype, NULL, ECP_ERR_TIMEOUT);
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

int ecp_timer_send(ECPConnection *conn, ecp_timer_retry_t *send_f, unsigned char ptype, unsigned short cnt, unsigned int timeout) {
    ECPTimerItem ti;

    int rv = ecp_timer_item_init(&ti, conn, ptype, cnt-1, timeout);
    if (rv) return rv;

    ti.retry = send_f;
    rv = ecp_timer_push(conn, &ti);
    if (rv) return rv;

    ssize_t _rv = send_f(conn, NULL);
    if (_rv < 0) {
        ecp_timer_pop(conn, ptype);
        return _rv;
    }

    return ECP_OK;
}
