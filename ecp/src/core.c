#include "core.h"

#include "cr.h"
#include "tr.h"
#include "tm.h"

#ifdef ECP_WITH_HTABLE
#include "ht.h"
#endif

int ecp_ctx_init(ECPContext *ctx) {
    int rv;

    if (ctx == NULL) return ECP_ERR;

    memset(ctx, 0, sizeof(ECPContext));

    rv = ecp_tr_init(ctx);
    if (rv) return rv;
    rv = ecp_tm_init(ctx);
    if (rv) return rv;

    return ECP_OK;
}

int ecp_node_init(ECPNode *node, ecp_dh_public_t *public, void *addr) {
    memset(node, 0, sizeof(ECPNode));

    if (public) memcpy(&node->public, public, sizeof(node->public));

    if (addr) {
        int rv;

        rv = ecp_tr_addr_set(&node->addr, addr);
        if (rv) return ECP_ERR_NET_ADDR;
    }

    return ECP_OK;
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

void ecp_frag_iter_reset(ECPFragIter *iter) {
    iter->seq = 0;
    iter->frag_cnt = 0;
    iter->msg_size = 0;
}

int ecp_dhkey_gen(ECPContext *ctx, ECPDHKey *key) {
    int rv;

    if (ctx->rng == NULL) return ECP_ERR_RNG;

    rv = ecp_cr_dh_mkpair(&key->public, &key->private, ctx->rng);
    if (rv) return rv;

    key->valid = 1;
    return ECP_OK;
}

static int ctable_create(ECPSockCTable *conn, ECPContext *ctx) {
    int rv;

    memset(conn, 0, sizeof(ECPSockCTable));

#ifdef ECP_WITH_HTABLE
    conn->htable = ecp_ht_create(ctx);
    if (conn->htable == NULL) return ECP_ERR_ALLOC;
#endif

#ifdef ECP_WITH_PTHREAD
    rv = pthread_mutex_init(&conn->mutex, NULL);
    if (rv) {
#ifdef ECP_WITH_HTABLE
        ecp_ht_destroy(conn->htable);
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
    ecp_ht_destroy(conn->htable);
#endif
}

static int ctable_insert(ECPConnection *conn) {
    ECPSocket *sock = conn->sock;

#ifdef ECP_WITH_HTABLE
    int i, rv = ECP_OK;

    if (conn->out) {
        for (i=0; i<ECP_MAX_CONN_KEY; i++) {
            if (conn->key[i].valid) rv = ecp_ht_insert(sock->conn.htable, ecp_cr_dh_pub_get_buf(&conn->key[i].public), conn);
            if (rv) {
                int j;
                for (j=0; j<i; j++) if (conn->key[j].valid) ecp_ht_remove(sock->conn.htable, ecp_cr_dh_pub_get_buf(&conn->key[j].public));
                return rv;
            }
        }
    } else {
        ECPDHRKeyBucket *remote = &conn->remote;

        for (i=0; i<ECP_MAX_NODE_KEY; i++) {
            if (remote->key[i].idx != ECP_ECDH_IDX_INV) rv = ecp_ht_insert(sock->conn.htable, ecp_cr_dh_pub_get_buf(&remote->key[i].public), conn);
            if (rv) {
                int j;
                for (j=0; j<i; j++) if (remote->key[j].idx != ECP_ECDH_IDX_INV) ecp_ht_remove(sock->conn.htable, ecp_cr_dh_pub_get_buf(&remote->key[j].public));
                return rv;
            }
        }
    }
#else
    if (sock->conn.size == ECP_MAX_SOCK_CONN) return ECP_ERR_MAX_SOCK_CONN;
    sock->conn.array[sock->conn.size] = conn;
    sock->conn.size++;
#endif

    return ECP_OK;
}

static void ctable_remove(ECPConnection *conn) {
    int i;
    ECPSocket *sock = conn->sock;

#ifdef ECP_WITH_HTABLE
    if (conn->out) {
        for (i=0; i<ECP_MAX_CONN_KEY; i++) if (conn->key[i].valid) ecp_ht_remove(sock->conn.htable, ecp_cr_dh_pub_get_buf(&conn->key[i].public));
    } else {
        ECPDHRKeyBucket *remote = &conn->remote;
        for (i=0; i<ECP_MAX_NODE_KEY; i++) if (remote->key[i].idx != ECP_ECDH_IDX_INV) ecp_ht_remove(sock->conn.htable, ecp_cr_dh_pub_get_buf(&remote->key[i].public));
    }
#else
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

static ECPConnection *ctable_search(ECPSocket *sock, unsigned char c_idx, unsigned char *c_public, ECPNetAddr *addr) {
#ifdef ECP_WITH_HTABLE
    return ecp_ht_search(sock->conn.htable, c_public);
#else
    ECPConnection *conn = NULL;
    int i;

    if (c_public) {
        for (i=0; i<sock->conn.size; i++) {
            conn = sock->conn.array[i];
            if (conn->out) {
                if ((c_idx < ECP_MAX_CONN_KEY) && conn->key[c_idx].valid && ecp_cr_dh_pub_eq(c_public, &conn->key[c_idx].public))
                    return conn;
            } else {
                if ((c_idx < ECP_MAX_SOCK_KEY) && (conn->remote.key_idx_map[c_idx] != ECP_ECDH_IDX_INV) && ecp_cr_dh_pub_eq(c_public, &conn->remote.key[conn->remote.key_idx_map[c_idx]].public))
                    return conn;
            }
        }
    } else if (addr) {
        /* in case server is not returning client's public key in packet */
        for (i=0; i<sock->conn.size; i++) {
            conn = sock->conn.array[i];
            if (conn->out && ecp_tr_addr_eq(&conn->node.addr, addr)) return conn;
        }
    }

    return NULL;
#endif
}

int ecp_sock_create(ECPSocket *sock, ECPContext *ctx, ECPDHKey *key) {
    int rv;

    if (sock == NULL) return ECP_ERR;
    if (ctx == NULL) return ECP_ERR;

    memset(sock, 0, sizeof(ECPSocket));
    sock->ctx = ctx;
    sock->poll_timeout = ECP_POLL_TIMEOUT;
    sock->key_curr = 0;
    if (key) sock->key_perma = *key;
    sock->handler[ECP_MTYPE_OPEN] = ecp_sock_handle_open;
    sock->handler[ECP_MTYPE_KGET] = ecp_sock_handle_kget;
#ifdef ECP_WITH_DIRSRV
    sock->handler[ECP_MTYPE_DIR] = ecp_dir_handle_req;
#endif

    rv = ecp_dhkey_gen(sock->ctx, &sock->key[sock->key_curr]);
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
    return ecp_tr_open(sock, myaddr);
}

void ecp_sock_close(ECPSocket *sock) {
    ecp_tr_close(sock);
}

int ecp_sock_dhkey_get_curr(ECPSocket *sock, unsigned char *idx, unsigned char *public) {
    unsigned char _idx;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->mutex);
#endif

    _idx = sock->key_curr;
    if (_idx != ECP_ECDH_IDX_INV) ecp_cr_dh_pub_to_buf(public, &sock->key[_idx].public);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&sock->mutex);
#endif

    if (_idx == ECP_ECDH_IDX_INV) return ECP_ERR_ECDH_IDX;

    if (idx) *idx = _idx;
    return ECP_OK;
}

int ecp_sock_dhkey_new(ECPSocket *sock) {
    ECPDHKey new_key;
    int rv;

    rv = ecp_dhkey_gen(sock->ctx, &new_key);
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

    conn->key_curr = conn->key_curr == ECP_ECDH_IDX_INV ? 0 : (conn->key_curr+1) % ECP_MAX_CONN_KEY;
#ifdef ECP_WITH_HTABLE
    if (conn->out && ecp_conn_is_reg(conn) && conn->key[conn->key_curr].valid) {
        ecp_ht_remove(sock->conn.htable, ecp_cr_dh_pub_get_buf(&conn->key[conn->key_curr].public));
    }
#endif

    conn->key[conn->key_curr] = *key;
    conn->key_idx_map[conn->key_curr] = ECP_ECDH_IDX_INV;

#ifdef ECP_WITH_HTABLE
    if (conn->out && ecp_conn_is_reg(conn)) {
        int rv;

        rv = ecp_ht_insert(sock->conn.htable, ecp_cr_dh_pub_get_buf(&conn->key[conn->key_curr].public), conn);
        if (rv) return rv;
    }
#endif

    return ECP_OK;
}

static void conn_dhkey_del_pair(ECPConnection *conn, unsigned char idx) {
    ECPSocket *sock = conn->sock;

#ifdef ECP_WITH_HTABLE
    if (conn->out && ecp_conn_is_reg(conn) && conn->key[idx].valid) {
        ecp_ht_remove(sock->conn.htable, ecp_cr_dh_pub_get_buf(&conn->key[idx].public));
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
    ECPDHRKeyBucket *remote = &conn->remote;
    unsigned char new = remote->key_curr == ECP_ECDH_IDX_INV ? 0 : (remote->key_curr+1) % ECP_MAX_NODE_KEY;
    int i;

    if (idx >= ECP_MAX_SOCK_KEY) return ECP_ERR_ECDH_IDX;
    if ((remote->key_idx_map[idx] != ECP_ECDH_IDX_INV) && ecp_cr_dh_pub_eq(public, &remote->key[remote->key_idx_map[idx]].public)) return ECP_ERR_ECDH_KEY_DUP;

#ifdef ECP_WITH_HTABLE
    if (!conn->out && ecp_conn_is_reg(conn) && (remote->key[new].idx != ECP_ECDH_IDX_INV)) {
        ecp_ht_remove(sock->conn.htable, ecp_cr_dh_pub_get_buf(&remote->key[new].public));
    }
#endif

    if (remote->key[new].idx != ECP_ECDH_IDX_INV) remote->key_idx_map[remote->key[new].idx] = ECP_ECDH_IDX_INV;
    remote->key_idx_map[idx] = new;
    ecp_cr_dh_pub_from_buf(&remote->key[new].public, public);
    remote->key[new].idx = idx;
    remote->key_curr = new;

#ifdef ECP_WITH_HTABLE
    if (!conn->out && ecp_conn_is_reg(conn)) {
        int rv;

        rv = ecp_ht_insert(sock->conn.htable, ecp_cr_dh_pub_get_buf(&remote->key[new].public), conn);
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
#if 0
            ECPDHRKey *pub = NULL;

            if (c_idx >= ECP_MAX_SOCK_KEY) return ECP_ERR_ECDH_IDX;
            if (conn->remote.key_idx_map[c_idx] == ECP_ECDH_IDX_INV) return ECP_ERR_ECDH_IDX_REMOTE;

            pub = &conn->remote.key[conn->remote.key_idx_map[c_idx]];
            public_p = pub->idx != ECP_ECDH_IDX_INV ? &pub->public : NULL;
            priv = &conn->sock->key_perma;
#endif
        }
        if (public_p == NULL) return ECP_ERR_ECDH_IDX;
        if ((priv == NULL) || !priv->valid) return ECP_ERR_ECDH_IDX;
        ecp_cr_dh_shsec(shsec, public_p, &priv->private);
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
            ecp_cr_dh_shsec(&shared->secret, &pub->public, &priv->private);
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
    int i, rv;

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
    int rv;

    conn->flags |= ECP_CONN_FLAG_REG;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn.mutex);
#endif
    rv = ctable_insert(conn);
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

/* initializes outbound connection */
int ecp_conn_set_remote(ECPConnection *conn, ECPNode *node) {
    ECPDHKey key;
    ECPContext *ctx = conn->sock->ctx;
    int rv;

    if (node == NULL) return ECP_ERR;

    conn->out = 1;
    conn->node = *node;
    rv = ecp_dhkey_gen(ctx, &key);
    if (!rv) rv = ctx->rng(conn->nonce, ECP_AEAD_SIZE_NONCE);
    if (!rv) rv = ctx->rng(&conn->seq_out, sizeof(conn->seq_out));

    if (!rv) rv = conn_dhkey_new_pair(conn, &key);
    if (!rv) rv = conn_dhkey_new_pub_local(conn, conn->key_curr);
    if (!rv) rv = ecp_conn_register(conn);

    return rv;
}

int ecp_conn_get_dirlist(ECPConnection *conn, ECPNode *node) {
    int rv;
    ssize_t _rv;

    rv = ecp_conn_set_remote(conn, node);
    if (rv) return rv;

    _rv = ecp_conn_send_dir(conn);
    if (_rv < 0) {
        ecp_conn_unregister(conn);
        return _rv;
    }

    return ECP_OK;
}

int ecp_conn_open(ECPConnection *conn, ECPNode *node) {
    int rv;
    ssize_t _rv;

    rv = ecp_conn_set_remote(conn, node);
    if (rv) return rv;

    _rv = ecp_conn_send_kget(conn);
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
        ecp_tm_sleep_ms(timeout);
        pthread_mutex_lock(&conn->mutex);
        refcount = conn->refcount;
        pthread_mutex_unlock(&conn->mutex);
    }

    if (refcount) return ECP_ERR_TIMEOUT;
#endif

    if (conn->out) {
        ecp_conn_close_t handler = conn->sock->ctx->handler[conn->type] ? conn->sock->ctx->handler[conn->type]->conn_close : NULL;
        if (handler) handler(conn);
    } else {
        ecp_conn_destroy_t handler = conn->sock->ctx->handler[conn->type] ? conn->sock->ctx->handler[conn->type]->conn_destroy : NULL;
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
    int rv;
    int i;

    rv = ecp_dhkey_gen(ctx, &key);
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
#ifdef ECP_WITH_DIRSRV
    handler->msg[ECP_MTYPE_DIR] = ecp_dir_handle_update;
#endif
#ifdef ECP_WITH_RBUF
    handler->msg[ECP_MTYPE_RBACK] = ecp_rbuf_handle_ack;
    handler->msg[ECP_MTYPE_RBFLUSH] = ecp_rbuf_handle_flush;
    handler->msg[ECP_MTYPE_RBFLUSH_PTS] = ecp_rbuf_handle_flush_pts;
#endif
    handler->conn_open = ecp_conn_send_open;
    return ECP_OK;
}

int ecp_conn_dhkey_new(ECPConnection *conn) {
    ECPSocket *sock = conn->sock;
    ECPDHKey new_key;
    int rv;
    ssize_t _rv;

    rv = ecp_dhkey_gen(sock->ctx, &new_key);
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

    _rv = ecp_conn_send_kput(conn);
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
    if (_idx != ECP_ECDH_IDX_INV) ecp_cr_dh_pub_to_buf(public, &conn->key[_idx].public);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    if (_idx == ECP_ECDH_IDX_INV) return ecp_sock_dhkey_get_curr(sock, idx, public);

    if (idx) *idx = _idx;
    return ECP_OK;
}

static ssize_t _conn_send_open(ECPConnection *conn, ECPTimerItem *ti) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(1, ECP_MTYPE_OPEN_REQ, conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(1, ECP_MTYPE_OPEN_REQ, conn)];
    unsigned char *buf;

    packet.buffer = pkt_buf;
    packet.size = ECP_SIZE_PKT_BUF(1, ECP_MTYPE_OPEN_REQ, conn);
    payload.buffer = pld_buf;
    payload.size = ECP_SIZE_PLD_BUF(1, ECP_MTYPE_OPEN_REQ, conn);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_OPEN_REQ);
    buf = ecp_pld_get_buf(payload.buffer, payload.size);
    buf[0] = conn->type;

    return ecp_pld_send_wtimer(conn, &packet, &payload, ECP_SIZE_PLD(1, ECP_MTYPE_OPEN_REQ), 0, ti);
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

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_KGET_REQ);
    return _ecp_pld_send(conn, &packet, ECP_ECDH_IDX_PERMA, ECP_ECDH_IDX_INV, NULL, &payload, ECP_SIZE_PLD(0, ECP_MTYPE_KGET_REQ), 0, ti);
}

static ssize_t _conn_send_kput(ECPConnection *conn, ECPTimerItem *ti) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KPUT_REQ, conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KPUT_REQ, conn)];
    unsigned char *buf;
    int rv;

    packet.buffer = pkt_buf;
    packet.size = ECP_SIZE_PKT_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KPUT_REQ, conn);
    payload.buffer = pld_buf;
    payload.size = ECP_SIZE_PLD_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KPUT_REQ, conn);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_KPUT_REQ);
    buf = ecp_pld_get_buf(payload.buffer, payload.size);
    rv = ecp_conn_dhkey_get_curr(conn, buf, buf+1);
    if (rv) return rv;

    return ecp_pld_send_wtimer(conn, &packet, &payload, ECP_SIZE_PLD(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KPUT_REQ), 0, ti);
}

static ssize_t _conn_send_dir(ECPConnection *conn, ECPTimerItem *ti) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(0, ECP_MTYPE_DIR_REQ, conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(0, ECP_MTYPE_DIR_REQ, conn)];

    packet.buffer = pkt_buf;
    packet.size = ECP_SIZE_PKT_BUF(0, ECP_MTYPE_DIR_REQ, conn);
    payload.buffer = pld_buf;
    payload.size = ECP_SIZE_PLD_BUF(0, ECP_MTYPE_DIR_REQ, conn);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_DIR_REQ);
    return _ecp_pld_send(conn, &packet, ECP_ECDH_IDX_PERMA, ECP_ECDH_IDX_INV, NULL, &payload, ECP_SIZE_PLD(0, ECP_MTYPE_DIR_REQ), 0, ti);
}

ssize_t ecp_conn_send_open(ECPConnection *conn) {
    return ecp_timer_send(conn, _conn_send_open, ECP_MTYPE_OPEN_REP, 3, 500);
}

ssize_t ecp_conn_send_kget(ECPConnection *conn) {
    return ecp_timer_send(conn, _conn_send_kget, ECP_MTYPE_KGET_REP, 3, 500);
}

ssize_t ecp_conn_send_kput(ECPConnection *conn) {
    return ecp_timer_send(conn, _conn_send_kput, ECP_MTYPE_KPUT_REP, 3, 500);
}

ssize_t ecp_conn_send_dir(ECPConnection *conn) {
    return ecp_timer_send(conn, _conn_send_dir, ECP_MTYPE_DIR_REP, 3, 500);
}

ssize_t ecp_conn_handle_open(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    int is_open;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif
    is_open = ecp_conn_is_open(conn);
    if (!is_open && (size >= 0)) conn->flags |= ECP_CONN_FLAG_OPEN;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    if (mtype & ECP_MTYPE_FLAG_REP) {
        if (!conn->out) return ECP_ERR;

        if (is_open && size == ECP_ERR_TIMEOUT) {
            int rv;

            rv = ecp_conn_reset(conn);
            if (rv) return rv;
            return 0;
        }

        if (size < 0) return size;
        return 0;
    } else {
        if (size < 0) return size;
        if (size < 1) return ECP_ERR;
        if (conn->out) return ECP_ERR;

        ECPBuffer packet;
        ECPBuffer payload;
        unsigned char pkt_buf[ECP_SIZE_PKT_BUF(0, ECP_MTYPE_OPEN_REP, conn)];
        unsigned char pld_buf[ECP_SIZE_PLD_BUF(0, ECP_MTYPE_OPEN_REP, conn)];
        unsigned char ctype = 0;
        ssize_t _rv;

        packet.buffer = pkt_buf;
        packet.size = ECP_SIZE_PKT_BUF(0, ECP_MTYPE_OPEN_REP, conn);
        payload.buffer = pld_buf;
        payload.size = ECP_SIZE_PLD_BUF(0, ECP_MTYPE_OPEN_REP, conn);

        ctype = msg[0];
        ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_OPEN_REP);
        _rv = ecp_pld_send(conn, &packet, &payload, ECP_SIZE_PLD(0, ECP_MTYPE_OPEN_REP), 0);

        return 1;
    }
    return ECP_ERR;
}

ssize_t ecp_conn_handle_kget(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    if (mtype & ECP_MTYPE_FLAG_REP) {
        ECPContext *ctx = conn->sock->ctx;
        int rv;
        int is_open;

        if (!conn->out) return ECP_ERR;

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&conn->mutex);
#endif
        is_open = ecp_conn_is_open(conn);
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn->mutex);
#endif

        if ((size < 0) && !is_open) {
            ecp_conn_handler_msg_t handler = ctx->handler[conn->type] ? ctx->handler[conn->type]->msg[ECP_MTYPE_OPEN] : NULL;
            return handler ? handler(conn, seq, mtype, msg, size, b) : size;
        }

        if (size < 0) return size;
        if (size < ECP_ECDH_SIZE_KEY+1) return ECP_ERR;

        rv = ecp_conn_dhkey_new_pub(conn, msg[0], msg+1);
        if (!rv && !is_open) {
            ecp_conn_open_t conn_open = ctx->handler[conn->type] ? ctx->handler[conn->type]->conn_open : NULL;
            if (conn_open) {
                ssize_t _rv;

                _rv = conn_open(conn);
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
        unsigned char *buf;
        int rv;
        ssize_t _rv;

        packet.buffer = pkt_buf;
        packet.size = ECP_SIZE_PKT_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP, conn);
        payload.buffer = pld_buf;
        payload.size = ECP_SIZE_PLD_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP, conn);

        ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_KGET_REP);
        buf= ecp_pld_get_buf(payload.buffer, payload.size);

        rv = ecp_conn_dhkey_get_curr(conn, buf, buf+1);
        if (rv) return rv;
        _rv = ecp_pld_send(conn, &packet, &payload, ECP_SIZE_PLD(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP), 0);

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
        int rv;
        ssize_t _rv;

        packet.buffer = pkt_buf;
        packet.size = ECP_SIZE_PKT_BUF(0, ECP_MTYPE_KPUT_REP, conn);
        payload.buffer = pld_buf;
        payload.size = ECP_SIZE_PLD_BUF(0, ECP_MTYPE_KPUT_REP, conn);

        rv = ecp_conn_dhkey_new_pub(conn, msg[0], msg+1);
        if (rv) return rv;

        ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_KPUT_REP);
        _rv = ecp_pld_send(conn, &packet, &payload, ECP_SIZE_PLD(0, ECP_MTYPE_KPUT_REP), 0);

        return ECP_ECDH_SIZE_KEY+1;
    }
    return ECP_ERR;
}

#ifdef ECP_MEM_TINY
/* Memory limited version */

ssize_t ecp_conn_handle_exec(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    if (size < 0) return size;
    if (b == NULL) return ECP_ERR;
    if (b->packet->buffer == NULL) return ECP_ERR;

    memcpy(b->packet->buffer, msg, size);
    return ecp_pkt_handle(conn->sock, NULL, conn, b, size);
}

#else

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

#endif

ssize_t ecp_conn_handle_msg(ECPConnection *conn, ecp_seq_t seq, unsigned char *msg, size_t msg_size, ECP2Buffer *bufs) {
    ecp_conn_handler_msg_t handler = NULL;
    unsigned char mtype = 0;
    unsigned char *content = NULL;
    size_t rem_size = msg_size;
    ssize_t rv;
    int _rv;

    while (rem_size) {
        _rv = ecp_msg_get_type(msg, rem_size, &mtype);
        if (_rv) return ECP_ERR_MIN_MSG;
        if ((mtype & ECP_MTYPE_MASK) >= ECP_MAX_MTYPE) return ECP_ERR_MAX_MTYPE;

        ecp_timer_pop(conn, mtype);

        if (mtype & ECP_MTYPE_FLAG_FRAG) {
#ifdef ECP_WITH_RBUF
            if (conn->rbuf.recv && conn->rbuf.recv->frag_iter) {
                _rv = ecp_msg_defrag(conn->rbuf.recv->frag_iter, seq, mtype, msg, msg_size, &msg, &rem_size);
                if (_rv == ECP_ITER_NEXT) break;
                if (_rv < 0) return _rv;
            } else {
                return ECP_ERR_ITER;
            }
#endif
        }

        content = ecp_msg_get_content(msg, rem_size);
        if (content == NULL) return ECP_ERR_MIN_MSG;
        rem_size -= content - msg;

        handler = conn->sock->ctx->handler[conn->type] ? conn->sock->ctx->handler[conn->type]->msg[mtype & ECP_MTYPE_MASK] : NULL;
        if (handler) {
            rv = handler(conn, seq, mtype, content, rem_size, bufs);
            if (rv < 0) return rv;
            if (rv > rem_size) return ECP_ERR;
            if (mtype & ECP_MTYPE_FLAG_FRAG) break;

            rem_size -= rv;
            msg = content + rv;
        } else {
            return msg_size - rem_size - 1;
        }
    }

    return msg_size;
}

int ecp_sock_handle_open(ECPSocket *sock, ECPNetAddr *addr, ECPConnection *parent, unsigned char *msg, size_t msg_size, ECPPktMeta *pkt_meta, ECP2Buffer *bufs, ECPConnection **_conn) {
    ECPConnection *conn = NULL;
    ecp_conn_create_t handle_create = NULL;
    ecp_conn_destroy_t handle_destroy = NULL;
    unsigned char ctype;
    unsigned char c_idx = pkt_meta->c_idx;
    unsigned char s_idx = pkt_meta->s_idx;
    int rv;

    if (msg_size < 1) return ECP_ERR;

    ctype = msg[0];

    conn = sock->ctx->conn_alloc ? sock->ctx->conn_alloc(ctype) : NULL;
    if (conn == NULL) return ECP_ERR_ALLOC;

    rv = ecp_conn_create(conn, sock, ctype);
    if (rv) {
        if (sock->ctx->conn_free) sock->ctx->conn_free(conn);
        return rv;
    }
    rv = sock->ctx->rng(&conn->seq_out, sizeof(conn->seq_out));
    if (rv) {
        ecp_conn_destroy(conn);
        if (sock->ctx->conn_free) sock->ctx->conn_free(conn);
        return rv;
    }

    conn->refcount = 1;
    conn->parent = parent;
    conn->pcount = parent ? parent->pcount+1 : 0;
    handle_create = conn->sock->ctx->handler[conn->type] ? conn->sock->ctx->handler[conn->type]->conn_create : NULL;
    handle_destroy = conn->sock->ctx->handler[conn->type] ? conn->sock->ctx->handler[conn->type]->conn_destroy : NULL;

    if (handle_create) rv = handle_create(conn, msg+1, msg_size-1);
    if (rv) {
        ecp_conn_destroy(conn);
        if (sock->ctx->conn_free) sock->ctx->conn_free(conn);
        return rv;
    }

    rv = conn_dhkey_new_pub_remote(conn, c_idx, ecp_cr_dh_pub_get_buf(&pkt_meta->public));
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
    return ECP_OK;
}

int ecp_sock_handle_kget(ECPSocket *sock, ECPNetAddr *addr, ECPConnection *parent, unsigned char *msg, size_t msg_size, ECPPktMeta *pkt_meta, ECP2Buffer *bufs, ECPConnection **_conn) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF_TR(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP, parent)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF_TR(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP, parent)];
    unsigned char *buf;
    int rv;
    ssize_t _rv;

    packet.buffer = pkt_buf;
    packet.size = ECP_SIZE_PKT_BUF_TR(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP, parent);
    payload.buffer = pld_buf;
    payload.size = ECP_SIZE_PLD_BUF_TR(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP, parent);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_KGET_REP);
    buf = ecp_pld_get_buf(payload.buffer, payload.size);

    rv = ecp_sock_dhkey_get_curr(sock, buf, buf+1);
    if (rv) return rv;

    _rv = ecp_pld_send_tr(sock, addr, parent, &packet, pkt_meta, &payload, ECP_SIZE_PLD(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_KGET_REP), 0);
    if (_rv < 0) return _rv;

    return ECP_OK;
}


ssize_t _ecp_pack(ECPContext *ctx, unsigned char *packet, size_t pkt_size, ECPPktMeta *pkt_meta, unsigned char *payload, size_t pld_size) {
    ssize_t rv;
    unsigned char s_idx, c_idx;

    if (pkt_size < ECP_SIZE_PKT_HDR) return ECP_ERR;

    // ECP_SIZE_PROTO
    packet[0] = 0;
    packet[1] = 0;
    s_idx = pkt_meta->s_idx & 0x0F;
    c_idx = pkt_meta->c_idx & 0x0F;
    packet[ECP_SIZE_PROTO] = (s_idx << 4) | c_idx;
    ecp_cr_dh_pub_to_buf(packet+ECP_SIZE_PROTO+1, &pkt_meta->public);
    memcpy(packet+ECP_SIZE_PROTO+1+ECP_ECDH_SIZE_KEY, pkt_meta->nonce, ECP_AEAD_SIZE_NONCE);

    payload[0] = (pkt_meta->seq & 0xFF000000) >> 24;
    payload[1] = (pkt_meta->seq & 0x00FF0000) >> 16;
    payload[2] = (pkt_meta->seq & 0x0000FF00) >> 8;
    payload[3] = (pkt_meta->seq & 0x000000FF);
    rv = ecp_cr_aead_enc(packet+ECP_SIZE_PKT_HDR, pkt_size-ECP_SIZE_PKT_HDR, payload, pld_size, &pkt_meta->shsec, pkt_meta->nonce);
    if (rv < 0) return ECP_ERR_ENCRYPT;

    memcpy(pkt_meta->nonce, packet+ECP_SIZE_PKT_HDR, ECP_AEAD_SIZE_NONCE);

    return rv+ECP_SIZE_PKT_HDR;
}

#ifndef ECP_WITH_VCONN
ssize_t ecp_pack(ECPContext *ctx, ECPConnection *parent, ECPBuffer *packet, ECPPktMeta *pkt_meta, ECPBuffer *payload, size_t pld_size, ECPNetAddr *addr) {
    if ((packet == NULL) || (packet->buffer == NULL)) return ECP_ERR;
    if ((payload == NULL) || (payload->buffer == NULL)) return ECP_ERR;

    return _ecp_pack(ctx, packet->buffer, packet->size, pkt_meta, payload->buffer, pld_size);
}
#endif

ssize_t _ecp_pack_conn(ECPConnection *conn, unsigned char *packet, size_t pkt_size, unsigned char s_idx, unsigned char c_idx, ECPSeqItem *si, unsigned char *payload, size_t pld_size, ECPNetAddr *addr) {
    ECPPktMeta pkt_meta;
    int rv;
    ssize_t _rv;

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
    rv = conn_shsec_get(conn, s_idx, c_idx, &pkt_meta.shsec);
    if (!rv) memcpy(pkt_meta.nonce, conn->nonce, sizeof(pkt_meta.nonce));
    if (!rv) {
        if (conn->out) {
            ECPDHKey *key = conn_dhkey_get(conn, c_idx);

            if ((key == NULL) || !key->valid) rv = ECP_ERR_ECDH_IDX;
            if (!rv) memcpy(&pkt_meta.public, &key->public, sizeof(pkt_meta.public));
        } else {
            memcpy(&pkt_meta.public, &conn->remote.key[conn->remote.key_curr].public, sizeof(pkt_meta.public));
        }
    }
    if (!rv) {
        if (si) {
            if (si->seq_w) {
                pkt_meta.seq = si->seq;
            } else {
                pkt_meta.seq = conn->seq_out + 1;
                si->seq = pkt_meta.seq;
            }

#ifdef ECP_WITH_RBUF
            if (conn->rbuf.send) {
                unsigned char mtype;

                rv = ecp_pld_get_type(payload, pld_size, &mtype);
                if (!rv) rv = ecp_rbuf_pkt_prep(conn->rbuf.send, si, mtype);
            }
#endif

            if (!rv && !si->seq_w) conn->seq_out = pkt_meta.seq;
        } else {
            pkt_meta.seq = conn->seq_out + 1;
            conn->seq_out = pkt_meta.seq;
        }
        if (!rv && addr) *addr = conn->node.addr;
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    if (rv) return rv;

    pkt_meta.s_idx = s_idx;
    pkt_meta.c_idx = c_idx;
    _rv = _ecp_pack(conn->sock->ctx, packet, pkt_size, &pkt_meta, payload, pld_size);
    if (_rv < 0) return _rv;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif
    memcpy(conn->nonce, pkt_meta.nonce, sizeof(conn->nonce));
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    return _rv;
}

#ifndef ECP_WITH_VCONN
ssize_t ecp_pack_conn(ECPConnection *conn, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, ECPSeqItem *si, ECPBuffer *payload, size_t pld_size, ECPNetAddr *addr) {
    if ((packet == NULL) || (packet->buffer == NULL)) return ECP_ERR;
    if ((payload == NULL) || (payload->buffer == NULL)) return ECP_ERR;

    return _ecp_pack_conn(conn, packet->buffer, packet->size, s_idx, c_idx, si, payload->buffer, pld_size, addr);
}
#endif

ssize_t ecp_unpack(ECPSocket *sock, ECPNetAddr *addr, ECPConnection *parent, ECP2Buffer *bufs, size_t pkt_size, ECPConnection **_conn, ecp_seq_t *_seq) {
    unsigned char s_idx;
    unsigned char c_idx;
    unsigned char l_idx = ECP_ECDH_IDX_INV;
    unsigned char *payload = bufs->payload->buffer;
    unsigned char *packet = bufs->packet->buffer;
    size_t pld_size = bufs->payload->size;
    ssize_t dec_size;
    ecp_aead_key_t shsec;
    ecp_dh_public_t public;
    ecp_dh_private_t private;
    unsigned char *public_buf;
    unsigned char *nonce;
    unsigned char nonce_next[ECP_AEAD_SIZE_NONCE];
    ECPConnection *conn = NULL;
    ECPDHKey *key = NULL;
    unsigned char mtype;
    unsigned char is_open = 0;
    unsigned char seq_check;
    unsigned char seq_reset;
    ecp_seq_t seq_pkt, seq_conn, seq_last;
    ecp_ack_t seq_map;
    int rv = ECP_OK;

    *_conn = NULL;
    *_seq = 0;

    s_idx = (packet[ECP_SIZE_PROTO] & 0xF0) >> 4;
    c_idx = (packet[ECP_SIZE_PROTO] & 0x0F);

    public_buf = packet+ECP_SIZE_PROTO+1;
    nonce = packet+ECP_SIZE_PROTO+1+ECP_ECDH_SIZE_KEY;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn.mutex);
#endif

    conn = ctable_search(sock, c_idx, public_buf, NULL);
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
            seq_conn = conn->seq_in;
            seq_map = conn->seq_in_map;
        }
    }

#ifdef ECP_WITH_PTHREAD
    if (conn) pthread_mutex_unlock(&conn->mutex);
#endif

    if (rv) return rv;

    if (key) {
        ecp_cr_dh_pub_from_buf(&public, public_buf);
        ecp_cr_dh_shsec(&shsec, &public, &private);
        memset(&private, 0, sizeof(private));
    }

    dec_size = ecp_cr_aead_dec(payload, pld_size, packet+ECP_SIZE_PKT_HDR, pkt_size-ECP_SIZE_PKT_HDR, &shsec, nonce);
    if (dec_size < ECP_SIZE_PLD_HDR+1) rv = ECP_ERR_DECRYPT;
    if (rv) goto ecp_unpack_err;

    seq_pkt = \
        (payload[0] << 24) | \
        (payload[1] << 16) | \
        (payload[2] << 8)  | \
        (payload[3]);

    mtype = payload[ECP_SIZE_PLD_HDR];
    memcpy(nonce_next, packet+ECP_SIZE_PKT_HDR, sizeof(nonce_next));
    // XXX!
    // if ((mtype & ECP_MTYPE_MASK) < ECP_MAX_MTYPE_SYS) ecp_tr_release(bufs->packet, 1);
    if (conn == NULL) {
        ECPPktMeta pkt_meta;
        unsigned char *msg = payload+ECP_SIZE_PLD_HDR+1;
        size_t msg_size = dec_size-ECP_SIZE_PLD_HDR-1;
        ecp_sock_handler_msg_t handler;

        if (key == NULL) return ECP_ERR;
        if ((mtype & ECP_MTYPE_MASK) >= ECP_MAX_MTYPE_SOCK) return ECP_ERR_MAX_MTYPE;
        if (mtype & ECP_MTYPE_FLAG_REP) return ECP_ERR;

        memcpy(&pkt_meta.public, &public, sizeof(pkt_meta.public));
        memcpy(&pkt_meta.shsec, &shsec, sizeof(pkt_meta.shsec));
        memcpy(pkt_meta.nonce, nonce_next, sizeof(pkt_meta.nonce));
        pkt_meta.seq = seq_pkt;
        pkt_meta.s_idx = s_idx;
        pkt_meta.c_idx = c_idx;

        handler = sock->handler[mtype & ECP_MTYPE_MASK];
        if (handler) {
            rv = handler(sock, addr, parent, msg, msg_size, &pkt_meta, bufs, &conn);
            if (rv) return rv;
        }
    }

    if (conn == NULL) return dec_size;

    seq_check = 1;
    seq_reset = 0;

    if (is_open) {
#ifdef ECP_WITH_RBUF
        if (conn->rbuf.recv || (mtype == ECP_MTYPE_RBACK) || (mtype == ECP_MTYPE_RBFLUSH)) {
            seq_check = 0;
        }
#endif

        if (seq_check) {
            if (ECP_SEQ_LTE(seq_pkt, seq_conn)) {
                ecp_seq_t seq_offset = seq_conn - seq_pkt;
                if (seq_offset < ECP_SIZE_ACKB) {
                    ecp_ack_t ack_mask = ((ecp_ack_t)1 << seq_offset);
                    if (ack_mask & seq_map) rv = ECP_ERR_SEQ;
                    if (!rv) seq_last = seq_conn;
                } else {
                    rv = ECP_ERR_SEQ;
                }
            } else {
                ecp_seq_t seq_offset = seq_pkt - seq_conn;
                if (seq_offset < ECP_MAX_SEQ_FWD) {
                    if (seq_offset < ECP_SIZE_ACKB) {
                        seq_map = seq_map << seq_offset;
                    } else {
                        seq_map = 0;
                    }
                    seq_map |= 1;
                    seq_last = seq_pkt;
                } else {
                    rv = ECP_ERR_SEQ;
                }
            }

            if ((rv == ECP_ERR_SEQ) && (mtype == ECP_MTYPE_OPEN_REQ) && key) {
                rv = ECP_OK;
                seq_reset = 1;
            }
            if (rv) goto ecp_unpack_err;
        }
    }

    if (!is_open || seq_reset) {
        seq_last = seq_pkt;
        seq_map = 1;

#ifdef ECP_WITH_RBUF
        if (conn->rbuf.recv) {
            rv = ecp_rbuf_recv_start(conn, seq_pkt);
            if (rv) goto ecp_unpack_err;
        }
#endif
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif
    memcpy(conn->nonce, nonce_next, sizeof(conn->nonce));
    if (addr) conn->node.addr = *addr;
    if (seq_check) {
        conn->seq_in = seq_last;
        conn->seq_in_map = seq_map;
    }
    if (key) {
        if (!rv) rv = conn_dhkey_new_pub_local(conn, l_idx);
        if (!rv) rv = conn_shsec_set(conn, s_idx, c_idx, &shsec);
    }
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    if (rv) goto ecp_unpack_err;

    *_conn = conn;
    *_seq = seq_pkt;
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
    ssize_t pld_size;
    ssize_t rv = 0;

    pld_size = ecp_unpack(sock, addr, parent, bufs, pkt_size, &conn, &seq);
    if (pld_size < 0) return pld_size;
    if (pld_size < ECP_SIZE_PLD_HDR) return ECP_ERR;

    if (conn) {
#ifdef ECP_WITH_RBUF
        if (conn->rbuf.recv) {
            rv = ecp_rbuf_recv_store(conn, seq, bufs->payload->buffer+ECP_SIZE_PLD_HDR, pld_size-ECP_SIZE_PLD_HDR, bufs);
        }
#endif
        if (rv == 0) rv = ecp_conn_handle_msg(conn, seq, bufs->payload->buffer+ECP_SIZE_PLD_HDR, pld_size-ECP_SIZE_PLD_HDR, bufs);

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
    ssize_t rv;

    rv = ecp_tr_send(sock, packet, pkt_size, addr, flags);
    if (rv < 0) return rv;
    if (rv < ECP_MIN_PKT) return ECP_ERR_SEND;

    return rv;
}

int ecp_msg_get_type(unsigned char *msg, size_t msg_size, unsigned char *mtype) {
    if (msg_size == 0) ECP_ERR;

    *mtype = msg[0];
    return ECP_OK;
}

int ecp_msg_set_type(unsigned char *msg, size_t msg_size, unsigned char type) {
    if (msg_size == 0) ECP_ERR;

    msg[0] = type;
    return ECP_OK;
}

int ecp_msg_get_frag(unsigned char *msg, size_t msg_size, unsigned char *frag_cnt, unsigned char *frag_tot, uint16_t *frag_size) {
    if (msg_size < 3) return ECP_ERR;
    if (!(msg[0] & ECP_MTYPE_FLAG_FRAG)) return ECP_ERR;
    if (msg[2] == 0) return ECP_ERR;

    *frag_cnt = msg[1];
    *frag_tot = msg[2];
    *frag_size = \
        (msg[3] << 8) | \
        (msg[4]);

    return ECP_OK;
}

int ecp_msg_set_frag(unsigned char *msg, size_t msg_size, unsigned char frag_cnt, unsigned char frag_tot, uint16_t frag_size) {
    if (msg_size < 3) return ECP_ERR;
    if (!(msg[0] & ECP_MTYPE_FLAG_FRAG)) return ECP_ERR;

    msg[1] = frag_cnt;
    msg[2] = frag_tot;
    msg[3] = (frag_size & 0xFF00) >> 8;
    msg[4] = (frag_size & 0x00FF);

    return ECP_OK;
}

int ecp_msg_get_pts(unsigned char *msg, size_t msg_size, ecp_pts_t *pts) {
    unsigned char mtype;
    size_t offset;

    if (msg_size == 0) ECP_ERR;

    mtype = msg[0];
    if (!(mtype & ECP_MTYPE_FLAG_PTS)) return ECP_ERR;

    offset = 1 + ECP_SIZE_MT_FRAG(mtype);
    if (msg_size < offset + sizeof(ecp_pts_t)) return ECP_ERR;

    *pts = \
        (msg[offset] << 24)     | \
        (msg[offset + 1] << 16) | \
        (msg[offset + 2] << 8)  | \
        (msg[offset + 3]);

    return ECP_OK;
}

int ecp_msg_set_pts(unsigned char *msg, size_t msg_size, ecp_pts_t pts) {
    unsigned char mtype;
    size_t offset;

    if (msg_size == 0) ECP_ERR;

    mtype = msg[0];
    if (!(mtype & ECP_MTYPE_FLAG_PTS)) return ECP_ERR;

    offset = 1 + ECP_SIZE_MT_FRAG(mtype);
    if (msg_size < offset + sizeof(ecp_pts_t)) return ECP_ERR;

    msg[offset]     = (pts & 0xFF000000) >> 24;
    msg[offset + 1] = (pts & 0x00FF0000) >> 16;
    msg[offset + 2] = (pts & 0x0000FF00) >> 8;
    msg[offset + 3] = (pts & 0x000000FF);

    return ECP_OK;
}

unsigned char *ecp_msg_get_content(unsigned char *msg, size_t msg_size) {
    unsigned char mtype;
    size_t offset;

    if (msg_size == 0) ECP_ERR;

    mtype = msg[0];
    offset = 1 + ECP_SIZE_MT_FLAG(mtype);
    if (msg_size < offset) return NULL;

    return msg + offset;
}

int ecp_msg_defrag(ECPFragIter *iter, ecp_seq_t seq, unsigned char mtype, unsigned char *msg_in, size_t msg_in_size, unsigned char **msg_out, size_t *msg_out_size) {
    unsigned char *content;
    unsigned char frag_cnt, frag_tot;
    uint16_t frag_size;
    size_t msg_size;
    size_t buf_offset;
    int rv;

    rv = ecp_msg_get_frag(msg_in, msg_in_size, &frag_cnt, &frag_tot, &frag_size);
    if (rv) return ECP_ERR_MIN_MSG;

    content = ecp_msg_get_content(msg_in, msg_in_size);
    if (content == NULL) return ECP_ERR_MIN_MSG;

    msg_size = msg_in_size - (content - msg_in);
    if (msg_size == 0) return ECP_ERR_MIN_MSG;

    if (iter->msg_size && (iter->seq + frag_cnt != seq)) ecp_frag_iter_reset(iter);

    if (iter->msg_size == 0) {
        iter->seq = seq - frag_cnt;
        iter->frag_cnt = 0;
    }

    mtype &= (~ECP_MTYPE_FLAG_FRAG);
    buf_offset = 1 + ECP_SIZE_MT_FLAG(mtype) + frag_size * frag_cnt;
    if (buf_offset + msg_size > iter->buf_size) return ECP_ERR_SIZE;
    memcpy(iter->buffer + buf_offset, content, msg_size);

    if (frag_cnt == 0) {
        if (1 + ECP_SIZE_MT_FLAG(mtype) > iter->buf_size) return ECP_ERR_SIZE;

        iter->buffer[0] = mtype;
        if (ECP_SIZE_MT_FLAG(mtype)) {
            memcpy(iter->buffer + 1, msg_in + 1, ECP_SIZE_MT_FLAG(mtype));
        }
        msg_size += 1 + ECP_SIZE_MT_FLAG(mtype);
    }

    iter->frag_cnt++;
    iter->msg_size += msg_size;
    if (iter->frag_cnt == frag_tot) {
        *msg_out = iter->buffer;
        *msg_out_size = iter->msg_size;
        return ECP_OK;
    } else {
        return ECP_ITER_NEXT;
    }
}

int ecp_pld_get_type(unsigned char *payload, size_t pld_size, unsigned char *mtype) {
    payload += ECP_SIZE_PLD_HDR;
    pld_size -= ECP_SIZE_PLD_HDR;

    return ecp_msg_get_type(payload, pld_size, mtype);
}

int ecp_pld_set_type(unsigned char *payload, size_t pld_size, unsigned char mtype) {
    payload += ECP_SIZE_PLD_HDR;
    pld_size -= ECP_SIZE_PLD_HDR;

    return ecp_msg_set_type(payload, pld_size, mtype);
}

int ecp_pld_get_frag(unsigned char *payload, size_t pld_size, unsigned char *frag_cnt, unsigned char *frag_tot, uint16_t *frag_size) {
    payload += ECP_SIZE_PLD_HDR;
    pld_size -= ECP_SIZE_PLD_HDR;

    return ecp_msg_get_frag(payload, pld_size, frag_cnt, frag_tot, frag_size);
}

int ecp_pld_set_frag(unsigned char *payload, size_t pld_size, unsigned char frag_cnt, unsigned char frag_tot, uint16_t frag_size) {
    payload += ECP_SIZE_PLD_HDR;
    pld_size -= ECP_SIZE_PLD_HDR;

    return ecp_msg_set_frag(payload, pld_size, frag_cnt, frag_tot, frag_size);
}

int ecp_pld_get_pts(unsigned char *payload, size_t pld_size, ecp_pts_t *pts) {
    payload += ECP_SIZE_PLD_HDR;
    pld_size -= ECP_SIZE_PLD_HDR;

    return ecp_msg_get_pts(payload, pld_size, pts);
}

int ecp_pld_set_pts(unsigned char *payload, size_t pld_size, ecp_pts_t pts) {
    payload += ECP_SIZE_PLD_HDR;
    pld_size -= ECP_SIZE_PLD_HDR;

    return ecp_msg_set_pts(payload, pld_size, pts);
}

unsigned char *ecp_pld_get_buf(unsigned char *payload, size_t pld_size) {
    payload += ECP_SIZE_PLD_HDR;
    pld_size -= ECP_SIZE_PLD_HDR;

    return ecp_msg_get_content(payload, pld_size);
}

static ssize_t pld_send(ECPConnection *conn, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, ECPSeqItem *si, ECPBuffer *payload, size_t pld_size, unsigned char flags, ECPTimerItem *ti) {
    ECPSocket *sock = conn->sock;
    ECPNetAddr addr;
    ssize_t rv;

    rv = ecp_pack_conn(conn, packet, s_idx, c_idx, si, payload, pld_size, &addr);
    if (rv < 0) return rv;

#ifdef ECP_WITH_RBUF
    if (conn->rbuf.send) {
        return ecp_rbuf_pkt_send(conn->rbuf.send, conn->sock, &addr, packet, rv, flags, ti, si);
    }
#endif

    if (ti) {
        int _rv;

        _rv = ecp_timer_push(ti);
        if (_rv) return _rv;
    }

    return ecp_pkt_send(sock, &addr, packet, rv, flags);
}

ssize_t _ecp_pld_send(ECPConnection *conn, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, ECPSeqItem *si, ECPBuffer *payload, size_t pld_size, unsigned char flags, ECPTimerItem *ti) {
#ifdef ECP_WITH_RBUF
    if ((si == NULL) && conn->rbuf.send) {
        ECPSeqItem seq_item;
        int rv;

        rv = ecp_seq_item_init(&seq_item);
        if (rv) return rv;

        return pld_send(conn, packet, s_idx, c_idx, &seq_item, payload, pld_size, flags, ti);
    }
#endif

    return pld_send(conn, packet, s_idx, c_idx, NULL, payload, pld_size, flags, ti);
}

ssize_t ecp_pld_send(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pld_size, unsigned char flags) {
    return _ecp_pld_send(conn, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, NULL, payload, pld_size, flags, NULL);
}

ssize_t ecp_pld_send_wtimer(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pld_size, unsigned char flags, ECPTimerItem *ti) {
    return _ecp_pld_send(conn, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, NULL, payload, pld_size, flags, ti);
}

ssize_t ecp_pld_send_tr(ECPSocket *sock, ECPNetAddr *addr, ECPConnection *parent, ECPBuffer *packet, ECPPktMeta *pkt_meta, ECPBuffer *payload, size_t pld_size, unsigned char flags) {
    ECPNetAddr _addr;
    ssize_t rv;

    rv = ecp_pack(sock->ctx, parent, packet, pkt_meta, payload, pld_size, &_addr);
    if (rv < 0) return rv;

    return ecp_pkt_send(sock, addr ? addr : &_addr, packet, rv, flags);
}

ssize_t ecp_send(ECPConnection *conn, unsigned char mtype, unsigned char *content, size_t content_size) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_MAX_PKT];
    unsigned char pld_buf[ECP_MAX_PLD];
    unsigned char *content_buf;
    ssize_t rv = 0;
    int pkt_cnt = 0;
    int vc_cnt = conn->pcount;
    size_t pld_max = ECP_MAX_PKT - (ECP_SIZE_PKT_HDR + ECP_AEAD_SIZE_TAG + ECP_SIZE_PLD_HDR + 1) * vc_cnt - (ECP_SIZE_PKT_HDR + ECP_AEAD_SIZE_TAG);

    packet.buffer = pkt_buf;
    packet.size = ECP_MAX_PKT;
    payload.buffer = pld_buf;
    payload.size = ECP_MAX_PLD;

    if (ECP_SIZE_PLD(content_size, mtype) > pld_max) {
        size_t frag_size, frag_size_final;
        ecp_seq_t seq_start;
        ECPSeqItem seq_item;
        int i;
        int _rv;

        _rv = ecp_seq_item_init(&seq_item);
        if (_rv) return _rv;

        mtype |= ECP_MTYPE_FLAG_FRAG;
        frag_size = pld_max - ECP_SIZE_PLD(0, mtype);
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
            ssize_t _rv;

            ecp_pld_set_type(pld_buf, ECP_MAX_PLD, mtype);
            ecp_pld_set_frag(pld_buf, ECP_MAX_PLD, i, pkt_cnt, frag_size);
            content_buf = ecp_pld_get_buf(pld_buf, ECP_MAX_PLD);

            if ((i == pkt_cnt - 1) && frag_size_final) frag_size = frag_size_final;
            memcpy(content_buf, content, frag_size);
            content += frag_size;
            seq_item.seq = seq_start + i;

            _rv = _ecp_pld_send(conn, &packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, &seq_item, &payload, ECP_SIZE_PLD(frag_size, mtype), 0, NULL);
            if (_rv < 0) return _rv;

            rv += _rv;
        }
    } else {
        ecp_pld_set_type(pld_buf, ECP_MAX_PLD, mtype);
        content_buf = ecp_pld_get_buf(pld_buf, ECP_MAX_PLD);

        memcpy(content_buf, content, content_size);
        rv = ecp_pld_send(conn, &packet, &payload, ECP_SIZE_PLD(content_size, mtype), 0);
    }
    return rv;
}

#if defined(ECP_WITH_RBUF) && defined(ECP_WITH_MSGQ)
ssize_t ecp_receive(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, ecp_cts_t timeout) {
    ssize_t rv;

    pthread_mutex_lock(&conn->rbuf.recv->msgq.mutex);
    rv = ecp_conn_msgq_pop(conn, mtype, msg, msg_size, timeout);
    pthread_mutex_unlock(&conn->rbuf.recv->msgq.mutex);

    return rv;
}
#else
ssize_t ecp_receive(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, ecp_cts_t timeout) {
    return ECP_ERR_NOT_IMPLEMENTED;
}
#endif

static int recv_p(ECPSocket *sock, ECPNetAddr *addr, ECPBuffer *packet, size_t size) {
    ECP2Buffer bufs;
    ECPBuffer payload;
    unsigned char pld_buf[ECP_MAX_PLD];
    ssize_t rv;

    bufs.packet = packet;
    bufs.payload = &payload;

    payload.buffer = pld_buf;
    payload.size = ECP_MAX_PLD;

    rv = ecp_pkt_handle(sock, addr, NULL, &bufs, size);
    if (rv < 0) return rv;

    return ECP_OK;
}

int ecp_receiver(ECPSocket *sock) {
    ECPNetAddr addr;
    ECPBuffer packet;
    unsigned char pkt_buf[ECP_MAX_PKT];
    ecp_cts_t next = 0;
    ssize_t rv;
    int _rv;

    sock->running = 1;
    while(sock->running) {
        packet.buffer = pkt_buf;
        packet.size = ECP_MAX_PKT;

        rv = ecp_tr_recv(sock, &packet, &addr, next ? next : sock->poll_timeout);
        if (rv > 0) {
            _rv = recv_p(sock, &addr, &packet, rv);
#ifdef ECP_DEBUG
            if (_rv) {
                printf("RECEIVER ERR:%d\n", _rv);
            }
#endif
        }
        next = ecp_timer_exe(sock);
    }

    return ECP_OK;
}

#ifdef ECP_WITH_PTHREAD
static void *_ecp_receiver(void *arg) {
    ecp_receiver((ECPSocket *)arg);
    return NULL;
}

int ecp_start_receiver(ECPSocket *sock) {
    int rv;

    rv = pthread_create(&sock->rcvr_thd, NULL, _ecp_receiver, sock);
    if (rv) return ECP_ERR;
    return ECP_OK;
}

int ecp_stop_receiver(ECPSocket *sock) {
    int rv;

    sock->running = 0;
    rv = pthread_join(sock->rcvr_thd, NULL);
    if (rv) return ECP_ERR;
    return ECP_OK;
}
#else
int ecp_start_receiver(ECPSocket *sock) {
    return ECP_ERR_NOT_IMPLEMENTED;
}

int ecp_stop_receiver(ECPSocket *sock) {
    return ECP_ERR_NOT_IMPLEMENTED;
}
#endif
