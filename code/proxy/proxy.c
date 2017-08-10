#include "core.h"
#include "proxy.h"

#include <string.h>

#ifdef ECP_WITH_PTHREAD
static pthread_mutex_t key_perma_mutex;
static pthread_mutex_t key_next_mutex;
#endif

static void *key_perma_table;
static void *key_next_table;
    
static unsigned char key_null[ECP_ECDH_SIZE_KEY] = { 0 };

static ECPConnHandler handler_f;
static ECPConnHandler handler_b;

static int proxyf_create(ECPConnection *conn, unsigned char *payload, size_t size) {
    ECPContext *ctx = conn->sock->ctx;
    ECPConnProxyF *conn_p = (ECPConnProxyF *)conn;
    int rv;
    
    if (conn->out) return ECP_ERR;
    if (conn->type != ECP_CTYPE_PROXYF) return ECP_ERR;
    if (size < 2*ECP_ECDH_SIZE_KEY) return ECP_ERR;

    conn_p->key_next_curr = 0;
    memset(conn_p->key_next, 0, sizeof(conn_p->key_next));
    memset(conn_p->key_out, 0, sizeof(conn_p->key_out));
    memcpy(conn_p->key_next[conn_p->key_next_curr], payload, ECP_ECDH_SIZE_KEY);
    memcpy(conn_p->key_out, payload+ECP_ECDH_SIZE_KEY, ECP_ECDH_SIZE_KEY);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&key_next_mutex);
#endif
    rv = ctx->ht.init ? ctx->ht.insert(key_next_table, conn_p->key_next[conn_p->key_next_curr], conn) : ECP_ERR;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&key_next_mutex);
#endif
    if (rv) return rv;

    return ECP_OK;
}

static void proxyf_destroy(ECPConnection *conn) {
    ECPContext *ctx = conn->sock->ctx;
    ECPConnProxyF *conn_p = (ECPConnProxyF *)conn;

    if (conn->out) return;
    if (conn->type != ECP_CTYPE_PROXYF) return;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&key_next_mutex);
    pthread_mutex_lock(&conn->mutex);
#endif
    if (ctx->ht.init) {
        int i;
        for (i=0; i<ECP_MAX_NODE_KEY; i++) {
            if (memcmp(conn_p->key_next[i], key_null, ECP_ECDH_SIZE_KEY)) ctx->ht.remove(key_next_table, conn_p->key_next[i]);
        }
    }
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
    pthread_mutex_unlock(&key_next_mutex);
#endif
}

static ssize_t _proxyf_send_open(ECPConnection *conn) {
    ECPConnProxy *conn_p = (ECPConnProxy *)conn;
    ECPConnection *conn_next = conn_p->next;
    unsigned char payload[ECP_SIZE_PLD(0)];

    if (conn_next == NULL) return ECP_ERR;

    ecp_pld_set_type(payload, ECP_MTYPE_KGET_REQ);
    return ecp_pld_send_wkey(conn_next, ECP_ECDH_IDX_PERMA, ECP_ECDH_IDX_INV, payload, sizeof(payload));
}

static ssize_t _proxyf_retry_kget(ECPConnection *conn, ECPTimerItem *ti) {
    if (conn->proxy == NULL) return ECP_ERR;
    
    return _proxyf_send_open(conn->proxy);
}

static ssize_t proxyf_open(ECPConnection *conn) {
    int rv = ECP_OK;
    ECPTimerItem ti;
    ECPConnProxy *conn_p = (ECPConnProxy *)conn;
    ECPConnection *conn_next = conn_p->next;

    rv = ecp_timer_item_init(&ti, conn_next, ECP_MTYPE_KGET_REP, 3, 3000);
    if (rv) return rv;

    ti.retry = _proxyf_retry_kget;
    rv = ecp_timer_push(&ti);
    if (rv) return rv;

    return _proxyf_send_open(conn);
}

static ssize_t proxyf_handle_open(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size) {
    if (conn->type != ECP_CTYPE_PROXYF) return ECP_ERR;

    if (mtype & ECP_MTYPE_FLAG_REP) {
        if (!conn->out) return ECP_ERR;
        if (size < 0) {
            ecp_conn_handler_msg_t *handler = NULL;
            while (conn->type == ECP_CTYPE_PROXYF) {
                ECPConnProxy *conn_p = (ECPConnProxy *)conn;
                conn = conn_p->next;
            }
            handler = conn->sock->ctx->handler[conn->type] ? conn->sock->ctx->handler[conn->type]->msg[ECP_MTYPE_OPEN] : NULL;
            return handler ? handler(conn, seq, mtype, msg, size) : size;
        }

        return ecp_conn_handle_open(conn, seq, mtype, msg, size);
    } else {
        ECPContext *ctx = conn->sock->ctx;
        ECPConnProxyF *conn_p = (ECPConnProxyF *)conn;
        int rv = ECP_OK;
        unsigned char ctype = 0;

        if (conn->out) return ECP_ERR;
        if (size < 0) return size;
        if (size < 1+2*ECP_ECDH_SIZE_KEY) return ECP_ERR;

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&key_next_mutex);
        pthread_mutex_lock(&conn->mutex);
#endif

        ctype = msg[0];
        msg++;

        if (!ecp_conn_is_open(conn)) conn->flags |= ECP_CONN_FLAG_OPEN;
        if (memcmp(conn_p->key_next[conn_p->key_next_curr], msg, ECP_ECDH_SIZE_KEY)) {
            conn_p->key_next_curr = (conn_p->key_next_curr + 1) % ECP_MAX_NODE_KEY;
            if (memcmp(conn_p->key_next[conn_p->key_next_curr], key_null, ECP_ECDH_SIZE_KEY)) {
                if (ctx->ht.init) ctx->ht.remove(key_next_table, conn_p->key_next[conn_p->key_next_curr]);
            }
            memcpy(conn_p->key_next[conn_p->key_next_curr], msg, ECP_ECDH_SIZE_KEY);
            rv = ctx->ht.init ? ctx->ht.insert(key_next_table, conn_p->key_next[conn_p->key_next_curr], conn) : ECP_ERR;
        }

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn->mutex);
        pthread_mutex_unlock(&key_next_mutex);
#endif
    
        if (rv) return rv;

        return 1+2*ECP_ECDH_SIZE_KEY;
    }

    return ECP_ERR;
}

static ssize_t proxyf_handle_relay(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size) {
    ECPContext *ctx = conn->sock->ctx;
    ECPConnection *conn_out = NULL;
    ECPConnProxyF *conn_p = (ECPConnProxyF *)conn;
    unsigned char *payload = NULL;
    ssize_t rv;

    if (conn->out) return ECP_ERR;
    if (conn->type != ECP_CTYPE_PROXYF) return ECP_ERR;

    if (size < 0) return size;
    if (size < ECP_MIN_PKT) return ECP_ERR_MIN_PKT;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&key_perma_mutex);
#endif
    conn_out = ctx->ht.init ? ctx->ht.search(key_perma_table, conn_p->key_out) : NULL;
    if (conn_out) {
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&conn_out->mutex);
#endif
        conn_out->refcount++;
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn_out->mutex);
#endif
    }
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&key_perma_mutex);
#endif

    if (conn_out == NULL) return ECP_ERR;
    
    payload = msg - ECP_SIZE_MSG_HDR;
    ecp_pld_set_type(payload, ECP_MTYPE_EXEC);
    rv = ecp_pld_send(conn_out, payload, ECP_SIZE_MSG_HDR+size);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn_out->mutex);
#endif
    conn_out->refcount--;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn_out->mutex);
#endif

    if (rv < 0) return rv;
    return size;
}

static int proxyb_create(ECPConnection *conn, unsigned char *payload, size_t size) {
    ECPContext *ctx = conn->sock->ctx;
    int rv;
    
    if (conn->out) return ECP_ERR;
    if (conn->type != ECP_CTYPE_PROXYB) return ECP_ERR;

    // XXX should verify perma_key
    if (size < ECP_ECDH_SIZE_KEY) return ECP_ERR;
    ctx->cr.dh_pub_from_buf(&conn->node.public, payload);
    
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&key_perma_mutex);
#endif
    rv = ctx->ht.init ? ctx->ht.insert(key_perma_table, ctx->cr.dh_pub_get_buf(&conn->node.public), conn) : ECP_ERR;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&key_perma_mutex);
#endif
    if (rv) return rv;
    
    return ECP_OK;
}

static void proxyb_destroy(ECPConnection *conn) {
    ECPContext *ctx = conn->sock->ctx;

    if (conn->out) return;
    if (conn->type != ECP_CTYPE_PROXYB) return;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&key_perma_mutex);
#endif
    if (ctx->ht.init) ctx->ht.remove(key_perma_table, ctx->cr.dh_pub_get_buf(&conn->node.public));
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&key_perma_mutex);
#endif
}

static ssize_t _proxyb_send_open(ECPConnection *conn, ECPTimerItem *ti) {
    ECPSocket *sock = conn->sock;
    ECPContext *ctx = sock->ctx;
    unsigned char payload[ECP_SIZE_PLD(ECP_ECDH_SIZE_KEY+1)];
    unsigned char *buf = ecp_pld_get_buf(payload);
    int rv = ECP_OK;
    
    // XXX server should verify perma_key
    ecp_pld_set_type(payload, ECP_MTYPE_OPEN_REQ);
    buf[0] = conn->type;
    memcpy(buf+1, ctx->cr.dh_pub_get_buf(&sock->key_perma.public), ECP_ECDH_SIZE_KEY);

    return ecp_pld_send(conn, payload, sizeof(payload));
}

static ssize_t proxyb_open(ECPConnection *conn) {
    return ecp_timer_send(conn, _proxyb_send_open, ECP_MTYPE_OPEN_REP, 3, 500);
}

static ssize_t proxyb_handle_open(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size) {
    ssize_t rv;
    
    if (conn->type != ECP_CTYPE_PROXYB) return ECP_ERR;

    if (size < 0) return size;
    
    rv = ecp_conn_handle_open(conn, seq, mtype, msg, size);
    if (rv < 0) return rv;

    if (mtype & ECP_MTYPE_FLAG_REP) {
        if (!conn->out) return ECP_ERR;
        return 0;
    } else {
        if (conn->out) return ECP_ERR;
        if (size < rv+ECP_ECDH_SIZE_KEY) return ECP_ERR;

        // XXX should verify perma_key
        return rv+ECP_ECDH_SIZE_KEY;
    }
    return ECP_ERR;
}

static ssize_t proxyb_handle_relay(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size) {
    ECPContext *ctx = conn->sock->ctx;
    unsigned char *payload = NULL;
    ssize_t rv;

    if (conn->out) return ECP_ERR;
    if (conn->type != ECP_CTYPE_PROXYB) return ECP_ERR;

    if (size < 0) return size;
    if (size < ECP_MIN_PKT) return ECP_ERR_MIN_PKT;
    
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&key_next_mutex);
#endif
    conn = ctx->ht.init ? ctx->ht.search(key_next_table, msg+ECP_SIZE_PROTO+1) : NULL;
    if (conn) {
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&conn->mutex);
#endif
        conn->refcount++;
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn->mutex);
#endif
    }
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&key_next_mutex);
#endif

    if (conn == NULL) return ECP_ERR;
    
    payload = msg - ECP_SIZE_MSG_HDR;
    ecp_pld_set_type(payload, ECP_MTYPE_EXEC);
    rv = ecp_pld_send(conn, payload, ECP_SIZE_MSG_HDR+size);
    
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif
    conn->refcount--;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif
    
    if (rv < 0) return rv;
    return size;
}

static ssize_t proxy_set_msg(ECPConnection *conn, unsigned char *pld_out, size_t pld_out_size, unsigned char *pld_in, size_t pld_in_size) {
    if ((conn->type == ECP_CTYPE_PROXYF) && conn->out) {
        unsigned char mtype = ecp_pld_get_type(pld_in);
        if ((mtype == ECP_MTYPE_OPEN_REQ) || (mtype == ECP_MTYPE_KGET_REQ)) {
            ECPConnProxy *conn_p = (ECPConnProxy *)conn;
            ECPContext *ctx = conn->sock->ctx;
            ECPConnection *conn_next = conn_p->next;
            unsigned char *buf = NULL;
            int rv;

            if (pld_out_size < ECP_SIZE_MSG_HDR+2+2*ECP_ECDH_SIZE_KEY) return ECP_ERR;
            if (conn_next == NULL) return ECP_ERR;

            ecp_pld_set_type(pld_out, ECP_MTYPE_OPEN_REQ);
            buf = ecp_pld_get_buf(pld_out);

            buf[0] = ECP_CTYPE_PROXYF;
            rv = ecp_conn_dhkey_get_curr(conn_next, NULL, buf+1);
            if (rv) return rv;

            memcpy(buf+1+ECP_ECDH_SIZE_KEY, ctx->cr.dh_pub_get_buf(&conn_next->node.public), ECP_ECDH_SIZE_KEY);
            buf[1+2*ECP_ECDH_SIZE_KEY] = ECP_MTYPE_RELAY;

            return ECP_SIZE_MSG_HDR+2+2*ECP_ECDH_SIZE_KEY;
        }
    }

    ecp_pld_set_type(pld_out, ECP_MTYPE_RELAY);
    return ECP_SIZE_MSG_HDR;
}


static ssize_t proxy_pack(ECPConnection *conn, unsigned char *packet, size_t pkt_size, unsigned char s_idx, unsigned char c_idx, unsigned char *payload, size_t payload_size, ECPNetAddr *addr, ecp_seq_t *seq, int *rbuf_idx) {
    ECPContext *ctx = conn->sock->ctx;

    if (conn->proxy) {
        unsigned char payload_[ECP_MAX_PLD];
        ssize_t rv, hdr_size = proxy_set_msg(conn->proxy, payload_, sizeof(payload_), payload, payload_size);
        if (hdr_size < 0) return hdr_size;

        rv = ecp_conn_pack(conn, payload_+hdr_size, ECP_MAX_PLD-hdr_size, s_idx, c_idx, payload, payload_size, NULL, seq, rbuf_idx);
        if (rv < 0) return rv;

        return proxy_pack(conn->proxy, packet, pkt_size, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, payload_, rv+hdr_size, addr, NULL, NULL);
    } else {
        return ecp_conn_pack(conn, packet, pkt_size, s_idx, c_idx, payload, payload_size, addr, seq, rbuf_idx);
    }
}

static ssize_t proxy_pack_raw(ECPSocket *sock, ECPConnection *proxy, unsigned char *packet, size_t pkt_size, unsigned char s_idx, unsigned char c_idx, ecp_dh_public_t *public, ecp_aead_key_t *shsec, unsigned char *nonce, ecp_seq_t seq, unsigned char *payload, size_t payload_size, ECPNetAddr *addr) {
    ECPContext *ctx = sock->ctx;

    if (proxy) {
        unsigned char payload_[ECP_MAX_PLD];
        ssize_t rv, hdr_size = proxy_set_msg(proxy, payload_, sizeof(payload_), payload, payload_size);
        if (hdr_size < 0) return hdr_size;

        rv = ecp_pack(ctx, payload_+hdr_size, ECP_MAX_PLD-hdr_size, s_idx, c_idx, public, shsec, nonce, seq, payload, payload_size);
        if (rv < 0) return rv;

        return proxy_pack(proxy, packet, pkt_size, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, payload_, rv+hdr_size, addr, NULL, NULL);
    } else {
        return ecp_pack(ctx, packet, pkt_size, s_idx, c_idx, public, shsec, nonce, seq, payload, payload_size);
    }
}

int ecp_proxy_init(ECPContext *ctx) {
    int rv;
    
    rv = ecp_conn_handler_init(&handler_f);
    if (rv) return rv;
    
    handler_f.conn_create = proxyf_create;
    handler_f.conn_destroy = proxyf_destroy;
    handler_f.conn_open = proxyf_open;
    handler_f.msg[ECP_MTYPE_OPEN] = proxyf_handle_open;
    handler_f.msg[ECP_MTYPE_EXEC] = ecp_conn_handle_exec;
    handler_f.msg[ECP_MTYPE_RELAY] = proxyf_handle_relay;
    ctx->handler[ECP_CTYPE_PROXYF] = &handler_f;

    rv = ecp_conn_handler_init(&handler_b);
    if (rv) return rv;

    handler_b.conn_create = proxyb_create;
    handler_b.conn_destroy = proxyb_destroy;
    handler_b.conn_open = proxyb_open;
    handler_b.msg[ECP_MTYPE_OPEN] = proxyb_handle_open;
    handler_b.msg[ECP_MTYPE_EXEC] = ecp_conn_handle_exec;
    handler_b.msg[ECP_MTYPE_RELAY] = proxyb_handle_relay;
    ctx->handler[ECP_CTYPE_PROXYB] = &handler_b;

    ctx->pack = proxy_pack;
    ctx->pack_raw = proxy_pack_raw;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_init(&key_perma_mutex, NULL);
    pthread_mutex_init(&key_next_mutex, NULL);
#endif

    if (ctx->ht.init) {
        key_perma_table = ctx->ht.create(ctx);
        key_next_table = ctx->ht.create(ctx);
    }

    return ECP_OK;
}

int ecp_conn_proxy_init(ECPConnection *conn, ECPNode *conn_node, ECPConnProxy proxy[], ECPNode proxy_node[], int size) {
    ECPSocket *sock = conn->sock;
    int i, rv;
    
    rv = ecp_conn_init(conn, conn_node);
    if (rv) return rv;

    conn->proxy = (ECPConnection *)&proxy[size-1];
    for (i=0; i<size; i++) {
        rv = ecp_conn_create((ECPConnection *)&proxy[i], sock, ECP_CTYPE_PROXYF);
        if (rv) return rv;

        rv = ecp_conn_init((ECPConnection *)&proxy[i], &proxy_node[i]);
        if (rv) return rv;

        if (i == 0) {
            proxy[i].b.proxy = NULL;
        } else {
            proxy[i].b.proxy = (ECPConnection *)&proxy[i-1];
        }
        if (i == size-1) {
            proxy[i].next = conn;
        } else {
            proxy[i].next = (ECPConnection *)&proxy[i+1];
        }
    }
    
    return ECP_OK;
}

static ssize_t _proxy_send_kget(ECPConnection *conn, ECPTimerItem *ti) {
    unsigned char payload[ECP_SIZE_PLD(0)];

    ecp_pld_set_type(payload, ECP_MTYPE_KGET_REQ);
    return ecp_pld_send_wkey(conn, ECP_ECDH_IDX_PERMA, ECP_ECDH_IDX_INV, payload, sizeof(payload));
}

int ecp_conn_proxy_open(ECPConnection *conn, ECPNode *conn_node, ECPConnProxy proxy[], ECPNode proxy_node[], int size) {
    int rv = ecp_conn_proxy_init(conn, conn_node, proxy, proxy_node, size);
    if (rv) return rv;

    ssize_t _rv = ecp_timer_send((ECPConnection *)&proxy[0], _proxy_send_kget, ECP_MTYPE_KGET_REP, 3, 500);
    if (_rv < 0) return _rv;
    
    return ECP_OK;
}