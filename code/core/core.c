#include "core.h"

#include <string.h>

int ecp_dhkey_generate(ECPContext *ctx, ECPDHKey *key) {
    int rv;
    
    if (ctx->rng == NULL) return ECP_ERR_RNG;

    rv = ctx->cr.dh_mkpair(&key->public, &key->private, ctx->rng);
    if (rv) return rv;

    key->valid = 1;
    return ECP_OK;
}

int ecp_node_init(ECPContext *ctx, ECPNode *node, ecp_dh_public_t *public, void *addr) {
    int rv = ECP_OK;
    
    memset(node, 0, sizeof(ECPNode));
    memcpy(&node->public, public, sizeof(node->public));

    if (addr) rv = ctx->tr.addr_set(&node->addr, addr);
    if (rv) return ECP_ERR_NET_ADDR;

    return ECP_OK;
}

int ecp_ctx_create(ECPContext *ctx) {
    int rv;
    
    if (ctx == NULL) return ECP_ERR;

    memset(ctx, 0, sizeof(ECPContext));

    ctx->pack = ecp_conn_pack;
    ctx->pack_raw = ecp_pack_raw;

    rv = ecp_crypto_init(&ctx->cr);
    if (rv) return rv;
    rv = ecp_transport_init(&ctx->tr);
    if (rv) return rv;
    rv = ecp_time_init(&ctx->tm);
    if (rv) return rv;
#ifdef ECP_WITH_HTABLE
    rv = ecp_htable_init(&ctx->ht);
    if (rv) return rv;
#endif
    
    return ECP_OK;
}

int ecp_ctx_destroy(ECPContext *ctx) {
    return ECP_OK;
}

static int ctable_create(ECPSockCTable *conn, ECPContext *ctx) {
    int rv = ECP_OK;
    
    memset(conn, 0, sizeof(ECPSockCTable));

#ifdef ECP_WITH_HTABLE
    if (ctx->ht.init) {
        conn->htable = ctx->ht.create(ctx);
        if (conn->htable == NULL) return ECP_ERR_ALLOC;
    }
#endif

#ifdef ECP_WITH_PTHREAD
    rv = pthread_mutex_init(&conn->mutex, NULL);
    if (rv) {
#ifdef ECP_WITH_HTABLE
        if (ctx->ht.init) ctx->ht.destroy(conn->htable);
#endif
        return ECP_ERR;
    }
#endif
    
    return ECP_OK;
}

static void ctable_destroy(ECPSockCTable *conn, ECPContext *ctx) {
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_destroy(&conn->mutex);
#endif
#ifdef ECP_WITH_HTABLE
    if (ctx->ht.init) ctx->ht.destroy(conn->htable);
#endif
}

static int ctable_insert(ECPConnection *conn) {
    int with_htable = 0;
    ECPSocket *sock = conn->sock;
    ECPContext *ctx = sock->ctx;

#ifdef ECP_WITH_HTABLE
    if (ctx->ht.init) with_htable = 1;
#endif
    
    if (with_htable) {
#ifdef ECP_WITH_HTABLE
        int i, rv = ECP_OK;
        if (conn->out) {
            for (i=0; i<ECP_MAX_CONN_KEY; i++) {
                if (conn->key[i].valid) rv = ctx->ht.insert(sock->conn.htable, ctx->cr.dh_pub_get_buf(&conn->key[i].public), conn);
                if (rv) {
                    int j;
                    for (j=0; j<i; j++) if (conn->key[j].valid) ctx->ht.remove(sock->conn.htable, ctx->cr.dh_pub_get_buf(&conn->key[j].public));
                    return rv;
                }
            }
        } else {
            ECPDHRKeyBucket *remote = &conn->remote;

            for (i=0; i<ECP_MAX_NODE_KEY; i++) {
                if (remote->key[i].idx != ECP_ECDH_IDX_INV) rv = ctx->ht.insert(sock->conn.htable, ctx->cr.dh_pub_get_buf(&remote->key[i].public), conn);
                if (rv) {
                    int j;
                    for (j=0; j<i; j++) if (remote->key[j].idx != ECP_ECDH_IDX_INV) ctx->ht.remove(sock->conn.htable, ctx->cr.dh_pub_get_buf(&remote->key[j].public));
                    return rv;
                }
            }
        }
#endif
    } else {
        if (sock->conn.size == ECP_MAX_SOCK_CONN) return ECP_ERR_MAX_SOCK_CONN;
        sock->conn.array[sock->conn.size] = conn;
        sock->conn.size++;
    }

    return ECP_OK;
}

static void ctable_remove(ECPConnection *conn) {
    int i, with_htable = 0;
    ECPSocket *sock = conn->sock;
    ECPContext *ctx = sock->ctx;
    
#ifdef ECP_WITH_HTABLE
    if (ctx->ht.init) with_htable = 1;
#endif

    if (with_htable) {
#ifdef ECP_WITH_HTABLE
        if (conn->out) {
            for (i=0; i<ECP_MAX_CONN_KEY; i++) if (conn->key[i].valid) ctx->ht.remove(sock->conn.htable, ctx->cr.dh_pub_get_buf(&conn->key[i].public));
        } else {
            ECPDHRKeyBucket *remote = &conn->remote;
            for (i=0; i<ECP_MAX_NODE_KEY; i++) if (remote->key[i].idx != ECP_ECDH_IDX_INV) ctx->ht.remove(sock->conn.htable, ctx->cr.dh_pub_get_buf(&remote->key[i].public));
        }
#endif
    } else {
        for (i=0; i<sock->conn.size; i++) {
            if (conn == sock->conn.array[i]) {
                sock->conn.array[i] = sock->conn.array[sock->conn.size-1];
                sock->conn.array[sock->conn.size-1] = NULL;
                sock->conn.size--;
                return;
            }
        }
    }
}

static ECPConnection *ctable_search(ECPSocket *sock, unsigned char c_idx, unsigned char *c_public, ECPNetAddr *addr) {
    int i, with_htable = 0;
    ECPConnection *conn = NULL;
    
#ifdef ECP_WITH_HTABLE
    if (sock->ctx->ht.init) with_htable = 1;
#endif

    if (with_htable) {
#ifdef ECP_WITH_HTABLE
        return sock->ctx->ht.search(sock->conn.htable, c_public);
#endif
    } else {
        if (c_public) {
            for (i=0; i<sock->conn.size; i++) {
                conn = sock->conn.array[i];
                if (conn->out) {
                    if ((c_idx < ECP_MAX_CONN_KEY) && conn->key[c_idx].valid && sock->ctx->cr.dh_pub_eq(c_public, &conn->key[c_idx].public))
                        return conn;
                } else {
                    if ((c_idx < ECP_MAX_SOCK_KEY) && (conn->remote.key_idx_map[c_idx] != ECP_ECDH_IDX_INV) && sock->ctx->cr.dh_pub_eq(c_public, &conn->remote.key[conn->remote.key_idx_map[c_idx]].public))
                        return conn;
                }
            }
        } else if (addr) {
            /* in case server is not returning client's public key in packet */
            for (i=0; i<sock->conn.size; i++) {
                conn = sock->conn.array[i];
                if (conn->out && conn->sock->ctx->tr.addr_eq(&conn->node.addr, addr)) return conn;
            }
        }
    }

    return NULL;
}

int ecp_sock_create(ECPSocket *sock, ECPContext *ctx, ECPDHKey *key) {
    int rv = ECP_OK; 
    
    if (sock == NULL) return ECP_ERR;
    if (ctx == NULL) return ECP_ERR;

    memset(sock, 0, sizeof(ECPSocket));
    sock->ctx = ctx;
    sock->poll_timeout = ECP_POLL_TIMEOUT;
    sock->key_curr = 0;
    if (key) sock->key_perma = *key;
    sock->conn_new = ecp_conn_handle_new;

    rv = ecp_dhkey_generate(sock->ctx, &sock->key[sock->key_curr]);
    if (!rv) rv = ctable_create(&sock->conn, sock->ctx);
    
    if (rv) return rv;
    
    rv = ecp_timer_create(&sock->timer);
    if (rv) {
        ctable_destroy(&sock->conn, sock->ctx);
        return rv;
    }
    
#ifdef ECP_WITH_PTHREAD
    rv = pthread_mutex_init(&sock->mutex, NULL);
    if (rv) {
        ecp_timer_destroy(&sock->timer);
        ctable_destroy(&sock->conn, sock->ctx);
        return ECP_ERR;
    }
#endif

    return ECP_OK;
}

void ecp_sock_destroy(ECPSocket *sock) {
    ecp_timer_destroy(&sock->timer);
    ctable_destroy(&sock->conn, sock->ctx);
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_destroy(&sock->mutex);
#endif
}

int ecp_sock_open(ECPSocket *sock, void *myaddr) {
    return sock->ctx->tr.open(&sock->sock, myaddr);
}

void ecp_sock_close(ECPSocket *sock) {
    sock->ctx->tr.close(&sock->sock);
}

int ecp_sock_dhkey_get_curr(ECPSocket *sock, unsigned char *idx, unsigned char *public) {
    unsigned char _idx;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->mutex);
#endif

    _idx = sock->key_curr;
    if (_idx != ECP_ECDH_IDX_INV) sock->ctx->cr.dh_pub_to_buf(public, &sock->key[_idx].public);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&sock->mutex);
#endif

    if (_idx == ECP_ECDH_IDX_INV) return ECP_ERR_ECDH_IDX;
    
    if (idx) *idx = _idx;
    return ECP_OK;
}

int ecp_sock_dhkey_new(ECPSocket *sock) {
    ECPDHKey new_key;
    int rv = ecp_dhkey_generate(sock->ctx, &new_key);
    if (rv) return rv;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->mutex);
#endif
    sock->key_curr = (sock->key_curr + 1) % ECP_MAX_SOCK_KEY;
    sock->key[sock->key_curr] = new_key;
    sock->key[(sock->key_curr + 1) % ECP_MAX_SOCK_KEY].valid = 0;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&sock->mutex);
#endif

    return ECP_OK;
}

static ECPDHKey *conn_dhkey_get(ECPConnection *conn, unsigned char idx) {
    if (conn->key_curr == ECP_ECDH_IDX_INV) {
        if (idx < ECP_MAX_SOCK_KEY) return &conn->sock->key[idx];
    } else {
        if (idx < ECP_MAX_CONN_KEY) return &conn->key[idx];
    }
    return NULL;
}

static int conn_dhkey_new_pair(ECPConnection *conn, ECPDHKey *key) {
    /* called when client makes new key pair */
    ECPSocket *sock = conn->sock;
    ECPContext *ctx = sock->ctx;

    conn->key_curr = conn->key_curr == ECP_ECDH_IDX_INV ? 0 : (conn->key_curr+1) % ECP_MAX_CONN_KEY;
#ifdef ECP_WITH_HTABLE
    if (ctx->ht.init && conn->out && ecp_conn_is_reg(conn) && conn->key[conn->key_curr].valid) {
        ctx->ht.remove(sock->conn.htable, ctx->cr.dh_pub_get_buf(&conn->key[conn->key_curr].public));
    }
#endif
    
    conn->key[conn->key_curr] = *key;
    conn->key_idx_map[conn->key_curr] = ECP_ECDH_IDX_INV;

#ifdef ECP_WITH_HTABLE
    if (ctx->ht.init && conn->out && ecp_conn_is_reg(conn)) {
        int rv = ctx->ht.insert(sock->conn.htable, ctx->cr.dh_pub_get_buf(&conn->key[conn->key_curr].public), conn);
        if (rv) return rv;
    }
#endif
    
    return ECP_OK;
}

static void conn_dhkey_del_pair(ECPConnection *conn, unsigned char idx) {
    ECPSocket *sock = conn->sock;
    ECPContext *ctx = sock->ctx;
    
#ifdef ECP_WITH_HTABLE
    if (ctx->ht.init && conn->out && ecp_conn_is_reg(conn) && conn->key[idx].valid) {
        ctx->ht.remove(sock->conn.htable, ctx->cr.dh_pub_get_buf(&conn->key[idx].public));
    }
#endif
    
    memset(&conn->key[idx], 0, sizeof(conn->key[idx]));
    conn->key_idx_map[idx] = ECP_ECDH_IDX_INV;
}

static int conn_dhkey_new_pub_local(ECPConnection *conn, unsigned char idx) {
    // Remote obtained our key
    unsigned char new = conn->key_idx_curr == ECP_ECDH_IDX_INV ? 0 : (conn->key_idx_curr+1) % ECP_MAX_NODE_KEY;
    int i;
    
    if (idx >= ECP_MAX_SOCK_KEY) return ECP_ERR_ECDH_IDX;

    if (conn->key_idx[new] != ECP_ECDH_IDX_INV) conn->key_idx_map[conn->key_idx[new]] = ECP_ECDH_IDX_INV;
    conn->key_idx_map[idx] = new;
    conn->key_idx[new] = idx;
    conn->key_idx_curr = new;

    for (i=0; i<ECP_MAX_NODE_KEY; i++) {
        conn->shared[new][i].valid = 0;
    }
    return ECP_OK;
}


static int conn_dhkey_new_pub_remote(ECPConnection *conn, unsigned char idx, unsigned char *public) {
    // We obtained remote key
    ECPSocket *sock = conn->sock;
    ECPContext *ctx = sock->ctx;
    ECPDHRKeyBucket *remote = &conn->remote;
    unsigned char new = remote->key_curr == ECP_ECDH_IDX_INV ? 0 : (remote->key_curr+1) % ECP_MAX_NODE_KEY;
    int i;

    if (idx >= ECP_MAX_SOCK_KEY) return ECP_ERR_ECDH_IDX;
    if ((remote->key_idx_map[idx] != ECP_ECDH_IDX_INV) && ctx->cr.dh_pub_eq(public, &remote->key[remote->key_idx_map[idx]].public)) return ECP_ERR_ECDH_KEY_DUP;

#ifdef ECP_WITH_HTABLE
    if (ctx->ht.init && !conn->out && ecp_conn_is_reg(conn) && (remote->key[new].idx != ECP_ECDH_IDX_INV)) {
        ctx->ht.remove(sock->conn.htable, ctx->cr.dh_pub_get_buf(&remote->key[new].public));
    }
#endif
        
    if (remote->key[new].idx != ECP_ECDH_IDX_INV) remote->key_idx_map[remote->key[new].idx] = ECP_ECDH_IDX_INV;
    remote->key_idx_map[idx] = new;
    ctx->cr.dh_pub_from_buf(&remote->key[new].public, public);
    remote->key[new].idx = idx;
    remote->key_curr = new;

#ifdef ECP_WITH_HTABLE
    if (ctx->ht.init && !conn->out && ecp_conn_is_reg(conn)) {
        int rv = ctx->ht.insert(sock->conn.htable, ctx->cr.dh_pub_get_buf(&remote->key[new].public), conn);
        if (rv) return rv;
    }
#endif
    
    for (i=0; i<ECP_MAX_NODE_KEY; i++) {
        conn->shared[i][new].valid = 0;
    }
    return ECP_OK;
}

static int conn_shsec_get(ECPConnection *conn, unsigned char s_idx, unsigned char c_idx, ecp_aead_key_t *shsec) {
    if (s_idx == ECP_ECDH_IDX_PERMA) {
        ECPDHKey *priv = NULL;
        ecp_dh_public_t *public_p = NULL;
        
        if (conn->out) {
            public_p = &conn->node.public;
            priv = conn_dhkey_get(conn, c_idx);
        } else {
            ECPDHRKey *pub = NULL;
            
            if (c_idx >= ECP_MAX_SOCK_KEY) return ECP_ERR_ECDH_IDX;
            if (conn->remote.key_idx_map[c_idx] == ECP_ECDH_IDX_INV) return ECP_ERR_ECDH_IDX_REMOTE;

            pub = &conn->remote.key[conn->remote.key_idx_map[c_idx]];
            public_p = pub->idx != ECP_ECDH_IDX_INV ? &pub->public : NULL;
            priv = &conn->sock->key_perma;
        }
        if (public_p == NULL) return ECP_ERR_ECDH_IDX;
        if ((priv == NULL) || !priv->valid) return ECP_ERR_ECDH_IDX;
        conn->sock->ctx->cr.dh_shsec(shsec, public_p, &priv->private);
    } else {
        unsigned char l_idx = conn->out ? c_idx : s_idx;
        unsigned char r_idx = conn->out ? s_idx : c_idx;
        ECPDHShared *shared = NULL;

        if ((l_idx >= ECP_MAX_SOCK_KEY) || (r_idx >= ECP_MAX_SOCK_KEY)) return ECP_ERR_ECDH_IDX;

        if (conn->key_idx_map[l_idx] == ECP_ECDH_IDX_INV) return ECP_ERR_ECDH_IDX_LOCAL;
        if (conn->remote.key_idx_map[r_idx] == ECP_ECDH_IDX_INV) return ECP_ERR_ECDH_IDX_REMOTE;

        shared = &conn->shared[conn->key_idx_map[l_idx]][conn->remote.key_idx_map[r_idx]];

        if (!shared->valid) {
            ECPDHRKeyBucket *remote = &conn->remote;
            ECPDHRKey *pub = &remote->key[conn->remote.key_idx_map[r_idx]];
            ECPDHKey *priv = conn_dhkey_get(conn, l_idx);

            if ((pub == NULL) || (pub->idx == ECP_ECDH_IDX_INV)) return ECP_ERR_ECDH_IDX;
            if ((priv == NULL) || !priv->valid) return ECP_ERR_ECDH_IDX;
            conn->sock->ctx->cr.dh_shsec(&shared->secret, &pub->public, &priv->private);
            shared->valid = 1;
        }

        memcpy(shsec, &shared->secret, sizeof(shared->secret));
    }
    
    return ECP_OK;
}

static int conn_shsec_set(ECPConnection *conn, unsigned char s_idx, unsigned char c_idx, ecp_aead_key_t *shsec) {
    unsigned char l_idx = conn->out ? c_idx : s_idx;
    unsigned char r_idx = conn->out ? s_idx : c_idx;
    ECPDHShared *shared = NULL;
    
    if ((l_idx >= ECP_MAX_SOCK_KEY) || (r_idx >= ECP_MAX_SOCK_KEY)) return ECP_ERR_ECDH_IDX;

    if (conn->key_idx_map[l_idx] == ECP_ECDH_IDX_INV) return ECP_ERR_ECDH_IDX_LOCAL;
    if (conn->remote.key_idx_map[r_idx] == ECP_ECDH_IDX_INV) return ECP_ERR_ECDH_IDX_REMOTE;

    shared = &conn->shared[conn->key_idx_map[l_idx]][conn->remote.key_idx_map[r_idx]];
    memcpy(&shared->secret, shsec, sizeof(shared->secret));
    shared->valid = 1;
    
    return ECP_OK;
}

int ecp_conn_create(ECPConnection *conn, ECPSocket *sock, unsigned char ctype) {
    int i, rv = ECP_OK;
    
    if (conn == NULL) return ECP_ERR;
    if (sock == NULL) return ECP_ERR;

    memset(conn, 0, sizeof(ECPConnection));

    if (ctype >= ECP_MAX_CTYPE) return ECP_ERR_MAX_CTYPE;
        
    conn->type = ctype;
    conn->key_curr = ECP_ECDH_IDX_INV;
    conn->key_idx_curr = ECP_ECDH_IDX_INV;
    conn->remote.key_curr = ECP_ECDH_IDX_INV;
    memset(conn->key_idx, ECP_ECDH_IDX_INV, sizeof(conn->key_idx));
    memset(conn->key_idx_map, ECP_ECDH_IDX_INV, sizeof(conn->key_idx_map));
    for (i=0; i<ECP_MAX_NODE_KEY; i++) {
        conn->remote.key[i].idx = ECP_ECDH_IDX_INV;
    }
    memset(conn->remote.key_idx_map, ECP_ECDH_IDX_INV, sizeof(conn->remote.key_idx_map));

    conn->sock = sock;

#ifdef ECP_WITH_PTHREAD
    rv = pthread_mutex_init(&conn->mutex, NULL);
    if (rv) return ECP_ERR;
#endif

    return ECP_OK;
}

void ecp_conn_destroy(ECPConnection *conn) {
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_destroy(&conn->mutex);
#endif
}

int ecp_conn_register(ECPConnection *conn) {
    ECPSocket *sock = conn->sock;

    conn->flags |= ECP_CONN_FLAG_REG;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn.mutex);
#endif
    int rv = ctable_insert(conn);
    if (rv) conn->flags &= ~ECP_CONN_FLAG_REG;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&sock->conn.mutex);
#endif

    return rv;
}

void ecp_conn_unregister(ECPConnection *conn) {
    ECPSocket *sock = conn->sock;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn.mutex);
    pthread_mutex_lock(&conn->mutex);
#endif
    if (ecp_conn_is_reg(conn)) {
        ctable_remove(conn);
        conn->flags &= ~ECP_CONN_FLAG_REG;
    } 
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
    pthread_mutex_unlock(&sock->conn.mutex);
#endif
}

static ssize_t _conn_send_kget(ECPConnection *conn, ECPTimerItem *ti) {
    unsigned char payload[ECP_SIZE_PLD(0, 0)];

    ecp_pld_set_type(payload, ECP_MTYPE_KGET_REQ);
    return ecp_pld_send_ll(conn, ti, ECP_ECDH_IDX_PERMA, ECP_ECDH_IDX_INV, payload, sizeof(payload));
}

int ecp_conn_init(ECPConnection *conn, ECPNode *node) {
    ECPDHKey key;
    ECPContext *ctx = conn->sock->ctx;
    int rv = ECP_OK;

    if (node == NULL) return ECP_ERR;

    conn->out = 1;
    conn->node = *node;
    rv = ecp_dhkey_generate(ctx, &key);
    if (!rv) rv = ctx->rng(conn->nonce, ECP_AEAD_SIZE_NONCE);
    
    if (!rv) rv = conn_dhkey_new_pair(conn, &key);
    if (!rv) rv = conn_dhkey_new_pub_local(conn, conn->key_curr);
    if (!rv) rv = ecp_conn_register(conn);
    return rv;
}

int ecp_conn_open(ECPConnection *conn, ECPNode *node) {
    int rv = ecp_conn_init(conn, node);
    if (rv) return rv;
    
    ssize_t _rv = ecp_timer_send(conn, _conn_send_kget, ECP_MTYPE_KGET_REP, 3, 500);
    if (_rv < 0) {
        ecp_conn_unregister(conn);
        return _rv;
    }

    return ECP_OK;
}

int ecp_conn_close(ECPConnection *conn, ecp_cts_t timeout) {
    ECPSocket *sock = conn->sock;
    int refcount = 0;
    
    ecp_conn_unregister(conn);
    ecp_timer_remove(conn);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
    refcount = conn->refcount;
    pthread_mutex_unlock(&conn->mutex);
    
    if (timeout && refcount) {
        sock->ctx->tm.sleep_ms(timeout);
        pthread_mutex_lock(&conn->mutex);
        refcount = conn->refcount;
        pthread_mutex_unlock(&conn->mutex);
    }

    if (refcount) return ECP_ERR_TIMEOUT;
#endif

    if (conn->out) {
        ecp_conn_close_t *handler = conn->sock->ctx->handler[conn->type] ? conn->sock->ctx->handler[conn->type]->conn_close : NULL;
        if (handler) handler(conn);
    } else {
        ecp_conn_destroy_t *handler = conn->sock->ctx->handler[conn->type] ? conn->sock->ctx->handler[conn->type]->conn_destroy : NULL;
        if (handler) handler(conn);
        if (conn->parent) {
#ifdef ECP_WITH_PTHREAD
            pthread_mutex_lock(&conn->parent->mutex);
#endif
            conn->parent->refcount--;
#ifdef ECP_WITH_PTHREAD
            pthread_mutex_unlock(&conn->parent->mutex);
#endif
        }
        ecp_conn_destroy(conn);
        if (sock->ctx->conn_free) sock->ctx->conn_free(conn);
    }
    
    return ECP_OK;
}

int ecp_conn_reset(ECPConnection *conn) {
    ECPDHKey key;
    ECPSocket *sock = conn->sock;
    ECPContext *ctx = sock->ctx;
    int rv = ECP_OK;
    int i;

    rv = ecp_dhkey_generate(ctx, &key);
    if (rv) return rv;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn.mutex);
    pthread_mutex_lock(&conn->mutex);
#endif

    for (i=0; i<ECP_MAX_CONN_KEY; i++) {
        conn_dhkey_del_pair(conn, i);
    }
    conn->key_curr = 0;
    rv = conn_dhkey_new_pair(conn, &key);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&sock->conn.mutex);
#endif

    if (!rv) rv = conn_dhkey_new_pub_local(conn, conn->key_curr);
    if (!rv) rv = ctx->rng(conn->nonce, ECP_AEAD_SIZE_NONCE);
    conn->flags &= ~ECP_CONN_FLAG_OPEN;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    return rv;
}

int ecp_conn_handler_init(ECPConnHandler *handler) {
    memset(handler, 0, sizeof(ECPConnHandler));
    handler->msg[ECP_MTYPE_OPEN] = ecp_conn_handle_open;
    handler->msg[ECP_MTYPE_KGET] = ecp_conn_handle_kget;
    handler->msg[ECP_MTYPE_KPUT] = ecp_conn_handle_kput;
#ifdef ECP_WITH_RBUF
    handler->msg[ECP_MTYPE_RBACK] = ecp_rbuf_handle_ack;
    handler->msg[ECP_MTYPE_RBFLUSH] = ecp_rbuf_handle_flush;
    handler->msg[ECP_MTYPE_RBFLUSH_PTS] = ecp_rbuf_handle_flush_pts;
#endif
    handler->conn_open = ecp_conn_send_open;
    return ECP_OK;
}

static ssize_t _conn_send_open(ECPConnection *conn, ECPTimerItem *ti) {
    unsigned char payload[ECP_SIZE_PLD(1, 0)];
    unsigned char *buf = ecp_pld_get_buf(payload, 0);
    
    ecp_pld_set_type(payload, ECP_MTYPE_OPEN_REQ);
    buf[0] = conn->type;
    
    return ecp_pld_send_wtimer(conn, ti, payload, sizeof(payload));
}

ssize_t ecp_conn_send_open(ECPConnection *conn) {
    return ecp_timer_send(conn, _conn_send_open, ECP_MTYPE_OPEN_REP, 3, 500);
}

int ecp_conn_handle_new(ECPSocket *sock, ECPConnection **_conn, ECPConnection *parent, unsigned char s_idx, unsigned char c_idx, unsigned char *c_public, ecp_aead_key_t *shsec, unsigned char *payload, size_t payload_size) {
    ECPConnection *conn = NULL;
    int rv = ECP_OK;
    unsigned char ctype = 0;
    ecp_conn_create_t *handle_create = NULL;
    ecp_conn_destroy_t *handle_destroy = NULL;
    
    if (payload_size < 1) return ECP_ERR;
    
    ctype = payload[0];

    conn = sock->ctx->conn_alloc ? sock->ctx->conn_alloc(ctype) : NULL;
    if (conn == NULL) return ECP_ERR_ALLOC;

    rv = ecp_conn_create(conn, sock, ctype);
    if (rv) {
        if (sock->ctx->conn_free) sock->ctx->conn_free(conn);
        return rv;
    }
    
    conn->refcount = 1;
    conn->parent = parent;
    handle_create = conn->sock->ctx->handler[conn->type] ? conn->sock->ctx->handler[conn->type]->conn_create : NULL;
    handle_destroy = conn->sock->ctx->handler[conn->type] ? conn->sock->ctx->handler[conn->type]->conn_destroy : NULL;
    
    rv = conn_dhkey_new_pub_local(conn, s_idx);
    if (!rv) rv = conn_dhkey_new_pub_remote(conn, c_idx, c_public);
    if (!rv) rv = conn_shsec_set(conn, s_idx, c_idx, shsec);
    if (rv) {
        ecp_conn_destroy(conn);
        if (sock->ctx->conn_free) sock->ctx->conn_free(conn);
        return rv;
    }
    if (handle_create) rv = handle_create(conn, payload+1, payload_size-1);
    if (rv) {
        ecp_conn_destroy(conn);
        if (sock->ctx->conn_free) sock->ctx->conn_free(conn);
        return rv;
    }

    rv = ecp_conn_register(conn);
    if (rv) {
        if (handle_destroy) handle_destroy(conn);
        ecp_conn_destroy(conn);
        if (sock->ctx->conn_free) sock->ctx->conn_free(conn);
        return rv;
    }

    if (parent) {
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&parent->mutex);
#endif
        parent->refcount++;
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&parent->mutex);
#endif
    }

    *_conn = conn;
    return rv;
}

ssize_t ecp_conn_handle_open(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size) {
    int is_open;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif
    is_open = ecp_conn_is_open(conn);
    if (!is_open) conn->flags |= ECP_CONN_FLAG_OPEN;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    if (mtype & ECP_MTYPE_FLAG_REP) {
        if (!conn->out) return ECP_ERR;

        if (is_open && size == ECP_ERR_TIMEOUT) {
            int rv = ecp_conn_reset(conn);
            if (rv) return rv;

            return 0;
        }

        if (size < 0) return size;

        if (!is_open) {
#ifdef ECP_WITH_PTHREAD
            pthread_mutex_lock(&conn->mutex);
#endif
            conn->seq_in = seq;
            conn->seq_in_map = 1;
#ifdef ECP_WITH_PTHREAD
            pthread_mutex_unlock(&conn->mutex);
#endif
#ifdef ECP_WITH_RBUF
            if (conn->rbuf.recv) {
                int rv = ecp_rbuf_recv_start(conn, seq);
                if (rv) return rv;
            }
#endif
        }

        return 0;
    } else {
        unsigned char payload[ECP_SIZE_PLD(0, 0)];
        unsigned char ctype = 0;

        if (conn->out) return ECP_ERR;
        if (size < 0) return size;
        if (size < 1) return ECP_ERR;

        ctype = msg[0];

        ecp_pld_set_type(payload, ECP_MTYPE_OPEN_REP);
        ssize_t _rv = ecp_pld_send(conn, payload, sizeof(payload));

        return 1;
    }
    return ECP_ERR;
}

ssize_t ecp_conn_handle_kget(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size) {
    if (mtype & ECP_MTYPE_FLAG_REP) {
        ECPContext *ctx = conn->sock->ctx;
        
        if (!conn->out) return ECP_ERR;
        
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&conn->mutex);
#endif
        int conn_is_open = ecp_conn_is_open(conn);
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn->mutex);
#endif

        if ((size < 0) && !conn_is_open) {
            ecp_conn_handler_msg_t *handler = ctx->handler[conn->type] ? ctx->handler[conn->type]->msg[ECP_MTYPE_OPEN] : NULL;
            return handler ? handler(conn, seq, mtype, msg, size) : size;
        }

        if (size < 0) return size;
        if (size < ECP_ECDH_SIZE_KEY+1) return ECP_ERR;

        int rv = ecp_conn_dhkey_new_pub(conn, msg[0], msg+1);
        if (!rv && !conn_is_open) {
            ecp_conn_open_t *conn_open = ctx->handler[conn->type] ? ctx->handler[conn->type]->conn_open : NULL;
            ssize_t _rv = conn_open(conn);
            if (_rv < 0) rv = _rv;
        }
        if (rv) return rv;
        
        return ECP_ECDH_SIZE_KEY+1;
    } else {
        unsigned char payload[ECP_SIZE_PLD(ECP_ECDH_SIZE_KEY+1, 0)];
        unsigned char *buf = ecp_pld_get_buf(payload, 0);
        ecp_pld_set_type(payload, ECP_MTYPE_KGET_REP);

        if (conn->out) return ECP_ERR;
        if (size < 0) return size;

        int rv = ecp_conn_dhkey_get_curr(conn, buf, buf+1);
        if (rv) return rv;
        
        ssize_t _rv = ecp_pld_send(conn, payload, sizeof(payload));

        return 0;
    }
    return ECP_ERR;
}
 
ssize_t ecp_conn_handle_kput(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size) {
    if (size < 0) return size;

    if (mtype & ECP_MTYPE_FLAG_REP) {
        if (!conn->out) return ECP_ERR;
        return 0;
    } else {
        unsigned char payload[ECP_SIZE_PLD(0, 0)];

        if (conn->out) return ECP_ERR;
        if (size < ECP_ECDH_SIZE_KEY+1) return ECP_ERR;

        int rv = ecp_conn_dhkey_new_pub(conn, msg[0], msg+1);
        if (rv) return rv;

        ecp_pld_set_type(payload, ECP_MTYPE_KPUT_REP);
        ssize_t _rv =  ecp_pld_send(conn, payload, sizeof(payload));

        return ECP_ECDH_SIZE_KEY+1;
    }
    return ECP_ERR;
}

ssize_t ecp_conn_handle_exec(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size) {
    if (size < 0) return size;
    return ecp_pkt_handle(conn->sock, NULL, conn, msg, size);
}

static ssize_t _conn_send_kput(ECPConnection *conn, ECPTimerItem *ti) {
    unsigned char payload[ECP_SIZE_PLD(ECP_ECDH_SIZE_KEY+1, 0)];
    unsigned char *buf = ecp_pld_get_buf(payload, 0);
    int rv = ECP_OK;
    
    ecp_pld_set_type(payload, ECP_MTYPE_KPUT_REQ);
    rv = ecp_conn_dhkey_get_curr(conn, buf, buf+1);
    if (rv) return rv;

    return ecp_pld_send_wtimer(conn, ti, payload, sizeof(payload));
}

int ecp_conn_dhkey_new(ECPConnection *conn) {
    ECPSocket *sock = conn->sock;
    ECPDHKey new_key;
    int rv;
    ssize_t _rv;
    
    rv = ecp_dhkey_generate(sock->ctx, &new_key);
    if (rv) return rv;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn.mutex);
    pthread_mutex_lock(&conn->mutex);
#endif
    rv = conn_dhkey_new_pair(conn, &new_key);
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
    pthread_mutex_unlock(&sock->conn.mutex);
#endif

    if (rv) return rv;
    
    _rv = ecp_timer_send(conn, _conn_send_kput, ECP_MTYPE_KPUT_REP, 3, 500);
    if (_rv < 0) return _rv;
        
    return ECP_OK;
}

int ecp_conn_dhkey_new_pub(ECPConnection *conn, unsigned char idx, unsigned char *public) {
    ECPSocket *sock = conn->sock;
    int rv;
    
    if (conn == NULL) return ECP_ERR;
    if (public == NULL) return ECP_ERR;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn.mutex);
    pthread_mutex_lock(&conn->mutex);
#endif
    rv = conn_dhkey_new_pub_remote(conn, idx, public);
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
    pthread_mutex_unlock(&sock->conn.mutex);
#endif
    if (rv == ECP_ERR_ECDH_KEY_DUP) rv = ECP_OK;

    return rv;
}

int ecp_conn_dhkey_get_curr(ECPConnection *conn, unsigned char *idx, unsigned char *public) {
    unsigned char _idx;
    ECPSocket *sock = conn->sock;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif

    _idx = conn->key_curr;
    if (_idx != ECP_ECDH_IDX_INV) sock->ctx->cr.dh_pub_to_buf(public, &conn->key[_idx].public);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif
    
    if (_idx == ECP_ECDH_IDX_INV) return ecp_sock_dhkey_get_curr(sock, idx, public);

    if (idx) *idx = _idx;
    return ECP_OK;
}
    
ssize_t ecp_pack(ECPContext *ctx, unsigned char *packet, size_t pkt_size, unsigned char s_idx, unsigned char c_idx, ecp_dh_public_t *public, ecp_aead_key_t *shsec, unsigned char *nonce, ecp_seq_t seq, unsigned char *payload, size_t payload_size) {
    ssize_t rv;

    if (payload == NULL) return ECP_ERR;
    if (pkt_size < ECP_SIZE_PKT_HDR) return ECP_ERR;
    
    // ECP_SIZE_PROTO
    packet[0] = 0;
    packet[1] = 0;
    s_idx = s_idx & 0x0F;
    c_idx = c_idx & 0x0F;
    packet[ECP_SIZE_PROTO] = (s_idx << 4) | c_idx;
    ctx->cr.dh_pub_to_buf(packet+ECP_SIZE_PROTO+1, public);
    memcpy(packet+ECP_SIZE_PROTO+1+ECP_ECDH_SIZE_KEY, nonce, ECP_AEAD_SIZE_NONCE);

    payload[0] = (seq & 0xFF000000) >> 24;
    payload[1] = (seq & 0x00FF0000) >> 16;
    payload[2] = (seq & 0x0000FF00) >> 8;
    payload[3] = (seq & 0x000000FF);
    rv = ctx->cr.aead_enc(packet+ECP_SIZE_PKT_HDR, pkt_size-ECP_SIZE_PKT_HDR, payload, payload_size, shsec, nonce);
    if (rv < 0) return ECP_ERR_ENCRYPT;
    
    memcpy(nonce, packet+ECP_SIZE_PKT_HDR, ECP_AEAD_SIZE_NONCE);
        
    return rv+ECP_SIZE_PKT_HDR;
}

ssize_t ecp_pack_raw(ECPSocket *sock, ECPConnection *parent, unsigned char *packet, size_t pkt_size, unsigned char s_idx, unsigned char c_idx, ecp_dh_public_t *public, ecp_aead_key_t *shsec, unsigned char *nonce, ecp_seq_t seq, unsigned char *payload, size_t payload_size, ECPNetAddr *addr) {
    ECPContext *ctx = sock->ctx;
    
    return ecp_pack(ctx, packet, pkt_size, s_idx, c_idx, public, shsec, nonce, seq, payload, payload_size);
}
    
ssize_t ecp_conn_pack(ECPConnection *conn, unsigned char *packet, size_t pkt_size, unsigned char s_idx, unsigned char c_idx, unsigned char *payload, size_t payload_size, ECPSeqItem *si, ECPNetAddr *addr) {
    ecp_aead_key_t shsec;
    ecp_dh_public_t public;
    ecp_seq_t _seq;
    unsigned char nonce[ECP_AEAD_SIZE_NONCE];
    int rv = ECP_OK;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif
    if (s_idx == ECP_ECDH_IDX_INV) {
        if (conn->out) {
            if (conn->remote.key_curr != ECP_ECDH_IDX_INV) s_idx = conn->remote.key[conn->remote.key_curr].idx;
        } else {
            if (conn->key_idx_curr != ECP_ECDH_IDX_INV) s_idx = conn->key_idx[conn->key_idx_curr];
        }
    }
    if (c_idx == ECP_ECDH_IDX_INV) {
        if (conn->out) {
            if (conn->key_idx_curr != ECP_ECDH_IDX_INV) c_idx = conn->key_idx[conn->key_idx_curr];
        } else {
            if (conn->remote.key_curr != ECP_ECDH_IDX_INV) c_idx = conn->remote.key[conn->remote.key_curr].idx;
        }
    }
    rv = conn_shsec_get(conn, s_idx, c_idx, &shsec);
    if (!rv) memcpy(nonce, conn->nonce, sizeof(nonce));
    if (!rv) {
        if (conn->out) {
            ECPDHKey *key = conn_dhkey_get(conn, c_idx);

            if ((key == NULL) || !key->valid) rv = ECP_ERR_ECDH_IDX;
            if (!rv) memcpy(&public, &key->public, sizeof(public));
        } else {
            memcpy(&public, &conn->remote.key[conn->remote.key_curr].public, sizeof(public));
        }
    }
    if (!rv) {
        if (si) {
            if (si->seq_w) {
                _seq = si->seq;
            } else {
                _seq = conn->seq_out + 1;
                si->seq = _seq;
            }

#ifdef ECP_WITH_RBUF
            if (conn->rbuf.send) rv = ecp_rbuf_pkt_prep(conn->rbuf.send, si, payload);
#endif
                
            if (!rv && !si->seq_w) conn->seq_out = _seq;
        } else {
            _seq = conn->seq_out + 1;
            conn->seq_out = _seq;
        }
        if (!rv && addr) *addr = conn->node.addr;
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif
    
    if (rv) return rv;
    
    ssize_t _rv = ecp_pack(conn->sock->ctx, packet, pkt_size, s_idx, c_idx, &public, &shsec, nonce, _seq, payload, payload_size);
    if (_rv < 0) return _rv;
    
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif
    memcpy(conn->nonce, nonce, sizeof(conn->nonce));
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    return _rv;
}

ssize_t ecp_pkt_handle(ECPSocket *sock, ECPNetAddr *addr, ECPConnection *parent, unsigned char *packet, size_t pkt_size) {
    unsigned char s_idx;
    unsigned char c_idx;
    unsigned char l_idx = ECP_ECDH_IDX_INV;
    unsigned char nonce[ECP_AEAD_SIZE_NONCE];
    ecp_aead_key_t shsec;
    ecp_dh_public_t public;
    ecp_dh_private_t private;
    unsigned char payload[ECP_MAX_PLD];
    ECPConnection *conn = NULL;
    ECPDHKey *key = NULL;
    int rv = ECP_OK;
    int seq_check = 1;
    ecp_seq_t seq_c, seq_p, seq_n;
    ecp_ack_t seq_map;
    ssize_t pld_size, cnt_size, proc_size;
    
    s_idx = (packet[ECP_SIZE_PROTO] & 0xF0) >> 4;
    c_idx = (packet[ECP_SIZE_PROTO] & 0x0F);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn.mutex);
#endif

    conn = ctable_search(sock, c_idx, packet+ECP_SIZE_PROTO+1, NULL);
    if (conn && !conn->out && (s_idx == ECP_ECDH_IDX_PERMA)) conn = NULL;

#ifdef ECP_WITH_PTHREAD
    if (conn) pthread_mutex_lock(&conn->mutex);
    pthread_mutex_unlock(&sock->conn.mutex);
#endif

    if (conn) {
        rv = conn_shsec_get(conn, s_idx, c_idx, &shsec);
        if (rv == ECP_ERR_ECDH_IDX_LOCAL) {
            rv = ECP_OK;
            l_idx = conn->out ? c_idx : s_idx;
            key = conn_dhkey_get(conn, l_idx);
            if ((key == NULL) || !key->valid) rv = ECP_ERR_ECDH_IDX;
        }
    } else {
        if (s_idx == ECP_ECDH_IDX_PERMA) {
            key = &sock->key_perma;
        } else {
            l_idx = s_idx;
            if (l_idx < ECP_MAX_SOCK_KEY) key = &sock->key[l_idx];
        }
        if ((key == NULL) || !key->valid) rv = ECP_ERR_ECDH_IDX;
    }

    if (!rv && key) memcpy(&private, &key->private, sizeof(private));

    if (!rv && conn) {
        seq_check = ecp_conn_is_open(conn) ? 1 : 0;
        seq_c = conn->seq_in;
        seq_map = conn->seq_in_map;
        conn->refcount++;
    }

#ifdef ECP_WITH_PTHREAD
    if (conn) pthread_mutex_unlock(&conn->mutex);
#endif

    if (rv) return rv;

    if (key) {
        sock->ctx->cr.dh_pub_from_buf(&public, packet+ECP_SIZE_PROTO+1);
        sock->ctx->cr.dh_shsec(&shsec, &public, &private);
        memset(&private, 0, sizeof(private));
    }

    pld_size = sock->ctx->cr.aead_dec(payload, ECP_MAX_PLD, packet+ECP_SIZE_PKT_HDR, pkt_size-ECP_SIZE_PKT_HDR, &shsec, packet+ECP_SIZE_PROTO+1+ECP_ECDH_SIZE_KEY);
    if (pld_size < ECP_SIZE_PLD_HDR+1) rv = ECP_ERR_DECRYPT;
    if (rv) goto pkt_handle_err;

    seq_p = \
        (payload[0] << 24) | \
        (payload[1] << 16) | \
        (payload[2] << 8)  | \
        (payload[3]);

    memcpy(nonce, packet+ECP_SIZE_PKT_HDR, sizeof(nonce));

    if (conn == NULL) {
        if (payload[ECP_SIZE_PLD_HDR] == ECP_MTYPE_OPEN_REQ) {
            rv = sock->conn_new(sock, &conn, parent, s_idx, c_idx, packet+ECP_SIZE_PROTO+1, &shsec, payload+ECP_SIZE_PLD_HDR+1, pld_size-ECP_SIZE_PLD_HDR-1);
            if (rv) return rv;

            seq_map = 1;
            seq_n = seq_p;
        } else if (payload[ECP_SIZE_PLD_HDR] == ECP_MTYPE_KGET_REQ) {
            unsigned char payload_[ECP_SIZE_PLD(ECP_ECDH_SIZE_KEY+1, 0)];
            unsigned char *buf = ecp_pld_get_buf(payload_, 0);
            ecp_pld_set_type(payload_, ECP_MTYPE_KGET_REP);
            
            rv = ecp_sock_dhkey_get_curr(sock, buf, buf+1);
            if (!rv) {
                ssize_t _rv = ecp_pld_send_raw(sock, parent, addr, s_idx, c_idx, &public, &shsec, nonce, seq_p, payload_, sizeof(payload_));
                if (_rv < 0) rv = _rv;
            }
            return ECP_MIN_PKT;
        } else {
            return ECP_ERR_CONN_NOT_FOUND;
        }
    } else {
#ifdef ECP_WITH_RBUF
        if (conn->rbuf.recv || (payload[ECP_SIZE_PLD_HDR] == ECP_MTYPE_RBACK) || (payload[ECP_SIZE_PLD_HDR] == ECP_MTYPE_RBFLUSH)) seq_check = 0;
#endif
        
        if (seq_check) {
            if (ECP_SEQ_LTE(seq_p, seq_c)) {
                ecp_seq_t seq_offset = seq_c - seq_p;
                if (seq_offset < ECP_SIZE_ACKB) {
                    ecp_ack_t ack_mask = ((ecp_ack_t)1 << seq_offset);
                    if (ack_mask & seq_map) rv = ECP_ERR_SEQ;
                    if (!rv) seq_n = seq_c;
                } else {
                    rv = ECP_ERR_SEQ;
                }
            } else {
                ecp_seq_t seq_offset = seq_p - seq_c;
                if (seq_offset < ECP_MAX_SEQ_FORWARD) {
                    if (seq_offset < ECP_SIZE_ACKB) {
                        seq_map = seq_map << seq_offset;
                    } else {
                        seq_map = 0;
                    }
                    seq_map |= 1;
                    seq_n = seq_p;
                } else {
                    rv = ECP_ERR_SEQ;
                }
            }

            if (rv) goto pkt_handle_err;
        }

        if (key) {
#ifdef ECP_WITH_PTHREAD
            pthread_mutex_lock(&conn->mutex);
#endif
            rv = conn_dhkey_new_pub_local(conn, l_idx);
            if (!rv) rv = conn_shsec_set(conn, s_idx, c_idx, &shsec);
#ifdef ECP_WITH_PTHREAD
            pthread_mutex_unlock(&conn->mutex);
#endif
            if (rv) goto pkt_handle_err;
        }
    }

    if (conn == NULL) return ECP_ERR;
    
    proc_size = 0;
    cnt_size = pld_size-ECP_SIZE_PLD_HDR;
    
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif
    memcpy(conn->nonce, nonce, sizeof(conn->nonce));
    if (addr) conn->node.addr = *addr;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

#ifdef ECP_WITH_RBUF
    if (conn->rbuf.recv) proc_size = ecp_rbuf_recv_store(conn, seq_p, payload+pld_size-cnt_size, cnt_size);
#endif
    if (proc_size == 0) proc_size = ecp_msg_handle(conn, seq_p, payload+pld_size-cnt_size, cnt_size);

    if (proc_size < 0) rv = ECP_ERR_HANDLE;
    if (!rv) cnt_size -= proc_size;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif
    if (!rv && seq_check) {
        conn->seq_in = seq_n;
        conn->seq_in_map = seq_map;
    }
    conn->refcount--;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    if (rv) return rv;
    return ECP_SIZE_PKT_HDR+ECP_AEAD_SIZE_TAG+pld_size-cnt_size;
    
pkt_handle_err:
    if (conn == NULL) return rv;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif
    conn->refcount--;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    return rv;
}

ssize_t ecp_pkt_send(ECPSocket *sock, ECPNetAddr *addr, unsigned char *packet, size_t pkt_size) {
    ssize_t rv = sock->ctx->tr.send(&sock->sock, packet, pkt_size, addr);
    if (rv < 0) return rv;
    if (rv < ECP_MIN_PKT) return ECP_ERR_SEND;

    return rv;
}

ssize_t ecp_pkt_recv(ECPSocket *sock, ECPNetAddr *addr, unsigned char *packet, size_t pkt_size) {
    ssize_t rv = sock->ctx->tr.recv(&sock->sock, packet, pkt_size, addr);
    if (rv < 0) return rv;
    if (rv < ECP_MIN_PKT) return ECP_ERR_RECV;

    return rv;
}

ssize_t ecp_msg_handle(ECPConnection *conn, ecp_seq_t seq, unsigned char *msg, size_t msg_size) {
    ecp_conn_handler_msg_t *handler = NULL;
    ssize_t rv = 0;
    unsigned char mtype = 0;
    unsigned char *content = NULL;
    size_t rem_size = msg_size;
    
    if (msg_size < 1) return ECP_ERR_MIN_MSG;
    
    while (rem_size) {
        mtype = ecp_msg_get_type(msg);
        content = ecp_msg_get_content(msg, rem_size);
        if (content == NULL) return ECP_ERR_MIN_MSG;
        
        rem_size -= content - msg;
            
        if ((mtype & ECP_MTYPE_MASK) >= ECP_MAX_MTYPE) return ECP_ERR_MAX_MTYPE;

        ecp_timer_pop(conn, mtype);
        handler = conn->sock->ctx->handler[conn->type] ? conn->sock->ctx->handler[conn->type]->msg[mtype & ECP_MTYPE_MASK] : NULL;
        if (handler) {
            rv = handler(conn, seq, mtype, content, rem_size);
            if (rv < 0) return rv;
            if (rv > rem_size) return ECP_ERR;

            rem_size -= rv;
            msg = content + rv;
        } else {
            return msg_size-rem_size-1;
        }
    }

    return msg_size;
}

int ecp_seq_item_init(ECPSeqItem *seq_item) {
    memset(seq_item, 0, sizeof(ECPSeqItem));

    return ECP_OK;
}

void ecp_pld_set_type(unsigned char *payload, unsigned char mtype) {
    payload[ECP_SIZE_PLD_HDR] = mtype;
}

unsigned char ecp_pld_get_type(unsigned char *payload) {
    return payload[ECP_SIZE_PLD_HDR];
}

unsigned char *ecp_pld_get_buf(unsigned char *payload, unsigned char mtype) {
    size_t offset = 0;
    if (mtype & ECP_MTYPE_FLAG_FRAG) offset += 2;
    if (mtype & ECP_MTYPE_FLAG_PTS) offset += sizeof(ecp_pts_t);

    return payload + ECP_SIZE_PLD_HDR + 1 + offset;
}

unsigned char ecp_msg_get_type(unsigned char *msg) {
    return msg[0];
}

unsigned char *ecp_msg_get_content(unsigned char *msg, size_t msg_size) {
    size_t offset = 0;
    unsigned char mtype = msg[0];
    
    if (mtype & ECP_MTYPE_FLAG_FRAG) offset += 2;
    if (mtype & ECP_MTYPE_FLAG_PTS) offset += sizeof(ecp_pts_t);

    if (msg_size < 1 + offset) return NULL; 
    return msg + 1 + offset;
}

int ecp_msg_get_frag(unsigned char *msg, size_t msg_size, unsigned char *frag_cnt, unsigned char *frag_tot) {
    unsigned char mtype = msg[0];

    if (!(mtype & ECP_MTYPE_FLAG_FRAG)) return ECP_ERR;
    if (msg_size < 3) return ECP_ERR;
    
    *frag_cnt = msg[1];
    *frag_tot = msg[2];

    return ECP_OK;
}

int ecp_msg_get_pts(unsigned char *msg, size_t msg_size, ecp_pts_t *pts) {
    size_t offset = 0;
    unsigned char mtype = msg[0];

    if (!(mtype & ECP_MTYPE_FLAG_PTS)) return ECP_ERR;

    if (mtype & ECP_MTYPE_FLAG_FRAG) offset += 2;
    if (msg_size < 1 + offset + sizeof(ecp_pts_t)) return ECP_ERR;

    *pts = \
        (msg[1 + offset] << 24) | \
        (msg[2 + offset] << 16) | \
        (msg[3 + offset] << 8)  | \
        (msg[4 + offset]);

    return ECP_OK;
}

static ssize_t pld_send(ECPConnection *conn, ECPTimerItem *ti, unsigned char s_idx, unsigned char c_idx, unsigned char *payload, size_t payload_size, ECPSeqItem *si) {
    unsigned char packet[ECP_MAX_PKT];
    ECPSocket *sock = conn->sock;
    ECPContext *ctx = sock->ctx;
    ECPNetAddr addr;

    ssize_t rv = ctx->pack(conn, packet, ECP_MAX_PKT, s_idx, c_idx, payload, payload_size, si, &addr);
    if (rv < 0) return rv;

#ifdef ECP_WITH_RBUF
    if (conn->rbuf.send) return ecp_rbuf_pkt_send(conn->rbuf.send, conn->sock, &addr, ti, si, packet, rv);
#endif
    
    if (ti) {
        int _rv = ecp_timer_push(ti);
        if (_rv) return _rv;
    }
    return ecp_pkt_send(sock, &addr, packet, rv);
}

ssize_t ecp_pld_send(ECPConnection *conn, unsigned char *payload, size_t payload_size) {
    return ecp_pld_send_ll(conn, NULL, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, payload, payload_size);
}

ssize_t ecp_pld_send_wtimer(ECPConnection *conn, ECPTimerItem *ti, unsigned char *payload, size_t payload_size) {
    return ecp_pld_send_ll(conn, ti, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, payload, payload_size);
}

ssize_t ecp_pld_send_ll(ECPConnection *conn, ECPTimerItem *ti, unsigned char s_idx, unsigned char c_idx, unsigned char *payload, size_t payload_size) {
    ECPSeqItem *_seq_item = NULL;

#ifdef ECP_WITH_RBUF
    ECPSeqItem seq_item;

    if (conn->rbuf.send) {
        int _rv = ecp_seq_item_init(&seq_item);
        if (_rv) return _rv;
        _seq_item = &seq_item;
    }
#endif

    return pld_send(conn, ti, s_idx, c_idx, payload, payload_size, _seq_item);
}

ssize_t ecp_pld_send_raw(ECPSocket *sock, ECPConnection *parent, ECPNetAddr *addr, unsigned char s_idx, unsigned char c_idx, ecp_dh_public_t *public, ecp_aead_key_t *shsec, unsigned char *nonce, ecp_seq_t seq, unsigned char *payload, size_t payload_size) {
    unsigned char packet[ECP_MAX_PKT];
    ECPContext *ctx = sock->ctx;
    ECPNetAddr _addr;
    ssize_t rv;

    rv = ctx->pack_raw(sock, parent, packet, ECP_MAX_PKT, s_idx, c_idx, public, shsec, nonce, seq, payload, payload_size, &_addr);
    if (rv < 0) return rv;
    
    return ecp_pkt_send(sock, parent ? &_addr : addr, packet, rv);
}

ssize_t ecp_send(ECPConnection *conn, unsigned char *payload, size_t payload_size) {
    return ecp_pld_send(conn, payload, payload_size);
}

ssize_t ecp_receive(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, ecp_cts_t timeout) {
#ifdef ECP_WITH_RBUF
#ifdef ECP_WITH_MSGQ
    pthread_mutex_lock(&conn->rbuf.recv->msgq.mutex);
    ssize_t rv = ecp_conn_msgq_pop(conn, mtype, msg, msg_size, timeout);
    pthread_mutex_unlock(&conn->rbuf.recv->msgq.mutex);
    return rv;
#else
    return ECP_ERR_NOT_IMPLEMENTED;
#endif
#else
    return ECP_ERR_NOT_IMPLEMENTED;
#endif
}

static int recv_p(ECPSocket *sock) {
    ECPNetAddr addr;
    unsigned char packet[ECP_MAX_PKT];
    
    ssize_t rv = ecp_pkt_recv(sock, &addr, packet, ECP_MAX_PKT);
    if (rv < 0) return rv;
    
    rv = ecp_pkt_handle(sock, &addr, NULL, packet, rv);
    if (rv < 0) return rv;
    
    return ECP_OK;
}

int ecp_receiver(ECPSocket *sock) {
    ecp_cts_t next = 0;
    sock->running = 1;
    while(sock->running) {
        int rv = sock->ctx->tr.poll(&sock->sock, next ? next : sock->poll_timeout);
        if (rv == 1) {
            rv = recv_p(sock);
            DPRINT(rv, "ERR:recv_p - RV:%d\n", rv);
        }
        next = ecp_timer_exe(sock);
    }
    return ECP_OK;
}

int ecp_start_receiver(ECPSocket *sock) {
#ifdef ECP_WITH_PTHREAD
    int rv = pthread_create(&sock->rcvr_thd, NULL, (void *(*)(void *))ecp_receiver, sock);
    if (rv) return ECP_ERR;
    return ECP_OK;
#else
    return ECP_ERR_NOT_IMPLEMENTED;
#endif
}

int ecp_stop_receiver(ECPSocket *sock) {
    sock->running = 0;
#ifdef ECP_WITH_PTHREAD
    int rv = pthread_join(sock->rcvr_thd, NULL);
    if (rv) return ECP_ERR;
    return ECP_OK;
#else
    return ECP_ERR_NOT_IMPLEMENTED;
#endif
}
