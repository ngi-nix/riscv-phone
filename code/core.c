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

int ecp_node_init(ECPContext *ctx, ECPNode *node, void *addr, ecp_dh_public_t *public) {
    int rv = ctx->tr.addr_set(&node->addr, addr);
    if (rv) return ECP_ERR_NET_ADDR;

    memcpy(&node->public, public, sizeof(node->public));
    return ECP_OK;
}

ssize_t ecp_pack(ECPContext *ctx, unsigned char *packet, size_t pkt_size, unsigned char s_idx, unsigned char c_idx, ecp_dh_public_t *public, ecp_aead_key_t *shsec, unsigned char *nonce, unsigned char *seq, unsigned char *payload, size_t payload_size) {
    ssize_t rv;

    if (packet == NULL) return ECP_ERR;
    if (payload == NULL) return ECP_ERR;

    packet[0] = 0;
    packet[1] = 0;
    s_idx = s_idx & 0x0F;
    c_idx = c_idx & 0x0F;
    packet[ECP_SIZE_PROTO] = (s_idx << 4) | c_idx;
    ctx->cr.dh_pub_to_buf(packet+ECP_SIZE_PROTO+1, public);
    memcpy(packet+ECP_SIZE_PROTO+1+ECP_ECDH_SIZE_KEY, nonce, ECP_AEAD_SIZE_NONCE);

    memcpy(payload, seq, ECP_SIZE_SEQ);
    rv = ctx->cr.aead_enc(packet+ECP_SIZE_PKT_HDR, pkt_size-ECP_SIZE_PKT_HDR, payload, payload_size, shsec, packet+ECP_SIZE_PROTO+1+ECP_ECDH_SIZE_KEY);
    if (rv < 0) return ECP_ERR_ENCRYPT;
    
    memcpy(nonce, packet+ECP_SIZE_PKT_HDR, ECP_AEAD_SIZE_NONCE);
        
    return rv+ECP_SIZE_PKT_HDR;
}
    
int ecp_ctx_create(ECPContext *ctx) {
    int rv;
    
    if (ctx == NULL) return ECP_ERR;

    memset(ctx, 0, sizeof(ECPContext));

    rv = ecp_crypto_init(&ctx->cr);
    if (rv) return rv;
#ifdef ECP_WITH_HTABLE
    rv = ecp_htable_init(&ctx->ht);
    if (rv) return rv;
#endif
    rv = ecp_transport_init(&ctx->tr);
    if (rv) return rv;
    rv = ecp_time_init(&ctx->tm);
    if (rv) return rv;
    
    return ECP_OK;
}

int ecp_ctx_destroy(ECPContext *ctx) {
    return ECP_OK;
}

static int ctable_create(ECPSockCTable *conn, ECPContext *ctx) {
    int rv = ECP_OK;
    
    memset(conn, 0, sizeof(ECPSockCTable));
    if (ctx->ht.init) {
        conn->htable = ctx->ht.create(ctx);
        if (conn->htable == NULL) return ECP_ERR_ALLOC;
    }

#ifdef ECP_WITH_PTHREAD
    rv = pthread_mutex_init(&conn->mutex, NULL);
    if (rv && ctx->ht.init) ctx->ht.destroy(conn->htable);
#endif
    return rv;
}

static void ctable_destroy(ECPSockCTable *conn, ECPContext *ctx) {
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_destroy(&conn->mutex);
#endif
    if (ctx->ht.init) ctx->ht.destroy(conn->htable);
}

static int ctable_insert(ECPConnection *conn) {
    ECPSocket *sock = conn->sock;
    ECPContext *ctx = sock->ctx;

    if (ctx->ht.init) {
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
    } else {
        if (sock->conn.size == ECP_MAX_SOCK_CONN) return ECP_ERR_MAX_SOCK_CONN;
        sock->conn.array[sock->conn.size] = conn;
        sock->conn.size++;
    }

    return ECP_OK;
}

static void ctable_remove(ECPConnection *conn) {
    int i;
    ECPSocket *sock = conn->sock;
    ECPContext *ctx = sock->ctx;
    
    if (ctx->ht.init) {
        int i;
        if (conn->out) {
            for (i=0; i<ECP_MAX_CONN_KEY; i++) if (conn->key[i].valid) ctx->ht.remove(sock->conn.htable, ctx->cr.dh_pub_get_buf(&conn->key[i].public));
        } else {
            ECPDHRKeyBucket *remote = &conn->remote;
            for (i=0; i<ECP_MAX_NODE_KEY; i++) if (remote->key[i].idx != ECP_ECDH_IDX_INV) ctx->ht.remove(sock->conn.htable, ctx->cr.dh_pub_get_buf(&remote->key[i].public));
        }
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
    ECPConnection *conn = NULL;
    int i;
    
    if (sock->ctx->ht.init) {
        return sock->ctx->ht.search(sock->conn.htable, c_public);
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

static ssize_t conn_handle_open(ECPConnection *conn, unsigned char ptype, unsigned char *payload, ssize_t size);
static ssize_t conn_handle_kget(ECPConnection *conn, unsigned char ptype, unsigned char *payload, ssize_t size);
static ssize_t conn_handle_kput(ECPConnection *conn, unsigned char ptype, unsigned char *payload, ssize_t size);

int ecp_sock_create(ECPSocket *sock, ECPContext *ctx, ECPDHKey *key) {
    int rv = ECP_OK; 
    
    if (sock == NULL) return ECP_ERR;
    if (ctx == NULL) return ECP_ERR;
    if (key == NULL) return ECP_ERR;

    memset(sock, 0, sizeof(ECPSocket));
    sock->ctx = ctx;
    sock->poll_timeout = ECP_POLL_TIMEOUT;
    sock->key_curr = 0;
    sock->key_perma = *key;
    sock->handler[ECP_PTYPE_OPEN] = conn_handle_open;
    sock->handler[ECP_PTYPE_KGET] = conn_handle_kget;
    sock->handler[ECP_PTYPE_KPUT] = conn_handle_kput;

    rv = ecp_dhkey_generate(sock->ctx, &sock->key[sock->key_curr]);
    if (!rv) rv = ctable_create(&sock->conn, sock->ctx);
    
    if (rv) return rv;
    
    rv = ecp_timer_create(&sock->timer);
    if (rv) {
        ctable_destroy(&sock->conn, sock->ctx);
    }
    
#ifdef ECP_WITH_PTHREAD
    if (!rv) rv = pthread_mutex_init(&sock->mutex, NULL);
    if (rv) {
        ecp_timer_destroy(&sock->timer);
        ctable_destroy(&sock->conn, sock->ctx);
    }
#endif

    return rv;
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
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->mutex);
#endif

    *idx = sock->key_curr;
    if (*idx != ECP_ECDH_IDX_INV) sock->ctx->cr.dh_pub_to_buf(public, &sock->key[sock->key_curr].public);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&sock->mutex);
#endif

    if (*idx == ECP_ECDH_IDX_INV) return ECP_ERR_ECDH_IDX;
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
    if (ctx->ht.init && conn->out && ecp_conn_is_reg(conn)) {
        int rv;
        unsigned char *pub_s = NULL;
        if ((conn->key_curr != ECP_ECDH_IDX_INV) && conn->key[conn->key_curr].valid) {
            pub_s = ctx->cr.dh_pub_get_buf(&conn->key[conn->key_curr].public);
            ctx->ht.remove(sock->conn.htable, pub_s);
        }
        pub_s = ctx->cr.dh_pub_get_buf(&key->public);
        rv = ctx->ht.insert(sock->conn.htable, pub_s, conn);
        if (rv) return rv;
    }
    conn->key[conn->key_curr] = *key;
    conn->key_idx_map[conn->key_curr] = ECP_ECDH_IDX_INV;

    return ECP_OK;
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

    if (ctx->ht.init && !conn->out && ecp_conn_is_reg(conn)) {
        if (remote->key[new].idx != ECP_ECDH_IDX_INV) {
            unsigned char *pub_s = ctx->cr.dh_pub_get_buf(&remote->key[new].public);
            ctx->ht.remove(sock->conn.htable, pub_s);
        }
        int rv = ctx->ht.insert(sock->conn.htable, public, conn);
        if (rv) return rv;
    }
    
    if (remote->key[new].idx != ECP_ECDH_IDX_INV) remote->key_idx_map[remote->key[new].idx] = ECP_ECDH_IDX_INV;
    remote->key_idx_map[idx] = new;
    ctx->cr.dh_pub_from_buf(&remote->key[new].public, public);
    remote->key[new].idx = idx;
    remote->key_curr = new;

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


static int conn_new(ECPConnection **_conn, ECPSocket *sock, unsigned char s_idx, unsigned char c_idx, unsigned char *c_public, ecp_aead_key_t *shsec, unsigned char *payload, size_t payload_size) {
    ECPConnection *conn = NULL;
    int rv = ECP_OK;
    
    if (_conn == NULL) return ECP_ERR;
    if (sock == NULL) return ECP_ERR;

    conn = sock->ctx->conn_alloc ? sock->ctx->conn_alloc() : NULL;
    if (conn == NULL) return ECP_ERR_ALLOC;

    rv = ecp_conn_create(conn, sock);
    if (rv) {
        if (sock->ctx->conn_free) sock->ctx->conn_free(conn);
        return rv;
    }
    conn->refcount = 1;
    rv = conn_dhkey_new_pub_local(conn, s_idx);
    if (!rv) rv = conn_dhkey_new_pub_remote(conn, c_idx, c_public);
    if (!rv) rv = conn_shsec_set(conn, s_idx, c_idx, shsec);
    if (!rv && sock->conn_create) rv = sock->conn_create(conn, payload, payload_size);
    if (rv) {
        ecp_conn_destroy(conn);
        if (sock->ctx->conn_free) sock->ctx->conn_free(conn);
        return rv;
    }

    rv = ecp_conn_register(conn);
    if (rv) {
        if (sock->conn_destroy) sock->conn_destroy(conn);
        ecp_conn_destroy(conn);
        if (sock->ctx->conn_free) sock->ctx->conn_free(conn);
        return rv;
    }

    *_conn = conn;
    return rv;
}

static ssize_t conn_handle_open(ECPConnection *conn, unsigned char ptype, unsigned char *payload, ssize_t size) {
    ecp_conn_handler_t *handler = conn->handler ? conn->handler->f[ptype] : NULL;

    if (conn->out) {
        if (size < 0) return handler ? handler(conn, ptype, payload, size) : 0;

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&conn->mutex);
#endif
        if (!ecp_conn_is_open(conn)) conn->flags |= ECP_CONN_FLAG_OPEN;
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn->mutex);
#endif

        return handler ? handler(conn, ptype, payload, size) : 0;
    } else {
        if (size < 0) return ECP_ERR;

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&conn->mutex);
#endif
        if (!ecp_conn_is_open(conn)) conn->flags |= ECP_CONN_FLAG_OPEN;
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn->mutex);
#endif

        ssize_t _rv = ecp_send(conn, ECP_PTYPE_OPEN, NULL, 0);

        return handler ? handler(conn, ptype, payload, size) : 0;
    }

    return ECP_ERR;
}

static ssize_t conn_handle_kget(ECPConnection *conn, unsigned char ptype, unsigned char *payload, ssize_t size) {
    ecp_conn_handler_t *handler = conn->handler ? conn->handler->f[ptype] : NULL;

    if (conn->out) {
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&conn->mutex);
#endif
        int conn_is_open = ecp_conn_is_open(conn);
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn->mutex);
#endif

        if (size < 0) {
            if (!conn_is_open) handler = conn->handler ? conn->handler->f[ECP_PTYPE_OPEN] : handler;
            return handler ? handler(conn, ptype, payload, size) : 0;
        }

        if (size < ECP_ECDH_SIZE_KEY+1) return ECP_ERR;

        int rv = ecp_conn_dhkey_new_pub(conn, payload[0], payload+1);
        if (!rv && !conn_is_open) {
            ECPTimerItem ti;
            ssize_t _rv = 0;

            rv = ecp_timer_item_init(&ti, conn, ECP_PTYPE_OPEN, 3, 500);
            if (!rv) rv = ecp_timer_push(conn, &ti);
            if (!rv) _rv = ecp_send(conn, ECP_PTYPE_OPEN, NULL, 0);
            if (_rv < 0) rv = _rv;
        }

        return handler ? handler(conn, ptype, rv ? NULL : payload, rv ? rv : size) : ECP_ECDH_SIZE_KEY+1;
    } else {
        unsigned char payload[ECP_SIZE_PLD(ECP_ECDH_SIZE_KEY+1)];
        unsigned char *buf = ecp_pld_get_buf(payload);
        ecp_pld_set_type(payload, ECP_PTYPE_KGET);

        if (size < 0) return ECP_ERR;

        int rv = ecp_conn_dhkey_get_curr(conn, buf, buf+1);
        if (rv) return rv;
        
        ssize_t _rv = ecp_pld_send(conn, payload, sizeof(payload));

        return handler ? handler(conn, ptype, payload, size) : 0;
    }

    return ECP_ERR;
}
 
static ssize_t conn_handle_kput(ECPConnection *conn, unsigned char ptype, unsigned char *payload, ssize_t size) {
    ecp_conn_handler_t *handler = conn->handler ? conn->handler->f[ptype] : NULL;

    if (conn->out) {
        return handler ? handler(conn, ptype, payload, size) : 0;
    } else {
        if (size < ECP_ECDH_SIZE_KEY+1) return ECP_ERR;

        int rv = ecp_conn_dhkey_new_pub(conn, payload[0], payload+1);
        if (rv) return rv;

        ssize_t _rv = ecp_send(conn, ECP_PTYPE_KPUT, NULL, 0);

        return handler ? handler(conn, ptype, payload, size) : ECP_ECDH_SIZE_KEY+1;
    }

    return ECP_ERR;
}

int ecp_conn_create(ECPConnection *conn, ECPSocket *sock) {
    int i;
    
    if (conn == NULL) return ECP_ERR;
    if (sock == NULL) return ECP_ERR;

    memset(conn, 0, sizeof(ECPConnection));

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
    int rv = pthread_mutex_init(&conn->mutex, NULL);
    if (rv) return ECP_ERR;

    rv = ecp_conn_msgq_create(conn);
    if (rv) {
        pthread_mutex_destroy(&conn->mutex);
        return ECP_ERR;
    }
#endif

    return ECP_OK;
}

void ecp_conn_destroy(ECPConnection *conn) {
#ifdef ECP_WITH_PTHREAD
    ecp_conn_msgq_destroy(conn);
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

int ecp_conn_unregister(ECPConnection *conn, unsigned int timeout) {
    ECPSocket *sock = conn->sock;
    int refcount = 0;

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

    return ECP_OK;
}

static int _conn_kget_send(ECPConnection *conn, ECPTimerItem *ti) {
    unsigned char payload[ECP_MIN_PLD];
    ecp_pld_set_type(payload, ECP_PTYPE_KGET);
    ssize_t rv = ecp_pld_send_wkey(conn, ECP_ECDH_IDX_PERMA, ECP_ECDH_IDX_INV, payload, sizeof(payload));
    if (rv < 0) return rv;
    return ECP_OK;
}

int ecp_conn_open(ECPConnection *conn, ECPNode *node, ECPConnHandler *handler) {
    int rv = ECP_OK;
    ECPDHKey key;
    ECPContext *ctx = conn->sock->ctx;
    
    if (conn == NULL) return ECP_ERR;
    if (node == NULL) return ECP_ERR;
    if (ctx->rng == NULL) return ECP_ERR_RNG;

    conn->out = 1;
    conn->node = *node;
    conn->handler = handler;
    rv = ctx->rng(conn->nonce, ECP_AEAD_SIZE_NONCE);
    if (!rv) rv = ecp_dhkey_generate(ctx, &key);
    if (!rv) rv = conn_dhkey_new_pair(conn, &key);
    if (!rv) rv = conn_dhkey_new_pub_local(conn, conn->key_curr);
    if (!rv) rv = ecp_conn_register(conn);
    if (rv) return rv;
    
    rv = ecp_timer_send(conn, _conn_kget_send, ECP_PTYPE_KGET, 3, 500);
    if (rv) {
        ecp_conn_unregister(conn, 0);
        return rv;
    }

    return ECP_OK;
}

int ecp_conn_close(ECPConnection *conn, unsigned int timeout) {
    return ecp_conn_unregister(conn, timeout);
}

int ecp_conn_hander_init(ECPConnHandler *handler) {
    memset(handler, 0, sizeof(ECPConnHandler));
    return ECP_OK;
}

int ecp_conn_dhkey_get_curr(ECPConnection *conn, unsigned char *idx, unsigned char *public) {
    ECPSocket *sock = conn->sock;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif

    *idx = conn->key_curr;
    if (conn->key_curr != ECP_ECDH_IDX_INV) sock->ctx->cr.dh_pub_to_buf(public, &conn->key[conn->key_curr].public);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif
    
    if (*idx == ECP_ECDH_IDX_INV) return ecp_sock_dhkey_get_curr(sock, idx, public);
    return ECP_OK;
}
    
static int _conn_kput_send(ECPConnection *conn, ECPTimerItem *ti) {
    unsigned char payload[ECP_SIZE_PLD(ECP_ECDH_SIZE_KEY+1)];
    unsigned char *buf = ecp_pld_get_buf(payload);
    ecp_pld_set_type(payload, ECP_PTYPE_KPUT);

    int rv = ecp_conn_dhkey_get_curr(conn, buf, buf+1);
    if (rv) return rv;

    ssize_t _rv = ecp_pld_send(conn, payload, sizeof(payload));
    if (_rv < 0) return _rv;
    
    return ECP_OK;
}

int ecp_conn_dhkey_new(ECPConnection *conn) {
    ECPSocket *sock = conn->sock;
    ECPDHKey new_key;
    unsigned char key_curr;
    int rv;
    
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

    if (!rv) rv = ecp_timer_send(conn, _conn_kput_send, ECP_PTYPE_KPUT, 3, 500);
        
    return rv;
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

ssize_t ecp_conn_pack(ECPConnection *conn, ECPNetAddr *addr, unsigned char *packet, size_t pkt_size, unsigned char s_idx, unsigned char c_idx, unsigned char *payload, size_t payload_size) {
    ecp_aead_key_t shsec;
    ecp_dh_public_t public;
    unsigned char nonce[ECP_AEAD_SIZE_NONCE];
    unsigned char seq[ECP_SIZE_SEQ];
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
        conn->seq_out++;
        seq[0] = (conn->seq_out & 0xFF000000) >> 24;
        seq[1] = (conn->seq_out & 0x00FF0000) >> 16;
        seq[2] = (conn->seq_out & 0x0000FF00) >> 8;
        seq[3] = (conn->seq_out & 0x000000FF);
        *addr = conn->node.addr;
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif
    
    if (rv) return rv;
    
    ssize_t _rv = ecp_pack(conn->sock->ctx, packet, pkt_size, s_idx, c_idx, &public, &shsec, nonce, seq, payload, payload_size);
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

ssize_t ecp_pkt_handle_err(ECPSocket *sock, ECPNetAddr *addr, unsigned char *packet, size_t pkt_size) {
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
    ssize_t _rv = 0;
    uint32_t c_seq, p_seq, n_seq, seq_bitmap;
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
        c_seq = conn->seq_in;
        seq_bitmap = conn->seq_in_bitmap;
        conn->refcount++;
    }

#ifdef ECP_WITH_PTHREAD
    if (conn) pthread_mutex_unlock(&conn->mutex);
#endif

    if (rv) return rv;

    if (key) {
        sock->ctx->cr.dh_pub_from_buf(&public, packet+ECP_SIZE_PROTO+1);
        sock->ctx->cr.dh_shsec(&shsec, &public, &private);
    }

    memset(private, 0, sizeof(private));

    pld_size = sock->ctx->cr.aead_dec(payload, ECP_MAX_PLD, packet+ECP_SIZE_PKT_HDR, pkt_size-ECP_SIZE_PKT_HDR, &shsec, packet+ECP_SIZE_PROTO+1+ECP_ECDH_SIZE_KEY);
    if (pld_size < ECP_MIN_PLD) rv = ECP_ERR_DECRYPT;
    if (rv) goto pkt_handle_err;

    p_seq = \
        (payload[0] << 24) | \
        (payload[1] << 16) | \
        (payload[2] << 8)  | \
        (payload[3]);

    memcpy(nonce, packet+ECP_SIZE_PKT_HDR, sizeof(nonce));

    if (conn == NULL) {
        if (payload[ECP_SIZE_SEQ] == ECP_PTYPE_OPEN) {
            rv = conn_new(&conn, sock, s_idx, c_idx, packet+ECP_SIZE_PROTO+1, &shsec, payload+ECP_SIZE_PLD_HDR, pld_size-ECP_SIZE_PLD_HDR);
            if (rv) return rv;

            seq_bitmap = 0;
            n_seq = p_seq;
        } else if (payload[ECP_SIZE_SEQ] == ECP_PTYPE_KGET) {
            unsigned char _payload[ECP_SIZE_PLD(ECP_ECDH_SIZE_KEY+1)];
            unsigned char *buf = ecp_pld_get_buf(_payload);
            ecp_pld_set_type(_payload, ECP_PTYPE_KGET);

            rv = ecp_sock_dhkey_get_curr(sock, buf, buf+1);
            if (!rv) {
                ssize_t _rv = ecp_pld_send_raw(sock, addr, s_idx, c_idx, &public, &shsec, nonce, payload, _payload, sizeof(_payload));
                if (_rv < 0) rv = _rv;
            }
            return ECP_MIN_PKT;
        } else {
            return ECP_ERR_CONN_NOT_FOUND;
        }
    } else {
        /*
        if ((c_seq > p_seq) && (c_seq - p_seq >= 32)) rv = ECP_ERR_SEQ;
        if ((p_seq > c_seq) && (p_seq - c_seq > 32)) rv = ECP_ERR_SEQ;

        if (rv) goto pkt_handle_err;
    
        if (p_seq <= c_seq) {
            uint32_t p_bit = (uint32_t)1 << (c_seq - p_seq);
            if (seq_bitmap & p_bit) rv = ECP_ERR_SEQ;
            if (rv) goto pkt_handle_err;
            seq_bitmap = seq_bitmap | p_bit;
            n_seq = c_seq;
        } else {
            seq_bitmap = seq_bitmap << (p_seq - c_seq);
            seq_bitmap = seq_bitmap | 1;
            n_seq = p_seq;
        }
        */
        
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
    
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif
    memcpy(conn->nonce, nonce, sizeof(conn->nonce));
    conn->node.addr = *addr;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    cnt_size = pld_size-ECP_SIZE_SEQ;
    proc_size = ecp_pld_handle(conn, payload+pld_size-cnt_size, cnt_size);
    if (proc_size < 0) rv = ECP_ERR_HANDLE;
    if (!rv) cnt_size -= proc_size;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
    if (!rv && (cnt_size > 0)) {
        proc_size = ecp_conn_msgq_push(conn, payload+pld_size-cnt_size, cnt_size);
        if (proc_size < 0) rv = ECP_ERR_HANDLE;
        if (!rv) cnt_size -= proc_size;
    }
#endif
    if (!rv) {
        conn->seq_in = n_seq;
        conn->seq_in_bitmap = seq_bitmap;
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

ssize_t ecp_pld_handle(ECPConnection *conn, unsigned char *payload, size_t payload_size) {
    ecp_conn_handler_t *handler = NULL;
    ssize_t rv = 0;
    unsigned char ptype = 0;
    size_t rem_size = payload_size;
    ECPSocket *sock = conn->sock;    
    
    while (rem_size) {
        ptype = payload[0];
        payload++;
        rem_size--;

        if (ptype >= ECP_MAX_PTYPE) return ECP_ERR_MAX_PTYPE;
        if (rem_size < ECP_MIN_MSG) return ECP_ERR_MIN_MSG;

        ecp_timer_pop(conn, ptype);
        handler = (ptype < ECP_MAX_PTYPE_SYS) && sock->handler[ptype] ? sock->handler[ptype] : (conn->handler ? conn->handler->f[ptype] : NULL);
        if (handler) {
            rv = handler(conn, ptype, payload, rem_size);
            if (rv == 0) rv = rem_size;
            if (rv < 0) return rv;
            if (rv > rem_size) return ECP_ERR;

            rem_size -= rv;
        } else {
            return payload_size-rem_size-1;
        }
    }

    return payload_size;
}

unsigned char *ecp_pld_get_buf(unsigned char *payload) {
    return payload+ECP_SIZE_PLD_HDR;
}

void ecp_pld_set_type(unsigned char *payload, unsigned char ptype) {
    payload[ECP_SIZE_SEQ] = ptype;
}

ssize_t ecp_pld_send(ECPConnection *conn, unsigned char *payload, size_t payload_size) {
    return ecp_pld_send_wkey(conn, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, payload, payload_size);
}

ssize_t ecp_pld_send_wkey(ECPConnection *conn, unsigned char s_idx, unsigned char c_idx, unsigned char *payload, size_t payload_size) {
    ECPSocket *sock = conn->sock;
    ECPNetAddr addr;
    unsigned char packet[ECP_MAX_PKT];
    ssize_t rv;

    rv = ecp_conn_pack(conn, &addr, packet, ECP_MAX_PKT, s_idx, c_idx, payload, payload_size);
    if (rv < 0) return rv;
    
    rv = ecp_pkt_send(sock, &addr, packet, rv);
    if (rv < 0) return rv;
    
    return rv-ECP_SIZE_PKT_HDR-ECP_AEAD_SIZE_TAG;
}

ssize_t ecp_pld_send_raw(ECPSocket *sock, ECPNetAddr *addr, unsigned char s_idx, unsigned char c_idx, ecp_dh_public_t *public, ecp_aead_key_t *shsec, unsigned char *nonce, unsigned char *seq, unsigned char *payload, size_t payload_size) {
    unsigned char packet[ECP_MAX_PKT];
    ssize_t rv;

    rv = ecp_pack(sock->ctx, packet, ECP_MAX_PKT, s_idx, c_idx, public, shsec, nonce, seq, payload, payload_size);
    if (rv < 0) return rv;
    
    rv = ecp_pkt_send(sock, addr, packet, rv);
    if (rv < 0) return rv;
    
    return rv-ECP_SIZE_PKT_HDR-ECP_AEAD_SIZE_TAG;
}
    
ssize_t ecp_send(ECPConnection *conn, unsigned char ptype, unsigned char *payload, size_t payload_size) {
    ssize_t rv;
    
    if (payload == NULL) {
        unsigned char _payload[ECP_MIN_PLD];
        ecp_pld_set_type(_payload, ptype);
        rv = ecp_pld_send_wkey(conn, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, _payload, sizeof(_payload));
    } else {
        ecp_pld_set_type(payload, ptype);
        rv = ecp_pld_send_wkey(conn, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, payload, ECP_SIZE_PLD(payload_size));
    }
    return rv-ECP_SIZE_PLD_HDR;
}

ssize_t ecp_receive(ECPConnection *conn, unsigned char ptype, unsigned char *payload, size_t payload_size, unsigned int timeout) {
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
    ssize_t rv = ecp_conn_msgq_pop(conn, ptype, payload, payload_size, timeout);
    pthread_mutex_unlock(&conn->mutex);
    return rv;
#else
    return ECP_ERR_NOT_IMPLEMENTED;
#endif
}

static int recv_p(ECPSocket *sock) {
    ECPNetAddr addr;
    unsigned char packet[ECP_MAX_PKT];
    
    ssize_t rv = ecp_pkt_recv(sock, &addr, packet, ECP_MAX_PKT);
    if (rv < 0) return rv;
    
    rv = ecp_pkt_handle(sock, &addr, packet, rv);
    if (rv < 0) return rv;
    
    return ECP_OK;
}

int ecp_receiver(ECPSocket *sock) {
    unsigned int next = 0;
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
