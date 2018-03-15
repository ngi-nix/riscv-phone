#include "core.h"

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

    if (public) memcpy(&node->public, public, sizeof(node->public));

    if (addr && ctx->tr.addr_set) rv = ctx->tr.addr_set(&node->addr, addr);
    if (rv) return ECP_ERR_NET_ADDR;

    return ECP_OK;
}

int ecp_ctx_create(ECPContext *ctx) {
    int rv;
    
    if (ctx == NULL) return ECP_ERR;

    memset(ctx, 0, sizeof(ECPContext));

    ctx->pack = _ecp_pack;
    ctx->pack_raw = _ecp_pack_raw;

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
    conn->htable = ctx->ht.create(ctx);
    if (conn->htable == NULL) return ECP_ERR_ALLOC;
#endif

#ifdef ECP_WITH_PTHREAD
    rv = pthread_mutex_init(&conn->mutex, NULL);
    if (rv) {
#ifdef ECP_WITH_HTABLE
        ctx->ht.destroy(conn->htable);
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
    ctx->ht.destroy(conn->htable);
#endif
}

static int ctable_insert(ECPConnection *conn) {
    int with_htable = 0;
    ECPSocket *sock = conn->sock;
    ECPContext *ctx = sock->ctx;

#ifdef ECP_WITH_HTABLE
    with_htable = 1;
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
#ifndef ECP_WITH_HTABLE
        if (sock->conn.size == ECP_MAX_SOCK_CONN) return ECP_ERR_MAX_SOCK_CONN;
        sock->conn.array[sock->conn.size] = conn;
        sock->conn.size++;
#endif
    }

    return ECP_OK;
}

static void ctable_remove(ECPConnection *conn) {
    int i, with_htable = 0;
    ECPSocket *sock = conn->sock;
    ECPContext *ctx = sock->ctx;
    
#ifdef ECP_WITH_HTABLE
    with_htable = 1;
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
#ifndef ECP_WITH_HTABLE
        for (i=0; i<sock->conn.size; i++) {
            if (conn == sock->conn.array[i]) {
                sock->conn.array[i] = sock->conn.array[sock->conn.size-1];
                sock->conn.array[sock->conn.size-1] = NULL;
                sock->conn.size--;
                return;
            }
        }
#endif
    }
}

static ECPConnection *ctable_search(ECPSocket *sock, unsigned char c_idx, unsigned char *c_public, ECPNetAddr *addr) {
    int i, with_htable = 0;
    ECPConnection *conn = NULL;
    
#ifdef ECP_WITH_HTABLE
    with_htable = 1;
#endif

    if (with_htable) {
#ifdef ECP_WITH_HTABLE
        return sock->ctx->ht.search(sock->conn.htable, c_public);
#endif
    } else {
#ifndef ECP_WITH_HTABLE
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
#endif
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
    if (conn->out && ecp_conn_is_reg(conn) && conn->key[conn->key_curr].valid) {
        ctx->ht.remove(sock->conn.htable, ctx->cr.dh_pub_get_buf(&conn->key[conn->key_curr].public));
    }
#endif
    
    conn->key[conn->key_curr] = *key;
    conn->key_idx_map[conn->key_curr] = ECP_ECDH_IDX_INV;

#ifdef ECP_WITH_HTABLE
    if (conn->out && ecp_conn_is_reg(conn)) {
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
    if (conn->out && ecp_conn_is_reg(conn) && conn->key[idx].valid) {
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
    if (!conn->out && ecp_conn_is_reg(conn) && (remote->key[new].idx != ECP_ECDH_IDX_INV)) {
        ctx->ht.remove(sock->conn.htable, ctx->cr.dh_pub_get_buf(&remote->key[new].public));
    }
#endif
        
    if (remote->key[new].idx != ECP_ECDH_IDX_INV) remote->key_idx_map[remote->key[new].idx] = ECP_ECDH_IDX_INV;
    remote->key_idx_map[idx] = new;
    ctx->cr.dh_pub_from_buf(&remote->key[new].public, public);
    remote->key[new].idx = idx;
    remote->key_curr = new;

#ifdef ECP_WITH_HTABLE
    if (!conn->out && ecp_conn_is_reg(conn)) {
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
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(0, ECP_MTYPE_KGET_REQ, conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(0, ECP_MTYPE_KGET_REQ, conn)];
    packet.buffer = pkt_buf;
    packet.size = ECP_SIZE_PKT_BUF(0, ECP_MTYPE_KGET_REQ, conn);
    payload.buffer = pld_buf;
    payload.size = ECP_SIZE_PLD_BUF(0, ECP_MTYPE_KGET_REQ, conn);

    ecp_pld_set_type(pld_buf, ECP_MTYPE_KGET_REQ);
    return ecp_pld_send_ll(conn, &packet, ECP_ECDH_IDX_PERMA, ECP_ECDH_IDX_INV, &payload, ECP_SIZE_PLD(0, ECP_MTYPE_KGET_REQ), 0, ti);
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
    if (!rv) rv = ctx->rng(&conn->seq_out, sizeof(conn->seq_out));
    
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
    
    if (timeout && refcount && sock->ctx->tm.sleep_ms) {
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
    if (!rv) rv = ctx->rng(&conn->seq_out, sizeof(conn->seq_out));
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
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(1, ECP_MTYPE_OPEN_REQ, conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(1, ECP_MTYPE_OPEN_REQ, conn)];
    unsigned char *buf = ecp_pld_get_buf(pld_buf, ECP_MTYPE_OPEN_REQ);
    
    packet.buffer = pkt_buf;
    packet.size = ECP_SIZE_PKT_BUF(1, ECP_MTYPE_OPEN_REQ, conn);
    payload.buffer = pld_buf;
    payload.size = ECP_SIZE_PLD_BUF(1, ECP_MTYPE_OPEN_REQ, conn);

    ecp_pld_set_type(pld_buf, ECP_MTYPE_OPEN_REQ);
    buf[0] = conn->type;
    
    return ecp_pld_send_wtimer(conn, &packet, &payload, ECP_SIZE_PLD(1, ECP_MTYPE_OPEN_REQ), 0, ti);
}

ssize_t ecp_conn_send_open(ECPConnection *conn) {
    return ecp_timer_send(conn, _conn_send_open, ECP_MTYPE_OPEN_REP, 1, 500);
}

int ecp_conn_handle_new(ECPSocket *sock, ECPConnection *parent, unsigned char *payload, size_t payload_size, ECPConnection **_conn) {
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
    if (!rv) rv = sock->ctx->rng(&conn->seq_out, sizeof(conn->seq_out));
    if (rv) {
        if (sock->ctx->conn_free) sock->ctx->conn_free(conn);
        return rv;
    }
    
    conn->refcount = 1;
    conn->parent = parent;
    conn->pcount = parent ? parent->pcount+1 : 0;
    handle_create = conn->sock->ctx->handler[conn->type] ? conn->sock->ctx->handler[conn->type]->conn_create : NULL;
    handle_destroy = conn->sock->ctx->handler[conn->type] ? conn->sock->ctx->handler[conn->type]->conn_destroy : NULL;
    
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

ssize_t ecp_conn_handle_open(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif
    int is_open = ecp_conn_is_open(conn);
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
        return 0;
    } else {
        if (conn->out) return ECP_ERR;
        if (size < 0) return size;
        if (size < 1) return ECP_ERR;

        ECPBuffer packet;
        ECPBuffer payload;
        unsigned char pkt_buf[ECP_SIZE_PKT_BUF(0, ECP_MTYPE_OPEN_REP, conn)];
        unsigned char pld_buf[ECP_SIZE_PLD_BUF(0, ECP_MTYPE_OPEN_REP, conn)];
        unsigned char ctype = 0;

        packet.buffer = pkt_buf;
        packet.size = ECP_SIZE_PKT_BUF(0, ECP_MTYPE_OPEN_REP, conn);
        payload.buffer = pld_buf;
        payload.size = ECP_SIZE_PLD_BUF(0, ECP_MTYPE_OPEN_REP, conn);

        ctype = msg[0];
        ecp_pld_set_type(pld_buf, ECP_MTYPE_OPEN_REP);
        ssize_t _rv = ecp_pld_send(conn, &packet, &payload, ECP_SIZE_PLD(0, ECP_MTYPE_OPEN_REP), 0);
        
        return 1;
    }
    return ECP_ERR;
}

ssize_t ecp_conn_handle_kget(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    if (mtype & ECP_MTYPE_FLAG_REP) {
        ECPContext *ctx = conn->sock->ctx;
        
        if (!conn->out) return ECP_ERR;
        
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&conn->mutex);
#endif
        int is_open = ecp_conn_is_open(conn);
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn->mutex);
#endif

        if ((size < 0) && !is_open) {
            ecp_conn_handler_msg_t *handler = ctx->handler[conn->type] ? ctx->handler[conn->type]->msg[ECP_MTYPE_OPEN] : NULL;
            return handler ? handler(conn, seq, mtype, msg, size, b) : size;
        }

        if (size < 0) return size;
        if (size < ECP_ECDH_SIZE_KEY+1) return ECP_ERR;

        int rv = ecp_conn_dhkey_new_pub(conn, msg[0], msg+1);
        if (!rv && !is_open) {
            ecp_conn_open_t *conn_open = ctx->handler[conn->type] ? ctx->handler[conn->type]->conn_open : NULL;
            if (conn_open) {
                ssize_t _rv = conn_open(conn);
                if (_rv < 0) rv = _rv;
            }
        }
        if (rv) return rv;
        
        return ECP_ECDH_SIZE_KEY+1;
    } else {
        if (conn->out) return ECP_ERR;
        if (size < 0) return size;

        ECPBuffer packet;
        ECPBuffer payload;
        unsigned char pkt_buf[ECP_SIZE_PKT_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP, conn)];
        unsigned char pld_buf[ECP_SIZE_PLD_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP, conn)];
        unsigned char *buf = ecp_pld_get_buf(pld_buf, 0);

        packet.buffer = pkt_buf;
        packet.size = ECP_SIZE_PKT_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP, conn);
        payload.buffer = pld_buf;
        payload.size = ECP_SIZE_PLD_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP, conn);

        ecp_pld_set_type(pld_buf, ECP_MTYPE_KGET_REP);
        int rv = ecp_conn_dhkey_get_curr(conn, buf, buf+1);
        if (rv) return rv;
        ssize_t _rv = ecp_pld_send(conn, &packet, &payload, ECP_SIZE_PLD(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP), 0);
        
        return 0;
    }
    return ECP_ERR;
}

ssize_t ecp_conn_handle_kput(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    if (size < 0) return size;

    if (mtype & ECP_MTYPE_FLAG_REP) {
        if (!conn->out) return ECP_ERR;
        return 0;
    } else {
        if (conn->out) return ECP_ERR;
        if (size < ECP_ECDH_SIZE_KEY+1) return ECP_ERR;

        ECPBuffer packet;
        ECPBuffer payload;
        unsigned char pkt_buf[ECP_SIZE_PKT_BUF(0, ECP_MTYPE_KPUT_REP, conn)];
        unsigned char pld_buf[ECP_SIZE_PLD_BUF(0, ECP_MTYPE_KPUT_REP, conn)];

        packet.buffer = pkt_buf;
        packet.size = ECP_SIZE_PKT_BUF(0, ECP_MTYPE_KPUT_REP, conn);
        payload.buffer = pld_buf;
        payload.size = ECP_SIZE_PLD_BUF(0, ECP_MTYPE_KPUT_REP, conn);

        int rv = ecp_conn_dhkey_new_pub(conn, msg[0], msg+1);
        if (rv) return rv;

        ecp_pld_set_type(pld_buf, ECP_MTYPE_KPUT_REP);
        ssize_t _rv = ecp_pld_send(conn, &packet, &payload, ECP_SIZE_PLD(0, ECP_MTYPE_KPUT_REP), 0);

        return ECP_ECDH_SIZE_KEY+1;
    }
    return ECP_ERR;
}

/* Memory limited version: */
ssize_t ecp_conn_handle_exec(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    if (size < 0) return size;
    if (b == NULL) return ECP_ERR;
    if (b->packet->buffer == NULL) return ECP_ERR;
    
    memcpy(b->packet->buffer, msg, size);
    return ecp_pkt_handle(conn->sock, NULL, conn, b, size);
}

/* Non memory limited version: */
/*
ssize_t ecp_conn_handle_exec(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    if (size < 0) return size;
    if (b == NULL) return ECP_ERR;
    
    ECP2Buffer b2;
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pld_buf[ECP_MAX_PLD];

    b2.packet = &packet;
    b2.payload = &payload;

    packet.buffer = msg;
    packet.size = b->payload->size - (msg - b->payload->buffer);
    payload.buffer = pld_buf;
    payload.size = ECP_MAX_PLD;
    
    return ecp_pkt_handle(conn->sock, NULL, conn, &b2, size);
}
*/

static ssize_t _conn_send_kput(ECPConnection *conn, ECPTimerItem *ti) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KPUT_REQ, conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KPUT_REQ, conn)];
    unsigned char *buf = ecp_pld_get_buf(pld_buf, ECP_MTYPE_KPUT_REQ);
    int rv = ECP_OK;

    packet.buffer = pkt_buf;
    packet.size = ECP_SIZE_PKT_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KPUT_REQ, conn);
    payload.buffer = pld_buf;
    payload.size = ECP_SIZE_PLD_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KPUT_REQ, conn);

    ecp_pld_set_type(pld_buf, ECP_MTYPE_KPUT_REQ);
    rv = ecp_conn_dhkey_get_curr(conn, buf, buf+1);
    if (rv) return rv;

    return ecp_pld_send_wtimer(conn, &packet, &payload, ECP_SIZE_PLD(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KPUT_REQ), 0, ti);
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

ssize_t ecp_pack(ECPContext *ctx, unsigned char *packet, size_t pkt_size, unsigned char s_idx, unsigned char c_idx, ecp_dh_public_t *public, ecp_aead_key_t *shsec, unsigned char *nonce, ecp_seq_t seq, unsigned char *payload, size_t pld_size) {
    ssize_t rv;

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
    rv = ctx->cr.aead_enc(packet+ECP_SIZE_PKT_HDR, pkt_size-ECP_SIZE_PKT_HDR, payload, pld_size, shsec, nonce);
    if (rv < 0) return ECP_ERR_ENCRYPT;

    memcpy(nonce, packet+ECP_SIZE_PKT_HDR, ECP_AEAD_SIZE_NONCE);

    return rv+ECP_SIZE_PKT_HDR;
}

ssize_t ecp_pack_raw(ECPSocket *sock, ECPConnection *parent, unsigned char *packet, size_t pkt_size, unsigned char s_idx, unsigned char c_idx, ecp_dh_public_t *public, ecp_aead_key_t *shsec, unsigned char *nonce, ecp_seq_t seq, unsigned char *payload, size_t pld_size, ECPNetAddr *addr) {
    ECPContext *ctx = sock->ctx;
    
    return ecp_pack(ctx, packet, pkt_size, s_idx, c_idx, public, shsec, nonce, seq, payload, pld_size);
}
    
ssize_t ecp_conn_pack(ECPConnection *conn, unsigned char *packet, size_t pkt_size, unsigned char s_idx, unsigned char c_idx, unsigned char *payload, size_t pld_size, ECPSeqItem *si, ECPNetAddr *addr) {
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
            if (conn->rbuf.send) rv = ecp_rbuf_pkt_prep(conn->rbuf.send, si, ecp_pld_get_type(payload));
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
    
    ssize_t _rv = ecp_pack(conn->sock->ctx, packet, pkt_size, s_idx, c_idx, &public, &shsec, nonce, _seq, payload, pld_size);
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

ssize_t _ecp_pack(ECPConnection *conn, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, ECPBuffer *payload, size_t pld_size, ECPSeqItem *si, ECPNetAddr *addr) {
    if ((packet == NULL) || (packet->buffer == NULL)) return ECP_ERR;
    if ((payload == NULL) || (payload->buffer == NULL)) return ECP_ERR;

    return ecp_conn_pack(conn, packet->buffer, packet->size, s_idx, c_idx, payload->buffer, pld_size, si, addr);
}

ssize_t _ecp_pack_raw(ECPSocket *sock, ECPConnection *parent, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, ecp_dh_public_t *public, ecp_aead_key_t *shsec, unsigned char *nonce, ecp_seq_t seq, ECPBuffer *payload, size_t pld_size, ECPNetAddr *addr) {
    if ((packet == NULL) || (packet->buffer == NULL)) return ECP_ERR;
    if ((payload == NULL) || (payload->buffer == NULL)) return ECP_ERR;

    return ecp_pack_raw(sock, parent, packet->buffer, packet->size, s_idx, c_idx, public, shsec, nonce, seq, payload->buffer, pld_size, addr);
}


ssize_t ecp_unpack(ECPSocket *sock, ECPNetAddr *addr, ECPConnection *parent, ECP2Buffer *bufs, size_t pkt_size, ECPConnection **_conn, ecp_seq_t *_seq) {
    unsigned char s_idx;
    unsigned char c_idx;
    unsigned char l_idx = ECP_ECDH_IDX_INV;
    unsigned char *payload = bufs->payload->buffer;
    unsigned char *packet = bufs->packet->buffer;
    ecp_aead_key_t shsec;
    ecp_dh_public_t public;
    ecp_dh_private_t private;
    ECPContext *ctx = sock->ctx;
    ECPConnection *conn = NULL;
    ECPDHKey *key = NULL;
    int rv = ECP_OK;
    unsigned char is_open = 0;
    unsigned char is_new = 0;
    unsigned char seq_check = 1;
    unsigned char seq_reset = 0;
    ecp_seq_t seq_c, seq_p, seq_n;
    ecp_ack_t seq_map;
    ssize_t dec_size;
    
    *_conn = NULL;
    *_seq = 0;

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
        conn->refcount++;
        is_open = ecp_conn_is_open(conn);
        if (is_open) {
            seq_c = conn->seq_in;
            seq_map = conn->seq_in_map;
        }
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

    dec_size = sock->ctx->cr.aead_dec(payload, bufs->payload->size, packet+ECP_SIZE_PKT_HDR, pkt_size-ECP_SIZE_PKT_HDR, &shsec, packet+ECP_SIZE_PROTO+1+ECP_ECDH_SIZE_KEY);
    if (dec_size < ECP_SIZE_PLD_HDR+1) rv = ECP_ERR_DECRYPT;
    if (rv) goto ecp_unpack_err;

    seq_p = \
        (payload[0] << 24) | \
        (payload[1] << 16) | \
        (payload[2] << 8)  | \
        (payload[3]);

    if (ctx->tr.buf_free && ((payload[ECP_SIZE_PLD_HDR] & ECP_MTYPE_MASK) < ECP_MAX_MTYPE_SYS)) ctx->tr.buf_free(bufs, ECP_SEND_FLAG_MORE);
    if (conn == NULL) {
        if (payload[ECP_SIZE_PLD_HDR] == ECP_MTYPE_OPEN_REQ) {
            is_new = 1;
            rv = sock->conn_new(sock, parent, payload+ECP_SIZE_PLD_HDR+1, dec_size-ECP_SIZE_PLD_HDR-1, &conn);
            if (rv) return rv;
        } else if (payload[ECP_SIZE_PLD_HDR] == ECP_MTYPE_KGET_REQ) {
            ECPBuffer _packet;
            ECPBuffer _payload;
            unsigned char pkt_buf[ECP_SIZE_PKT_RAW_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP, parent)];
            unsigned char pld_buf[ECP_SIZE_PLD_RAW_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP, parent)];

            unsigned char *buf = ecp_pld_get_buf(pld_buf, ECP_MTYPE_KGET_REP);
            ecp_pld_set_type(pld_buf, ECP_MTYPE_KGET_REP);

            _packet.buffer = pkt_buf;
            _packet.size = ECP_SIZE_PKT_RAW_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP, parent);
            _payload.buffer = pld_buf;
            _payload.size = ECP_SIZE_PLD_RAW_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP, parent);

            rv = ecp_sock_dhkey_get_curr(sock, buf, buf+1);
            if (!rv) {
                ssize_t _rv = ecp_pld_send_raw(sock, parent, addr, &_packet, s_idx, c_idx, &public, &shsec, packet+ECP_SIZE_PKT_HDR, seq_p, &_payload, ECP_SIZE_PLD(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP), 0);
                if (_rv < 0) rv = _rv;
            }
            if (rv) return rv;
        } else {
            return ECP_ERR_CONN_NOT_FOUND;
        }
    } 
    
    if (conn == NULL) return dec_size;
    
    if (is_open) {
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
                if (seq_offset < ECP_MAX_SEQ_FWD) {
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

            if ((rv == ECP_ERR_SEQ) && (payload[ECP_SIZE_PLD_HDR] == ECP_MTYPE_OPEN_REQ) && key) {
                rv = ECP_OK;
                seq_reset = 1;
            }
            if (rv) goto ecp_unpack_err;
        }
    }

    if (!is_open || seq_reset) {
        seq_n = seq_p;
        seq_map = 1;

#ifdef ECP_WITH_RBUF
        if (conn->rbuf.recv) {
            rv = ecp_rbuf_recv_start(conn, seq_p);
            if (rv) goto ecp_unpack_err;
        }
#endif
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif
    memcpy(conn->nonce, packet+ECP_SIZE_PKT_HDR, sizeof(conn->nonce));
    if (addr) conn->node.addr = *addr;
    if (seq_check) {
        conn->seq_in = seq_n;
        conn->seq_in_map = seq_map;
    }
    if (key) {
        if (is_new) rv = conn_dhkey_new_pub_remote(conn, c_idx, packet+ECP_SIZE_PROTO+1);
        if (!rv) rv = conn_dhkey_new_pub_local(conn, l_idx);
        if (!rv) rv = conn_shsec_set(conn, s_idx, c_idx, &shsec);
    }
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    if (rv) goto ecp_unpack_err;

    *_conn = conn;
    *_seq = seq_p;
    return dec_size;

ecp_unpack_err:
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

ssize_t ecp_pkt_handle(ECPSocket *sock, ECPNetAddr *addr, ECPConnection *parent, ECP2Buffer *bufs, size_t pkt_size) {
    ECPConnection *conn;
    ecp_seq_t seq;
    ssize_t rv = 0;
    ssize_t pld_size = ecp_unpack(sock, addr, parent, bufs, pkt_size, &conn, &seq);

    if (pld_size < 0) return pld_size;
    if (conn) {
        rv = 0;
#ifdef ECP_WITH_RBUF
        if (conn->rbuf.recv) rv = ecp_rbuf_recv_store(conn, seq, bufs->payload->buffer+ECP_SIZE_PLD_HDR, pld_size-ECP_SIZE_PLD_HDR, bufs);
#endif
        if (rv == 0) rv = ecp_msg_handle(conn, seq, bufs->payload->buffer+ECP_SIZE_PLD_HDR, pld_size-ECP_SIZE_PLD_HDR, bufs);

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&conn->mutex);
#endif
        conn->refcount--;
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn->mutex);
#endif
    } else {
        rv = pld_size-ECP_SIZE_PLD_HDR;
    }

    if (rv < 0) return rv;
    return ECP_SIZE_PKT_HDR+ECP_AEAD_SIZE_TAG+ECP_SIZE_PLD_HDR+rv;
}

ssize_t ecp_pkt_send(ECPSocket *sock, ECPNetAddr *addr, ECPBuffer *packet, size_t pkt_size, unsigned char flags) {
    ssize_t rv = sock->ctx->tr.send(&sock->sock, packet, pkt_size, addr, flags);
    if (rv < 0) return rv;
    if (rv < ECP_MIN_PKT) return ECP_ERR_SEND;

    if (flags & ECP_SEND_FLAG_REPLY) {
        packet->buffer = NULL;
        packet->size = 0;
    }
    return rv;
}

int ecp_seq_item_init(ECPSeqItem *seq_item) {
    memset(seq_item, 0, sizeof(ECPSeqItem));

    return ECP_OK;
}

int ecp_frag_iter_init(ECPFragIter *iter, unsigned char *buffer, size_t buf_size) {
    memset(iter, 0, sizeof(ECPFragIter));
    iter->buffer = buffer;
    iter->buf_size = buf_size;

    return ECP_OK;
}

unsigned char ecp_msg_get_type(unsigned char *msg) {
    return msg[0];
}

unsigned char *ecp_msg_get_content(unsigned char *msg, size_t msg_size) {
    size_t offset = 1 + ECP_SIZE_MT_FLAG(msg[0]);
    
    if (msg_size < offset) return NULL;
    return msg + offset;
}

int ecp_msg_get_frag(unsigned char *msg, size_t msg_size, unsigned char *frag_cnt, unsigned char *frag_tot) {
    if (!(msg[0] & ECP_MTYPE_FLAG_FRAG)) return ECP_ERR;
    if (msg_size < 3) return ECP_ERR;
    
    *frag_cnt = msg[1];
    *frag_tot = msg[2];

    return ECP_OK;
}

int ecp_msg_get_pts(unsigned char *msg, size_t msg_size, ecp_pts_t *pts) {
    unsigned char mtype = msg[0];
    size_t offset = 1 + ECP_SIZE_MT_FRAG(mtype);

    if (!(mtype & ECP_MTYPE_FLAG_PTS)) return ECP_ERR;
    if (msg_size < offset + sizeof(ecp_pts_t)) return ECP_ERR;

    *pts = \
        (msg[offset] << 24)     | \
        (msg[offset + 1] << 16) | \
        (msg[offset + 2] << 8)  | \
        (msg[offset + 3]);

    return ECP_OK;
}

int ecp_msg_defrag(ECPFragIter *iter, ecp_seq_t seq, unsigned char *msg_in, size_t msg_in_size, unsigned char **msg_out, size_t *msg_out_size) {
    unsigned char frag_cnt, frag_tot;
    int rv = ecp_msg_get_frag(msg_in, msg_in_size, &frag_cnt, &frag_tot);
    if (rv == ECP_OK) {
        unsigned char mtype = ecp_msg_get_type(msg_in) & (~ECP_MTYPE_FLAG_FRAG);
        unsigned char *content = NULL;
        size_t content_size = 0;
        size_t buf_offset = 0;

        content = ecp_msg_get_content(msg_in, msg_in_size);
        if (content == NULL) return ECP_ERR_MIN_MSG;

        content_size = msg_in_size - (content - msg_in);
        if (frag_cnt == 0) {
            iter->seq = seq;
            iter->frag_cnt = 0;
            iter->frag_size = content_size;
            iter->buffer[0] = mtype;
            if (ECP_SIZE_MT_FLAG(mtype)) {
                memcpy(iter->buffer + 1, msg_in, ECP_SIZE_MT_FLAG(mtype));
            }
        } else {
            if (iter->seq + frag_cnt != seq) {
                iter->seq = seq - frag_cnt;
                iter->frag_cnt = 0;
                return ECP_ERR_ITER;
            }
            if (iter->frag_cnt != frag_cnt) return ECP_ERR_ITER;
        }
        if ((1 + ECP_SIZE_MT_FLAG(mtype) + iter->frag_size * frag_tot) > iter->buf_size) return ECP_ERR_SIZE;
        
        buf_offset = 1 + ECP_SIZE_MT_FLAG(mtype) + iter->frag_size * frag_cnt;
        memcpy(iter->buffer + buf_offset, content, content_size);
        iter->frag_cnt++;
        
        if (iter->frag_cnt == frag_tot) {
            *msg_out = iter->buffer;
            *msg_out_size = buf_offset + content_size;
            return ECP_OK;
        } else {
            return ECP_ITER_NEXT;
        }
    } else {
        iter->seq = seq;
        *msg_out = msg_in;
        *msg_out_size = msg_in_size;
        return ECP_OK;
    }
}

ssize_t ecp_msg_handle(ECPConnection *conn, ecp_seq_t seq, unsigned char *msg, size_t msg_size, ECP2Buffer *bufs) {
    ecp_conn_handler_msg_t *handler = NULL;
    ssize_t rv = 0;
    unsigned char mtype = 0;
    unsigned char *content = NULL;
    size_t rem_size = msg_size;
    
    while (rem_size) {
        mtype = ecp_msg_get_type(msg);
        if ((mtype & ECP_MTYPE_MASK) >= ECP_MAX_MTYPE) return ECP_ERR_MAX_MTYPE;

        ecp_timer_pop(conn, mtype);

#ifdef ECP_WITH_RBUF
        if (conn->rbuf.recv && conn->rbuf.recv->frag_iter) {
            int _rv = ecp_msg_defrag(conn->rbuf.recv->frag_iter, seq, msg, msg_size, &msg, &rem_size);
            if (_rv < 0) return _rv;
            if (_rv == ECP_ITER_NEXT) return msg_size;
        }
#endif

        content = ecp_msg_get_content(msg, rem_size);
        if (content == NULL) return ECP_ERR_MIN_MSG;
        rem_size -= content - msg;
        
        handler = conn->sock->ctx->handler[conn->type] ? conn->sock->ctx->handler[conn->type]->msg[mtype & ECP_MTYPE_MASK] : NULL;
        if (handler) {
            rv = handler(conn, seq, mtype, content, rem_size, bufs);
            if (rv < 0) return rv;
            if (rv > rem_size) return ECP_ERR;

            rem_size -= rv;
            msg = content + rv;
        } else {
            return msg_size - rem_size - 1;
        }
    }

    return msg_size;
}

void ecp_pld_set_type(unsigned char *payload, unsigned char mtype) {
    payload[ECP_SIZE_PLD_HDR] = mtype;
}

int ecp_pld_set_frag(unsigned char *payload, unsigned char mtype, unsigned char frag_cnt, unsigned char frag_tot) {
    size_t offset = ECP_SIZE_PLD_HDR + 1;

    if (!(mtype & ECP_MTYPE_FLAG_FRAG)) return ECP_ERR;
    payload[offset] = frag_cnt;
    payload[offset + 1] = frag_tot;
    return ECP_OK;
}

int ecp_pld_set_pts(unsigned char *payload, unsigned char mtype, ecp_pts_t pts) {
    size_t offset = ECP_SIZE_PLD_HDR + 1 + ECP_SIZE_MT_FRAG(mtype);

    if (!(mtype & ECP_MTYPE_FLAG_PTS)) return ECP_ERR;

    payload[offset] = (pts & 0xFF000000) >> 24;
    payload[offset + 1] = (pts & 0x00FF0000) >> 16;
    payload[offset + 2] = (pts & 0x0000FF00) >> 8;
    payload[offset + 3] = (pts & 0x000000FF);

    return ECP_OK;
}

unsigned char ecp_pld_get_type(unsigned char *payload) {
    return payload[ECP_SIZE_PLD_HDR];
}

unsigned char *ecp_pld_get_buf(unsigned char *payload, unsigned char mtype) {
    return payload + ECP_SIZE_PLD_HDR + 1 + ECP_SIZE_MT_FLAG(mtype);
}

static ssize_t pld_send(ECPConnection *conn, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, ECPBuffer *payload, size_t pld_size, unsigned char flags, ECPTimerItem *ti, ECPSeqItem *si) {
    ECPSocket *sock = conn->sock;
    ECPContext *ctx = sock->ctx;
    ECPNetAddr addr;

    ssize_t rv = ctx->pack(conn, packet, s_idx, c_idx, payload, pld_size, si, &addr);
    if (rv < 0) return rv;

#ifdef ECP_WITH_RBUF
    if (conn->rbuf.send) return ecp_rbuf_pkt_send(conn->rbuf.send, conn->sock, &addr, packet, rv, flags, ti, si);
#endif
    
    if (ti) {
        int _rv = ecp_timer_push(ti);
        if (_rv) return _rv;
    }
    return ecp_pkt_send(sock, &addr, packet, rv, flags);
}

ssize_t ecp_pld_send(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pld_size, unsigned char flags) {
    return ecp_pld_send_ll(conn, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, payload, pld_size, flags, NULL);
}

ssize_t ecp_pld_send_wtimer(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pld_size, unsigned char flags, ECPTimerItem *ti) {
    return ecp_pld_send_ll(conn, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, payload, pld_size, flags, ti);
}

ssize_t ecp_pld_send_ll(ECPConnection *conn, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, ECPBuffer *payload, size_t pld_size, unsigned char flags, ECPTimerItem *ti) {
    ECPSeqItem *_seq_item = NULL;

#ifdef ECP_WITH_RBUF
    ECPSeqItem seq_item;

    if (conn->rbuf.send) {
        int _rv = ecp_seq_item_init(&seq_item);
        if (_rv) return _rv;
        _seq_item = &seq_item;
    }
#endif

    return pld_send(conn, packet, s_idx, c_idx, payload, pld_size, flags, ti, _seq_item);
}

ssize_t ecp_pld_send_raw(ECPSocket *sock, ECPConnection *parent, ECPNetAddr *addr, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, ecp_dh_public_t *public, ecp_aead_key_t *shsec, unsigned char *nonce, ecp_seq_t seq, ECPBuffer *payload, size_t pld_size, unsigned char flags) {
    ECPContext *ctx = sock->ctx;
    ECPNetAddr _addr;
    ssize_t rv;

    rv = ctx->pack_raw(sock, parent, packet, s_idx, c_idx, public, shsec, nonce, seq, payload, pld_size, &_addr);
    if (rv < 0) return rv;
    
    return ecp_pkt_send(sock, parent ? &_addr : addr, packet, rv, flags);
}

ssize_t ecp_send(ECPConnection *conn, unsigned char mtype, unsigned char *content, size_t content_size) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_MAX_PKT];
    unsigned char pld_buf[ECP_MAX_PLD];
    ssize_t rv = 0;
    int pkt_cnt = 0;
    int vc_cnt = conn->pcount + 1;
    size_t size_max = ECP_MAX_PKT - (ECP_SIZE_PKT_HDR + ECP_AEAD_SIZE_TAG + ECP_SIZE_SEQ + 1) * vc_cnt;

    packet.buffer = pkt_buf;
    packet.size = ECP_MAX_PKT;
    payload.buffer = pld_buf;
    payload.size = ECP_MAX_PLD;
    
    if (content_size + ECP_SIZE_MT_FLAG(mtype) > size_max) {
        int i;
        int _rv = ECP_OK;
        size_t frag_size, frag_size_final;
        ecp_seq_t seq_start = 0;
        ECPSeqItem seq_item;

        _rv = ecp_seq_item_init(&seq_item);
        if (_rv) return _rv;
        
        mtype |= ECP_MTYPE_FLAG_FRAG;
        frag_size = size_max - ECP_SIZE_MT_FLAG(mtype);
        pkt_cnt = content_size / frag_size;
        frag_size_final = content_size - frag_size * pkt_cnt;
        if (frag_size_final) pkt_cnt++;
        
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&conn->mutex);
#endif
        
        seq_start = conn->seq_out + 1;
        conn->seq_out += pkt_cnt;
        
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn->mutex);
#endif
        
        seq_item.seq_w = 1;
        for (i=0; i<pkt_cnt; i++) {
            if ((i == pkt_cnt - 1) && frag_size_final) frag_size = frag_size_final;
            ecp_pld_set_type(pld_buf, mtype);
            ecp_pld_set_frag(pld_buf, mtype, i, pkt_cnt);
            memcpy(ecp_pld_get_buf(pld_buf, mtype), content, frag_size);
            content += frag_size;

            seq_item.seq = seq_start + i;
            ssize_t _rv = pld_send(conn, &packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, &payload, ECP_SIZE_PLD(frag_size, mtype), 0, NULL, &seq_item);
            if (_rv < 0) return _rv;

            rv += _rv;
        }
    } else {
        ecp_pld_set_type(pld_buf, mtype);
        memcpy(ecp_pld_get_buf(pld_buf, mtype), content, content_size);
        rv = ecp_pld_send(conn, &packet, &payload, ECP_SIZE_PLD(content_size, mtype), 0);
    }
    return rv;
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

#ifdef ECP_DEBUG
static char *_utoa(unsigned value, char *str, int base) {
  const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  int i, j;
  unsigned remainder;
  char c;
  
  /* Check base is supported. */
  if ((base < 2) || (base > 36))
    { 
      str[0] = '\0';
      return NULL;
    }  
    
  /* Convert to string. Digits are in reverse order.  */
  i = 0;
  do 
    {
      remainder = value % base;
      str[i++] = digits[remainder];
      value = value / base;
    } while (value != 0);  
  str[i] = '\0'; 
  
  /* Reverse string.  */
  for (j = 0, i--; j < i; j++, i--)
    {
      c = str[j];
      str[j] = str[i];
      str[i] = c; 
    }       
  
  return str;
}

static char *_itoa(int value, char *str, int base) {
  unsigned uvalue;
  int i = 0;
  
  /* Check base is supported. */
  if ((base < 2) || (base > 36))
    { 
      str[0] = '\0';
      return NULL;
    }  
    
  /* Negative numbers are only supported for decimal.
   * Cast to unsigned to avoid overflow for maximum negative value.  */ 
  if ((base == 10) && (value < 0))
    {              
      str[i++] = '-';
      uvalue = (unsigned)-value;
    }
  else
    uvalue = (unsigned)value;
  
  _utoa(uvalue, &str[i], base);
  return str;
}
#endif

static int recv_p(ECPSocket *sock, ECPNetAddr *addr, ECPBuffer *packet, size_t size) {
    ECP2Buffer bufs;
    ECPBuffer payload;
    unsigned char pld_buf[ECP_MAX_PLD];
    
    bufs.packet = packet;
    bufs.payload = &payload;

    payload.buffer = pld_buf;
    payload.size = ECP_MAX_PLD;

    ssize_t rv = ecp_pkt_handle(sock, addr, NULL, &bufs, size);
    if (rv < 0) return rv;
    
    return ECP_OK;
}

int ecp_receiver(ECPSocket *sock) {
    ECPNetAddr addr;
    ECPBuffer packet;
    unsigned char pkt_buf[ECP_MAX_PKT];
    ecp_cts_t next = 0;
    ssize_t rv = 0;
    if (sock->ctx->tr.recv == NULL) return ECP_ERR;

    sock->running = 1;
    while(sock->running) {
        packet.buffer = pkt_buf;
        packet.size = ECP_MAX_PKT;

        rv = sock->ctx->tr.recv(&sock->sock, &packet, &addr, next ? next : sock->poll_timeout);
        if (rv > 0) {
            int _rv = recv_p(sock, &addr, &packet, rv);
#ifdef ECP_DEBUG
            if (_rv) {
                char b[16];
                puts("ERR:");
                puts(_itoa(_rv, b, 10));
                puts("\n");
            }
#endif
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
