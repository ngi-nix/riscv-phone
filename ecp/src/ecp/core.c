#include <stdlib.h>
#include <string.h>

#ifdef ECP_DEBUG
#include <stdio.h>
#endif

#include "core.h"
#include "ext.h"

#ifdef ECP_WITH_VCONN
#include "vconn/vconn.h"
#endif

#ifdef ECP_WITH_DIR
#include "dir/dir.h"
#endif

#include "cr.h"
#include "tr.h"
#include "tm.h"

#ifdef ECP_WITH_HTABLE
#include "ht.h"
#endif


int ecp_ctx_init(ECPContext *ctx, ecp_err_handler_t handle_err, ecp_dir_handler_t handle_dir, ecp_conn_alloc_t conn_alloc, ecp_conn_free_t conn_free) {
    int rv;

    if (ctx == NULL) return ECP_ERR;

    memset(ctx, 0, sizeof(ECPContext));
    ctx->handle_err = handle_err;
    ctx->handle_dir = handle_dir;
    ctx->conn_alloc = conn_alloc;
    ctx->conn_free = conn_free;

    rv = ecp_tr_init(ctx);
    if (rv) return rv;
    rv = ecp_tm_init(ctx);
    if (rv) return rv;

    return ECP_OK;
}

int ecp_ctx_set_handler(ECPContext *ctx, ECPConnHandler *handler, unsigned char ctype) {
    if (ctype >= ECP_MAX_CTYPE) return ECP_ERR_CTYPE;

    ctx->handler[ctype] = handler;
    return ECP_OK;
}

int ecp_node_init(ECPNode *node, ecp_ecdh_public_t *public, void *addr) {
    memset(node, 0, sizeof(ECPNode));

    if (public) {
        ECPDHPub *key = &node->key_perma;

        memcpy(&key->public, public, sizeof(key->public));
        key->valid = 1;
    }

    if (addr) {
        int rv;

        rv = ecp_tr_addr_set(&node->addr, addr);
        if (rv) return ECP_ERR_NET_ADDR;
    }

    return ECP_OK;
}

int ecp_dhkey_gen(ECPDHKey *key) {
    int rv;

    rv = ecp_ecdh_mkpair(&key->public, &key->private);
    if (rv) return rv;

    key->valid = 1;
    return ECP_OK;
}

static int conn_table_create(ECPConnTable *conn_table) {
    int rv;

    memset(conn_table, 0, sizeof(ECPConnTable));

#ifdef ECP_WITH_HTABLE
    conn_table->keys = ecp_ht_create_keys();
    if (conn_table->keys == NULL) return ECP_ERR_ALLOC;
    conn_table->addrs = ecp_ht_create_addrs();
    if (conn_table->addrs == NULL) {
        ecp_ht_destroy(conn_table->keys);
        return ECP_ERR_ALLOC;
    }
#endif

#ifdef ECP_WITH_PTHREAD
    rv = pthread_mutex_init(&conn_table->mutex, NULL);
    if (rv) {
#ifdef ECP_WITH_HTABLE
        ecp_ht_destroy(conn_table->addrs);
        ecp_ht_destroy(conn_table->keys);
#endif
        return ECP_ERR;
    }
#endif

    return ECP_OK;
}

static void conn_table_destroy(ECPConnTable *conn_table) {
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_destroy(&conn_table->mutex);
#endif
#ifdef ECP_WITH_HTABLE
    ecp_ht_destroy(conn_table->addrs);
    ecp_ht_destroy(conn_table->keys);
#endif
}

static int conn_table_insert(ECPConnection *conn) {
    ECPSocket *sock = conn->sock;

#ifdef ECP_WITH_HTABLE
    int i, rv = ECP_OK;

    if (ecp_conn_is_outb(conn)) {
        if (!ecp_conn_is_open(conn)) rv = ecp_ht_insert(sock->conn_table.addrs, &conn->remote.addr, conn);
        if (rv) return rv;

        for (i=0; i<ECP_MAX_CONN_KEY; i++) {
            if (conn->key[i].valid) rv = ecp_ht_insert(sock->conn_table.keys, &conn->key[i].public, conn);
            if (rv) {
                int j;

                for (j=0; j<i; j++) {
                    if (conn->key[j].valid) ecp_ht_remove(sock->conn_table.keys, &conn->key[j].public);
                }
                if (!ecp_conn_is_open(conn)) ecp_ht_remove(sock->conn_table.addrs, &conn->remote.addr);
                return rv;
            }
        }
    } else {
        for (i=0; i<ECP_MAX_NODE_KEY; i++) {
            if (conn->rkey[i].valid) rv = ecp_ht_insert(sock->conn_table.keys, &conn->rkey[i].public, conn);
            if (rv) {
                int j;

                for (j=0; j<i; j++) {
                    if (conn->rkey[j].valid) ecp_ht_remove(sock->conn_table.keys, &conn->rkey[j].public);
                }
                return rv;
            }
        }
    }
#else
    if (sock->conn_table.size == ECP_MAX_SOCK_CONN) return ECP_ERR_FULL;
    sock->conn_table.arr[sock->conn_table.size] = conn;
    sock->conn_table.size++;
#endif

    return ECP_OK;
}

static void conn_table_remove(ECPConnection *conn) {
    ECPSocket *sock = conn->sock;
    int i;

#ifdef ECP_WITH_HTABLE
    if (ecp_conn_is_outb(conn)) {
        for (i=0; i<ECP_MAX_CONN_KEY; i++) {
            if (conn->key[i].valid) ecp_ht_remove(sock->conn_table.keys, &conn->key[i].public);
        }
        if (!ecp_conn_is_open(conn)) ecp_ht_remove(sock->conn_table.addrs, &conn->remote.addr);
    } else {
        for (i=0; i<ECP_MAX_NODE_KEY; i++) {
            if (conn->rkey[i].valid) ecp_ht_remove(sock->conn_table.keys, &conn->rkey[i].public);
        }
    }
#else
    for (i=0; i<sock->conn_table.size; i++) {
        if (conn == sock->conn_table.arr[i]) {
            while (i < (sock->conn_table.size-1)) {
                sock->conn_table.arr[i] = sock->conn_table.arr[i+1];
                i++;
            }
            sock->conn_table.arr[i] = NULL;
            sock->conn_table.size--;
            return;
        }
    }
#endif
}

static void conn_table_remove_addr(ECPConnection *conn) {
    ECPSocket *sock = conn->sock;

#ifdef ECP_WITH_HTABLE
    ecp_ht_remove(sock->conn_table.addrs, &conn->remote.addr);
#endif
}

static ECPConnection *conn_table_search(ECPSocket *sock, unsigned char c_idx, ecp_ecdh_public_t *c_public, ecp_tr_addr_t *addr) {
#ifdef ECP_WITH_HTABLE
    if (c_public) {
        return ecp_ht_search(sock->conn_table.keys, c_public);
    } else if (addr) {
        return ecp_ht_search(sock->conn_table.addrs, addr);
    } else {
        return NULL;
    }
#else
    ECPConnection *conn;
    int i;

    if (c_public) {
        if (ecp_conn_is_outb(conn)) {
            if (c_idx >= ECP_MAX_CONN_KEY) return ECP_ERR_ECDH_IDX;

            for (i=0; i<sock->conn_table.size; i++) {
                conn = sock->conn_table.arr[i];
                if (conn->key[c_idx].valid && ecp_ecdh_pub_eq(c_public, &conn->key[c_idx].public)) return conn;
            }
        } else {
            unsigned char *_c_idx;

            if (c_idx & ~ECP_ECDH_IDX_MASK) return ECP_ERR_ECDH_IDX;

            _c_idx = c_idx % ECP_MAX_NODE_KEY;
            for (i=0; i<sock->conn_table.size; i++) {
                conn = sock->conn_table.arr[i];
                if (conn->rkey[_c_idx].valid && ecp_ecdh_pub_eq(c_public, &conn->rkey[_c_idx].public)) return conn;
            }
        }
    } else if (addr) {
        for (i=0; i<sock->conn_table.size; i++) {
            conn = sock->conn_table.arr[i];
            if (ecp_conn_is_outb(conn) && ecp_tr_addr_eq(&conn->remote.addr, addr)) return conn;
        }
    }

    return NULL;
#endif
}

int ecp_sock_init(ECPSocket *sock, ECPContext *ctx, ECPDHKey *key) {
    memset(sock, 0, sizeof(ECPSocket));
    sock->ctx = ctx;
    sock->key_curr = 0;
    if (key) sock->key_perma = *key;

    return ecp_dhkey_gen(&sock->key[sock->key_curr]);
}

int ecp_sock_create(ECPSocket *sock, ECPContext *ctx, ECPDHKey *key) {
    int rv;

    rv = ecp_sock_init(sock, ctx, key);
    if (rv) return rv;

    rv = conn_table_create(&sock->conn_table);
    if (rv) return rv;

    rv = ecp_timer_create(&sock->timer);
    if (rv) {
        conn_table_destroy(&sock->conn_table);
        return rv;
    }

#ifdef ECP_WITH_PTHREAD
    rv = pthread_mutex_init(&sock->mutex, NULL);
    if (rv) {
        ecp_timer_destroy(&sock->timer);
        conn_table_destroy(&sock->conn_table);
        return ECP_ERR;
    }
#endif

    arc4random_buf(&sock->nonce_out, sizeof(sock->nonce_out));
    return ECP_OK;
}

void ecp_sock_destroy(ECPSocket *sock) {
    ecp_timer_destroy(&sock->timer);
    conn_table_destroy(&sock->conn_table);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_destroy(&sock->mutex);
#endif
}

int ecp_sock_open(ECPSocket *sock, void *myaddr) {
    return ecp_tr_open(sock, myaddr);
}

void ecp_sock_close(ECPSocket *sock) {
    ecp_sock_destroy(sock);
    ecp_tr_close(sock);
}

int ecp_sock_dhkey_new(ECPSocket *sock) {
    ECPDHKey new_key;
    int rv;

    rv = ecp_dhkey_gen(&new_key);
    if (rv) return rv;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->mutex);
#endif

    sock->key_curr = (sock->key_curr + 1) % ECP_MAX_SOCK_KEY;
    sock->key[sock->key_curr] = new_key;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&sock->mutex);
#endif

    return ECP_OK;
}

int ecp_sock_dhkey_get(ECPSocket *sock, unsigned char idx, ECPDHKey *key) {
    int rv = ECP_OK;

    if (idx == ECP_ECDH_IDX_PERMA) {
        *key = sock->key_perma;
    } else {

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&sock->mutex);
#endif

        if (idx < ECP_MAX_SOCK_KEY) {
            *key = sock->key[idx];
        } else {
            rv = ECP_ERR_ECDH_IDX;
        }

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&sock->mutex);
#endif

    }

    if (!rv && !key->valid) rv = ECP_ERR_ECDH_IDX;
    return rv;
}

int ecp_sock_dhkey_get_pub(ECPSocket *sock, unsigned char *idx, ecp_ecdh_public_t *public) {
    ECPDHKey *key;
    unsigned char _idx;
    int rv = ECP_OK;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->mutex);
#endif

    _idx = sock->key_curr;
    if (_idx == ECP_ECDH_IDX_INV) rv = ECP_ERR_ECDH_IDX;

    if (!rv) {
        key = &sock->key[_idx];
        if (!key->valid) rv = ECP_ERR_ECDH_IDX;
    }
    if (!rv) memcpy(public, &key->public, sizeof(key->public));

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&sock->mutex);
#endif

    if (rv) return rv;

    if (idx) *idx = _idx;
    return ECP_OK;
}

void ecp_sock_get_nonce(ECPSocket *sock, ecp_nonce_t *nonce) {
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->mutex);
#endif

    *nonce = sock->nonce_out;
    sock->nonce_out++;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&sock->mutex);
#endif
}

ECPConnection *ecp_sock_keys_search(ECPSocket *sock, ecp_ecdh_public_t *public) {
    ECPConnection *conn;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn_table.mutex);
#endif

    conn = ecp_ht_search(sock->conn_table.keys, public);
    if (conn) ecp_conn_refcount_inc(conn);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&sock->conn_table.mutex);
#endif

    return conn;
}

int ecp_sock_keys_insert(ECPSocket *sock, ecp_ecdh_public_t *public, ECPConnection *conn) {
    int rv;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn_table.mutex);
#endif

    rv = ecp_ht_insert(sock->conn_table.keys, public, conn);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&sock->conn_table.mutex);
#endif

    return rv;
}

void ecp_sock_keys_remove(ECPSocket *sock, ecp_ecdh_public_t *public) {
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn_table.mutex);
#endif

    ecp_ht_remove(sock->conn_table.keys, public);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&sock->conn_table.mutex);
#endif
}

static int conn_dhkey_new(ECPConnection *conn, unsigned char idx, ECPDHKey *key) {
    ECPSocket *sock = conn->sock;

    if (idx >= ECP_MAX_CONN_KEY) return ECP_ERR_ECDH_IDX;

#ifdef ECP_WITH_HTABLE
    if (ecp_conn_is_outb(conn) && ecp_conn_is_reg(conn) && conn->key[idx].valid) {
        ecp_ht_remove(sock->conn_table.keys, &conn->key[idx].public);
    }
#endif

    conn->key[idx] = *key;

#ifdef ECP_WITH_HTABLE
    if (ecp_conn_is_outb(conn) && ecp_conn_is_reg(conn)) {
        int rv;

        rv = ecp_ht_insert(sock->conn_table.keys, &conn->key[idx].public, conn);
        if (rv) return rv;
    }
#endif

    return ECP_OK;
}

static void conn_dhkey_del(ECPConnection *conn, unsigned char idx) {
    ECPSocket *sock = conn->sock;

    if (idx >= ECP_MAX_CONN_KEY) return;

#ifdef ECP_WITH_HTABLE
    if (ecp_conn_is_outb(conn) && ecp_conn_is_reg(conn) && conn->key[idx].valid) {
        ecp_ht_remove(sock->conn_table.keys, &conn->key[idx].public);
    }
#endif

    memset(&conn->key[idx], 0, sizeof(conn->key[idx]));
}

static ECPDHKey *conn_dhkey_get(ECPConnection *conn, unsigned char idx) {
    ECPDHKey *key = NULL;

    /* always outbound */
    if (idx < ECP_MAX_CONN_KEY) {
        key = &conn->key[idx];
    }

    if (key && key->valid) return key;
    return NULL;
}

static ECPDHPub *conn_dhkey_get_remote(ECPConnection *conn, unsigned char idx) {
    ECPDHPub *key = NULL;

    if (ecp_conn_is_outb(conn) && (idx == ECP_ECDH_IDX_PERMA)) {
        key = &conn->remote.key_perma;
    } else if ((idx & ECP_ECDH_IDX_MASK) == idx) {
        key = &conn->rkey[idx % ECP_MAX_NODE_KEY];
    }

    if (key && key->valid) return key;
    return NULL;
}

/* node will send public key */
static void conn_dhkey_get_pub(ECPConnection *conn, unsigned char idx) {
    unsigned char _idx;
    int i;

    _idx = idx % ECP_MAX_NODE_KEY;
    if (ecp_conn_is_outb(conn)) {
        if (idx == conn->key_curr) return;

        for (i=0; i<ECP_MAX_NODE_KEY; i++) {
            conn->shkey[i][_idx].valid = 0;
        }
    } else {
        if (idx == conn->rkey_curr) return;

        for (i=0; i<ECP_MAX_NODE_KEY; i++) {
            conn->shkey[_idx][i].valid = 0;
        }
    }
}

/* node received public key */
static int conn_dhkey_set_pub(ECPConnection *conn, unsigned char idx, ecp_ecdh_public_t *public) {
    unsigned char _idx;
    ECPDHPub *key;
    ECPSocket *sock = conn->sock;
    int i;

    if (idx & ~ECP_ECDH_IDX_MASK) return ECP_ERR_ECDH_IDX;

    _idx = idx % ECP_MAX_NODE_KEY;
    key = &conn->rkey[_idx];
    if (key->valid && (memcmp(public, &key->public, sizeof(key->public)) == 0)) return ECP_ERR_ECDH_KEY_DUP;

#ifdef ECP_WITH_HTABLE
    if (ecp_conn_is_inb(conn) && ecp_conn_is_reg(conn) && (key->valid)) {
        ecp_ht_remove(sock->conn_table.keys, &key->public);
    }
#endif

    memcpy(&key->public, public, sizeof(key->public));
    key->valid = 1;

#ifdef ECP_WITH_HTABLE
    if (ecp_conn_is_inb(conn) && ecp_conn_is_reg(conn)) {
        int rv;

        rv = ecp_ht_insert(sock->conn_table.keys, &key->public, conn);
        if (rv) return rv;
    }
#endif

    if (ecp_conn_is_outb(conn)) {
        conn->rkey_curr = idx;
        for (i=0; i<ECP_MAX_NODE_KEY; i++) {
            conn->shkey[_idx][i].valid = 0;
        }
    } else {
        for (i=0; i<ECP_MAX_NODE_KEY; i++) {
            conn->shkey[i][_idx].valid = 0;
        }
    }
    return ECP_OK;
}

static int conn_shkey_get(ECPConnection *conn, unsigned char s_idx, unsigned char c_idx, ecp_aead_key_t *shkey) {
    ECPDHPub *pub;
    ECPDHKey *priv;
    int rv;

    if (ecp_conn_is_outb(conn) && (s_idx == ECP_ECDH_IDX_PERMA)) {
        pub = conn_dhkey_get_remote(conn, s_idx);
        priv = conn_dhkey_get(conn, c_idx);

        if ((pub == NULL) || (priv == NULL)) return ECP_ERR_ECDH_IDX;
        ecp_ecdh_shkey(shkey, &pub->public, &priv->private);
    } else {
        ECPDHShkey *_shkey;

        if (s_idx & ~ECP_ECDH_IDX_MASK) return ECP_ERR_ECDH_IDX;
        if (c_idx & ~ECP_ECDH_IDX_MASK) return ECP_ERR_ECDH_IDX;

        _shkey = &conn->shkey[s_idx % ECP_MAX_NODE_KEY][c_idx % ECP_MAX_NODE_KEY];
        if (!_shkey->valid) {
            if (ecp_conn_is_inb(conn)) {
                ECPSocket *sock = conn->sock;
                ECPDHKey priv;

                pub = conn_dhkey_get_remote(conn, c_idx);
                if (pub == NULL) return ECP_ERR_ECDH_IDX;

                rv = ecp_sock_dhkey_get(sock, s_idx, &priv);
                if (rv) return rv;

                ecp_ecdh_shkey(&_shkey->key, &pub->public, &priv.private);
                conn->key_curr = s_idx;
                conn->rkey_curr = c_idx;
            } else {
                pub = conn_dhkey_get_remote(conn, s_idx);
                priv = conn_dhkey_get(conn, c_idx);

                if ((pub == NULL) || (priv == NULL)) return ECP_ERR_ECDH_IDX;
                ecp_ecdh_shkey(&_shkey->key, &pub->public, &priv->private);
            }
            _shkey->valid = 1;
        }
        memcpy(shkey, &_shkey->key, sizeof(_shkey->key));
    }

    return ECP_OK;
}

static int conn_shkey_set(ECPConnection *conn, unsigned char s_idx, unsigned char c_idx, ecp_aead_key_t *shkey) {
    ECPDHShkey *_shkey;

    if (s_idx & ~ECP_ECDH_IDX_MASK) return ECP_ERR_ECDH_IDX;
    if (c_idx & ~ECP_ECDH_IDX_MASK) return ECP_ERR_ECDH_IDX;

    _shkey = &conn->shkey[s_idx % ECP_MAX_NODE_KEY][c_idx % ECP_MAX_NODE_KEY];
    memcpy(_shkey->key, shkey, sizeof(_shkey->key));
    _shkey->valid = 1;

    return ECP_OK;
}

int ecp_conn_alloc(ECPSocket *sock, unsigned char ctype, ECPConnection **_conn) {
    ECPContext *ctx = sock->ctx;
    ECPConnection *conn;
    int rv;

    if (ctx->conn_alloc == NULL) return ECP_ERR_ALLOC;

    conn = ctx->conn_alloc(sock, ctype);
    if (conn == NULL) return ECP_ERR_ALLOC;

    *_conn = conn;
    return ECP_OK;
}

void ecp_conn_free(ECPConnection *conn) {
    ECPContext *ctx = conn->sock->ctx;

    if (ctx->conn_free) ctx->conn_free(conn);
}

void ecp_conn_init(ECPConnection *conn, ECPSocket *sock, unsigned char ctype) {
    int i;

    memset(conn, 0, sizeof(ECPConnection));

    conn->sock = sock;
    conn->type = ctype;
    conn->key_curr = ECP_ECDH_IDX_INV;
    conn->key_next = ECP_ECDH_IDX_INV;
    conn->rkey_curr = ECP_ECDH_IDX_INV;
    arc4random_buf(&conn->nonce_out, sizeof(conn->nonce_out));
}

void ecp_conn_reinit(ECPConnection *conn) {
    conn->flags = 0;
    conn->key_curr = ECP_ECDH_IDX_INV;
    conn->key_next = ECP_ECDH_IDX_INV;
    conn->rkey_curr = ECP_ECDH_IDX_INV;
    memset(&conn->key, 0, sizeof(conn->key));
    memset(&conn->rkey, 0, sizeof(conn->rkey));
    memset(&conn->shkey, 0, sizeof(conn->shkey));
    arc4random_buf(&conn->nonce_out, sizeof(conn->nonce_out));
}

int ecp_conn_create(ECPConnection *conn, ECPSocket *sock, unsigned char ctype) {
    int rv;

    ecp_conn_init(conn, sock, ctype);

#ifdef ECP_WITH_PTHREAD
    rv = pthread_mutex_init(&conn->mutex, NULL);
    if (rv) return ECP_ERR;
#endif

    return ECP_OK;
}

int ecp_conn_create_inb(ECPConnection *conn, ECPSocket *sock, unsigned char ctype) {
    int rv;

    rv = ecp_conn_create(conn, sock, ctype);
    if (rv) return rv;

    ecp_conn_set_inb(conn);

#ifdef ECP_WITH_VCONN
    if (conn->parent) {
        ecp_conn_refcount_inc(conn->parent);
    }
#endif

    return ECP_OK;
}

void ecp_conn_destroy(ECPConnection *conn) {
#ifdef ECP_WITH_VCONN
    if (ecp_conn_is_inb(conn) && conn->parent) {
        ecp_conn_refcount_dec(conn->parent);
    }
#endif

    ecp_ext_conn_destroy(conn);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_destroy(&conn->mutex);
#endif
}

void ecp_conn_set_flags(ECPConnection *conn, unsigned char flags) {
    flags &= flags & ECP_CONN_FLAG_MASK;
    conn->flags_im |= flags;
}

void ecp_conn_clr_flags(ECPConnection *conn, unsigned char flags) {
    flags &= flags & ECP_CONN_FLAG_MASK;
    conn->flags_im &= ~flags;
}

void ecp_conn_set_remote_key(ECPConnection *conn, ECPDHPub *key) {
    conn->remote.key_perma = *key;
}

void ecp_conn_set_remote_addr(ECPConnection *conn, ecp_tr_addr_t *addr) {
    conn->remote.addr = *addr;
}

int ecp_conn_init_inb(ECPConnection *conn, ECPConnection *parent, unsigned char s_idx, unsigned char c_idx, ecp_ecdh_public_t *public, ECPDHPub *remote_key, ecp_aead_key_t *shkey) {
    ECPSocket *sock = conn->sock;
    unsigned short pcount;
    int rv;

#ifdef ECP_WITH_VCONN
    pcount = (parent ? parent->pcount + 1 : 0);
    if (pcount > ECP_MAX_PARENT) return ECP_ERR_MAX_PARENT;
#endif

    if (ecp_conn_has_vbox(conn) && ((remote_key == NULL) || !remote_key->valid)) return ECP_ERR_VBOX;

    rv = conn_dhkey_set_pub(conn, c_idx, public);
    if (rv) return rv;

    rv = conn_shkey_set(conn, s_idx, c_idx, shkey);
    if (rv) return rv;

#ifdef ECP_WITH_VCONN
    conn->parent = parent;
    conn->pcount = pcount;
#endif

    conn->refcount = 1;
    conn->key_curr = s_idx;
    conn->rkey_curr = c_idx;
    if (remote_key && remote_key->valid) conn->remote.key_perma = *remote_key;

    return ECP_OK;
}

int ecp_conn_init_outb(ECPConnection *conn, ECPNode *node) {
    ECPDHKey key;
    int rv;

    rv = ecp_dhkey_gen(&key);
    if (rv) return rv;

    rv = conn_dhkey_new(conn, 0, &key);
    if (rv) return rv;

    if (node) conn->remote = *node;
    conn->key_curr = 0;

    return ECP_OK;
}

int ecp_conn_insert(ECPConnection *conn) {
    ECPSocket *sock = conn->sock;
    int rv;

    ecp_conn_set_reg(conn);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn_table.mutex);
#endif

    rv = conn_table_insert(conn);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&sock->conn_table.mutex);
#endif

    if (rv) ecp_conn_clr_reg(conn);

    return rv;
}

void ecp_conn_remove(ECPConnection *conn, unsigned short *refcount) {
    ECPSocket *sock = conn->sock;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn_table.mutex);
    pthread_mutex_lock(&conn->mutex);
#endif

    if (ecp_conn_is_reg(conn)) {
        conn_table_remove(conn);
        ecp_conn_clr_reg(conn);
    }
    if (refcount) *refcount = conn->refcount;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
    pthread_mutex_unlock(&sock->conn_table.mutex);
#endif
}

void ecp_conn_remove_addr(ECPConnection *conn) {
    ECPSocket *sock = conn->sock;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn_table.mutex);
    pthread_mutex_lock(&conn->mutex);
#endif

    conn_table_remove_addr(conn);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&sock->conn_table.mutex);
    pthread_mutex_lock(&conn->mutex);
#endif

}

int ecp_conn_open(ECPConnection *conn, ECPNode *node) {
    int rv;
    ssize_t _rv;

    rv = ecp_conn_init_outb(conn, node);
    if (rv) return rv;

    rv = ecp_conn_insert(conn);
    if (rv) return rv;

    _rv = ecp_send_init_req(conn);
    if (_rv < 0) {
        ecp_timer_remove(conn);
        ecp_conn_remove(conn, NULL);
        return _rv;
    }

    return ECP_OK;
}

int ecp_conn_reset(ECPConnection *conn) {
    unsigned short refcount = 0;
    int i;
    int rv;

    /* timer holds one reference to this connection */
    ecp_conn_remove(conn, &refcount);
    if (refcount > 1) return ECP_ERR_BUSY;

    ecp_conn_reinit(conn);
    if (rv) return rv;

    rv = ecp_conn_init_outb(conn, NULL);
    if (rv) return rv;

    rv = ecp_conn_insert(conn);
    if (rv) return rv;

    return ECP_OK;
}

void _ecp_conn_close(ECPConnection *conn) {

    if (ecp_conn_is_open(conn)) {
        ecp_close_handler_t handler;

        ecp_conn_clr_open(conn);
        handler = ecp_get_close_handler(conn);
        if (handler) handler(conn);
    }
    ecp_conn_destroy(conn);
    if (ecp_conn_is_inb(conn)) ecp_conn_free(conn);
}

int ecp_conn_close(ECPConnection *conn) {
    unsigned short refcount = 0;

    ecp_timer_remove(conn);
    ecp_conn_remove(conn, &refcount);
    if (refcount) return ECP_ERR_BUSY;

    _ecp_conn_close(conn);
    return ECP_OK;
}

void ecp_conn_refcount_inc(ECPConnection *conn) {
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif
    conn->refcount++;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif
}

void ecp_conn_refcount_dec(ECPConnection *conn) {
    int is_reg;
    unsigned short refcount;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif
    conn->refcount--;
    refcount = conn->refcount;
    is_reg = ecp_conn_is_reg(conn);
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    if (!is_reg && (refcount == 0)) _ecp_conn_close(conn);
}

int ecp_conn_dhkey_new(ECPConnection *conn) {
    ECPSocket *sock = conn->sock;
    ECPDHKey new_key;
    unsigned char idx;
    int rv;
    ssize_t _rv;

    rv = ecp_dhkey_gen(&new_key);
    if (rv) return rv;

#ifdef ECP_WITH_PTHREAD
    if (ecp_conn_is_outb(conn)) pthread_mutex_lock(&sock->conn_table.mutex);
    pthread_mutex_lock(&conn->mutex);
#endif

    idx = conn->key_curr;
    if (idx != ECP_ECDH_IDX_INV) {
        idx = (idx + 1) % ECP_MAX_CONN_KEY;
        rv = conn_dhkey_new(conn, idx, &new_key);
        conn->key_next = idx;
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
    if (ecp_conn_is_outb(conn)) pthread_mutex_unlock(&sock->conn_table.mutex);
#endif
    if (idx == ECP_ECDH_IDX_INV) return ECP_ERR_ECDH_IDX;
    if (rv) return rv;

    _rv = ecp_send_keyx_req(conn);
    if (_rv < 0) return _rv;

    return ECP_OK;
}

int ecp_conn_dhkey_get(ECPSocket *sock, unsigned char idx, ECPDHKey *key) {
    int rv = ECP_OK;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->mutex);
#endif

    if (idx < ECP_MAX_CONN_KEY) {
        *key = sock->key[idx];
    } else {
        rv = ECP_ERR_ECDH_IDX;
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&sock->mutex);
#endif

    if (!rv && !key->valid) rv = ECP_ERR_ECDH_IDX;
    return rv;
}

int ecp_conn_dhkey_get_pub(ECPConnection *conn, unsigned char *idx, ecp_ecdh_public_t *public, int will_send) {
    int rv = ECP_OK;
    unsigned char _idx;

    if (ecp_conn_is_inb(conn)) {
        ECPSocket *sock = conn->sock;

        rv = ecp_sock_dhkey_get_pub(sock, &_idx, public);
        if (rv) return rv;

#ifdef ECP_WITH_PTHREAD
        if (will_send) pthread_mutex_lock(&conn->mutex);
#endif

    } else {
        ECPDHKey *key;

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&conn->mutex);
#endif

        if (will_send) {
            _idx = conn->key_next;
        } else {
            _idx = conn->key_curr;
        }
        if (_idx == ECP_ECDH_IDX_INV) rv = ECP_ERR_ECDH_IDX;

        if (!rv) {
            key = &conn->key[_idx];
            if (!key->valid) rv = ECP_ERR_ECDH_IDX;
        }
        if (!rv) memcpy(public, &key->public, sizeof(key->public));
    }

    if (!rv && will_send) conn_dhkey_get_pub(conn, _idx);

#ifdef ECP_WITH_PTHREAD
    if (will_send || ecp_conn_is_outb(conn)) pthread_mutex_unlock(&conn->mutex);
#endif

    if (rv) return rv;

    if (idx) *idx = _idx;
    return ECP_OK;
}

int ecp_conn_dhkey_set_pub(ECPConnection *conn, unsigned char idx, ecp_ecdh_public_t *public) {
    ECPSocket *sock = conn->sock;
    int rv;

#ifdef ECP_WITH_PTHREAD
    if (ecp_conn_is_inb(conn)) pthread_mutex_lock(&sock->conn_table.mutex);
    pthread_mutex_lock(&conn->mutex);
#endif
    rv = conn_dhkey_set_pub(conn, idx, public);
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
    if (ecp_conn_is_inb(conn)) pthread_mutex_unlock(&sock->conn_table.mutex);
#endif
    if (rv == ECP_ERR_ECDH_KEY_DUP) rv = ECP_OK;

    return rv;
}

void ecp_conn_dhkey_set_curr(ECPConnection *conn) {
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif

    if (conn->key_next != ECP_ECDH_IDX_INV) {
        conn->key_curr = conn->key_next;
        conn->key_next = ECP_ECDH_IDX_INV;
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif
}

void ecp_conn_handler_init(ECPConnHandler *handler, ecp_msg_handler_t handle_msg, ecp_open_handler_t handle_open, ecp_close_handler_t handle_close, ecp_open_send_t send_open) {
    memset(handler, 0, sizeof(ECPConnHandler));
    handler->handle_msg = handle_msg;
    handler->handle_open = handle_open;
    handler->handle_close = handle_close;
    handler->send_open = send_open;
}

ecp_msg_handler_t ecp_get_msg_handler(ECPConnection *conn) {
    ECPContext *ctx = conn->sock->ctx;
    unsigned char ctype;

    ctype = conn->type;
    if (ecp_conn_is_sys(conn)) {
        switch (ctype) {
#ifdef ECP_WITH_DIR
            case ECP_CTYPE_DIR:
                return ecp_dir_handle_msg;
#endif

#ifdef ECP_WITH_VCONN
            case ECP_CTYPE_VCONN:
            case ECP_CTYPE_VLINK:
                return ecp_vconn_handle_msg;
#endif

            default:
                return NULL;
        }
    }

    if (ctype >= ECP_MAX_CTYPE) return NULL;
    return ctx->handler[ctype] ? ctx->handler[ctype]->handle_msg : NULL;
}

ecp_open_handler_t ecp_get_open_handler(ECPConnection *conn) {
    ECPContext *ctx = conn->sock->ctx;
    unsigned char ctype;

    ctype = conn->type;
    if (ecp_conn_is_sys(conn)) {
        switch (ctype) {
#ifdef ECP_WITH_DIR
            case ECP_CTYPE_DIR:
                return ecp_dir_handle_open;
#endif

#ifdef ECP_WITH_VCONN
            case ECP_CTYPE_VCONN:
            case ECP_CTYPE_VLINK:
                return ecp_vconn_handle_open;
#endif

            default:
                return NULL;
        }
    }

    if (ctype >= ECP_MAX_CTYPE) return NULL;
    return ctx->handler[ctype] ? ctx->handler[ctype]->handle_open : NULL;
}

ecp_close_handler_t ecp_get_close_handler(ECPConnection *conn) {
    ECPContext *ctx = conn->sock->ctx;
    unsigned char ctype;

    ctype = conn->type;
    if (ecp_conn_is_sys(conn)) {
        switch (ctype) {
#ifdef ECP_WITH_DIR
            case ECP_CTYPE_DIR:
                return NULL;
#endif

#ifdef ECP_WITH_VCONN
            case ECP_CTYPE_VCONN:
            case ECP_CTYPE_VLINK:
                return ecp_vconn_handle_close;
#endif

            default:
                return NULL;
        }
    }

    if (ctype >= ECP_MAX_CTYPE) return NULL;
    return ctx->handler[ctype] ? ctx->handler[ctype]->handle_close : NULL;
}

ecp_dir_handler_t ecp_get_dir_handler(ECPConnection *conn) {
    ECPContext *ctx = conn->sock->ctx;

    return ctx->handle_dir;
}

void ecp_err_handle(ECPConnection *conn, unsigned char mtype, int err) {
    ECPContext *ctx = conn->sock->ctx;
    int rv;

    rv = ecp_ext_err_handle(conn, mtype, err);
    if (rv != ECP_PASS) return;

    if (ctx->handle_err) ctx->handle_err(conn, mtype, err);
}

static ssize_t _send_ireq(ECPConnection *conn, ECPTimerItem *ti) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(0, ECP_MTYPE_INIT_REQ, conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(0, ECP_MTYPE_INIT_REQ, conn)];

    packet.buffer = pkt_buf;
    packet.size = sizeof(pkt_buf);
    payload.buffer = pld_buf;
    payload.size = sizeof(pld_buf);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_INIT_REQ);

    return _ecp_pld_send(conn, &packet, ECP_ECDH_IDX_PERMA, ECP_ECDH_IDX_INV, NULL, NULL, &payload, ECP_SIZE_PLD(0, ECP_MTYPE_INIT_REQ), 0, ti);
}

static ssize_t _retry_ireq(ECPConnection *conn, ECPTimerItem *ti) {
    int rv;

    rv = ecp_conn_reset(conn);
    if (rv) return rv;

    return _send_ireq(conn, ti);
}

ssize_t ecp_send_init_req(ECPConnection *conn) {
    ECPTimerItem ti;

    ecp_timer_item_init(&ti, conn, ECP_MTYPE_OPEN_REP, _retry_ireq, ECP_SEND_TRIES-1, ECP_SEND_TIMEOUT);

    return _send_ireq(conn, &ti);
}

ssize_t ecp_handle_init_req(ECPSocket *sock, ECPConnection *parent, ecp_tr_addr_t *addr, unsigned char *public_buf, ecp_aead_key_t *shkey) {
    ssize_t rv;

    rv = ecp_send_init_rep(sock, parent, addr, public_buf, shkey);
    if (rv < 0) return rv;

    return 0;
}

ssize_t ecp_send_init_rep(ECPSocket *sock, ECPConnection *parent, ecp_tr_addr_t *addr, unsigned char *public_buf, ecp_aead_key_t *shkey) {
    ECPBuffer packet;
    ECPBuffer payload;
    ECPPktMeta pkt_meta;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF_IREP(1+ECP_SIZE_ECDH_PUB+ECP_SIZE_COOKIE, ECP_MTYPE_INIT_REP, parent)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF_IREP(1+ECP_SIZE_ECDH_PUB+ECP_SIZE_COOKIE, ECP_MTYPE_INIT_REP, parent)];
    unsigned char cookie[ECP_SIZE_COOKIE];
    unsigned char *msg;
    ecp_nonce_t nonce;
    ssize_t rv;
    int _rv;

    packet.buffer = pkt_buf;
    packet.size = sizeof(pkt_buf);
    payload.buffer = pld_buf;
    payload.size = sizeof(pld_buf);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_INIT_REP);
    msg = ecp_pld_get_msg(payload.buffer, payload.size);

    _rv = ecp_cookie_gen(sock, cookie, public_buf);
    if (_rv) return _rv;

    _rv = ecp_sock_dhkey_get_pub(sock, msg, (ecp_ecdh_public_t *)(msg+1));
    if (_rv) return _rv;
    memcpy(msg+1+ECP_SIZE_ECDH_PUB, cookie, ECP_SIZE_COOKIE);

    ecp_sock_get_nonce(sock, &nonce);
    pkt_meta.cookie = NULL;
    pkt_meta.public = NULL;
    pkt_meta.shkey = shkey;
    pkt_meta.nonce = &nonce;
    pkt_meta.ntype = ECP_NTYPE_INB;
    pkt_meta.s_idx = 0xf;
    pkt_meta.c_idx = 0xf;

    rv = ecp_pld_send_irep(sock, parent, addr, &packet, &pkt_meta, &payload, ECP_SIZE_PLD(1+ECP_SIZE_ECDH_PUB+ECP_SIZE_COOKIE, ECP_MTYPE_INIT_REP), 0);
    return rv;
}

ssize_t ecp_handle_init_rep(ECPConnection *conn, unsigned char *msg, size_t msg_size) {
    ecp_open_send_t send_open_f;
    unsigned char ctype;
    unsigned char *cookie;
    ssize_t rv;
    int _rv;

    if (msg_size < 1+ECP_SIZE_ECDH_PUB+ECP_SIZE_COOKIE) return ECP_ERR_SIZE;

    _rv = ecp_conn_dhkey_set_pub(conn, msg[0], (ecp_ecdh_public_t *)(msg+1));
    if (_rv) return _rv;

    ctype = conn->type;
    send_open_f = NULL;
    cookie = msg+1+ECP_SIZE_ECDH_PUB;
    if (ecp_conn_is_sys(conn)) {
        switch (ctype) {
#ifdef ECP_WITH_VCONN
            case ECP_CTYPE_VCONN:
            case ECP_CTYPE_VLINK:
                send_open_f = ecp_vconn_send_open_req;
                break;
#endif

            default:
                break;
        }
    } else {
        ECPContext *ctx = conn->sock->ctx;

        if (ctype >= ECP_MAX_CTYPE) return ECP_ERR_CTYPE;
        send_open_f = ctx->handler[ctype] ? ctx->handler[ctype]->send_open : NULL;
    }
    if (send_open_f == NULL) send_open_f = ecp_send_open_req;

    rv = send_open_f(conn, cookie);
    if (rv < 0) return rv;

    return 1+ECP_SIZE_ECDH_PUB+ECP_SIZE_COOKIE;
}

ssize_t ecp_write_open_req(ECPConnection *conn, ECPBuffer *payload) {
    unsigned char *msg;
    unsigned char vbox;
    ssize_t rv;
    int _rv;

    if (payload->size < ECP_SIZE_PLD(2, ECP_MTYPE_OPEN_REQ)) return ECP_ERR_SIZE;

    ecp_pld_set_type(payload->buffer, payload->size, ECP_MTYPE_OPEN_REQ);
    msg = ecp_pld_get_msg(payload->buffer, payload->size);
    *msg = conn->type;
    msg++;

    vbox = ecp_conn_has_vbox(conn);
    *msg = vbox;
    msg++;

    rv = 0;
    if (vbox) {
        ECPSocket *sock = conn->sock;
        ECPDHKey key;
        ECPDHPub *remote_key;
        ecp_ecdh_public_t public;
        ecp_aead_key_t vbox_shkey;
        ecp_nonce_t vbox_nonce;

        remote_key = &conn->remote.key_perma;
        if (payload->size < ECP_SIZE_PLD(2+ECP_SIZE_VBOX, ECP_MTYPE_OPEN_REQ)) _rv = ECP_ERR_SIZE;
        if (!_rv && !remote_key->valid) _rv = ECP_ERR;
        if (!_rv) _rv = ecp_sock_dhkey_get(sock, ECP_ECDH_IDX_PERMA, &key);
        if (!_rv) _rv = ecp_conn_dhkey_get_pub(conn, NULL, &public, 0);
        if (_rv) return _rv;

        memcpy(msg, &key.public, ECP_SIZE_ECDH_PUB);
        msg += ECP_SIZE_ECDH_PUB;

        arc4random_buf(&vbox_nonce, sizeof(vbox_nonce));
        ecp_nonce2buf(msg, &vbox_nonce);
        msg += ECP_SIZE_NONCE;

        ecp_ecdh_shkey(&vbox_shkey, &remote_key->public, &key.private);
        rv = ecp_aead_enc(msg, payload->size - (msg - payload->buffer), (unsigned char *)&public, ECP_SIZE_ECDH_PUB, &vbox_shkey, &vbox_nonce, ECP_NTYPE_VBOX);
        if (rv < 0) return rv;
    }

    return ECP_SIZE_PLD(2+rv, ECP_MTYPE_OPEN_REQ);
}

ssize_t ecp_send_open_req(ECPConnection *conn, unsigned char *cookie) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF_WCOOKIE(2+ECP_SIZE_VBOX, ECP_MTYPE_OPEN_REQ, conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(2+ECP_SIZE_VBOX, ECP_MTYPE_OPEN_REQ, conn)];
    ssize_t rv;

    packet.buffer = pkt_buf;
    packet.size = sizeof(pkt_buf);
    payload.buffer = pld_buf;
    payload.size = sizeof(pld_buf);

    rv = ecp_write_open_req(conn, &payload);
    if (rv < 0) return rv;

    rv = ecp_pld_send_wcookie(conn, &packet, &payload, rv, 0, cookie);
    return rv;
}

ssize_t ecp_handle_open_req(ECPSocket *sock, ECPConnection *parent, unsigned char s_idx, unsigned char c_idx, unsigned char *public_buf, unsigned char *msg, size_t msg_size, ecp_aead_key_t *shkey, ECPConnection **_conn) {
    ECPConnection *conn;
    ECPDHPub remote_key;
    unsigned char ctype;
    unsigned char vbox;
    ssize_t rv;
    int _rv;

    if (msg_size < 2) return ECP_ERR_SIZE;
    ctype = *msg;
    msg++;

    vbox = *msg;
    msg++;

    msg_size -= 2;
    remote_key.valid = 0;
    if (vbox) {
        ECPDHKey key;
        ecp_ecdh_public_t public;
        ecp_aead_key_t vbox_shkey;
        ecp_nonce_t vbox_nonce;

        if (msg_size < ECP_SIZE_VBOX) return ECP_ERR_SIZE;

        _rv = ecp_sock_dhkey_get(sock, ECP_ECDH_IDX_PERMA, &key);
        if (_rv) return _rv;

        memcpy(&remote_key.public, msg, ECP_SIZE_ECDH_PUB);
        msg+= ECP_SIZE_ECDH_PUB;
        msg_size -= ECP_SIZE_ECDH_PUB;

        ecp_buf2nonce(&vbox_nonce, msg);
        msg+= ECP_SIZE_NONCE;
        msg_size -= ECP_SIZE_NONCE;

        ecp_ecdh_shkey(&vbox_shkey, &remote_key.public, &key.private);
        rv = ecp_aead_dec((unsigned char *)&public, ECP_SIZE_ECDH_PUB, msg, msg_size, &vbox_shkey, &vbox_nonce, ECP_NTYPE_VBOX);
        if (rv != ECP_SIZE_ECDH_PUB) return ECP_ERR_VBOX;

        if (memcmp(&public, public_buf, ECP_SIZE_ECDH_PUB) != 0) return ECP_ERR_VBOX;
        remote_key.valid = 1;
    }

    _rv = ecp_conn_alloc(sock, ctype, &conn);
    if (_rv) return _rv;

    _rv = ecp_conn_init_inb(conn, parent, s_idx, c_idx, (ecp_ecdh_public_t *)public_buf, remote_key.valid ? &remote_key : NULL, shkey);
    if (!_rv) _rv = ecp_conn_insert(conn);
    if (_rv) {
        ecp_conn_destroy(conn);
        ecp_conn_free(conn);
        return _rv;
    }

    *_conn = conn;

    /* handle_open will be called later from msg handler */
    return 2+(vbox ? ECP_SIZE_VBOX : 0);
}

ssize_t ecp_send_open_rep(ECPConnection *conn) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(0, ECP_MTYPE_OPEN_REP, conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(0, ECP_MTYPE_OPEN_REP, conn)];
    ssize_t rv;

    packet.buffer = pkt_buf;
    packet.size = sizeof(pkt_buf);
    payload.buffer = pld_buf;
    payload.size = sizeof(pld_buf);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_OPEN_REP);
    rv = ecp_pld_send(conn, &packet, &payload, ECP_SIZE_PLD(0, ECP_MTYPE_OPEN_REP), 0);
    return rv;
}

ssize_t ecp_handle_open(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *bufs) {
    size_t rsize;
    ecp_open_handler_t handler;
    int rv = ECP_OK;

    if (mtype == ECP_MTYPE_OPEN_REQ) {
        if (ecp_conn_is_outb(conn)) return ECP_ERR;

        rsize = 2;
        if (msg_size < rsize) return ECP_ERR_SIZE;

        if (msg[1]) rsize += ECP_SIZE_VBOX;
        if (msg_size < rsize) return ECP_ERR_SIZE;
    } else {
        if (ecp_conn_is_inb(conn)) return ECP_ERR;
        rsize = 0;
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif

    if (ecp_conn_is_open(conn)) rv = ECP_ERR;
    if (!rv) rv = ecp_ext_conn_open(conn);
    if (!rv) ecp_conn_set_open(conn);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    if (rv) return rv;

    handler = ecp_get_open_handler(conn);
    if (handler) rv = handler(conn, bufs);
    if (rv) {
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&conn->mutex);
#endif

        ecp_conn_clr_open(conn);

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn->mutex);
#endif

        if (ecp_conn_is_inb(conn)) ecp_conn_close(conn);
        return rv;
    }

    if (ecp_conn_is_inb(conn)) {
        ssize_t _rv;

        _rv = ecp_send_open_rep(conn);
        if (_rv < 0) {
            ecp_conn_close(conn);
            return _rv;
        }
    } else {
        ecp_conn_remove_addr(conn);
    }
    return rsize;
}

static ssize_t _send_keyx_req(ECPConnection *conn, ECPTimerItem *ti) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(1+ECP_SIZE_ECDH_PUB, ECP_MTYPE_KEYX_REQ, conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(1+ECP_SIZE_ECDH_PUB, ECP_MTYPE_KEYX_REQ, conn)];
    unsigned char *msg;
    ssize_t rv;
    int _rv;

    packet.buffer = pkt_buf;
    packet.size = sizeof(pkt_buf);
    payload.buffer = pld_buf;
    payload.size = sizeof(pld_buf);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_KEYX_REQ);
    msg = ecp_pld_get_msg(payload.buffer, payload.size);

    _rv = ecp_conn_dhkey_get_pub(conn, msg, (ecp_ecdh_public_t *)(msg+1), 1);
    if (_rv) return _rv;

    rv = ecp_pld_send_wtimer(conn, &packet, &payload, ECP_SIZE_PLD(1+ECP_SIZE_ECDH_PUB, ECP_MTYPE_KEYX_REQ), 0, ti);
    return rv;
}

ssize_t ecp_send_keyx_req(ECPConnection *conn) {
    return ecp_timer_send(conn, _send_keyx_req, ECP_MTYPE_KEYX_REP, ECP_SEND_TRIES, ECP_SEND_TIMEOUT);
}

ssize_t ecp_send_keyx_rep(ECPConnection *conn) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(1+ECP_SIZE_ECDH_PUB, ECP_MTYPE_KEYX_REP, conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(1+ECP_SIZE_ECDH_PUB, ECP_MTYPE_KEYX_REP, conn)];
    unsigned char *msg;
    ssize_t rv;
    int _rv;

    packet.buffer = pkt_buf;
    packet.size = sizeof(pkt_buf);
    payload.buffer = pld_buf;
    payload.size = sizeof(pld_buf);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_KEYX_REP);
    msg = ecp_pld_get_msg(payload.buffer, payload.size);

    _rv = ecp_conn_dhkey_get_pub(conn, msg, (ecp_ecdh_public_t *)(msg+1), 1);
    if (_rv) return _rv;

    rv = ecp_pld_send(conn, &packet, &payload, ECP_SIZE_PLD(1+ECP_SIZE_ECDH_PUB, ECP_MTYPE_KEYX_REP), 0);
    return rv;
}

ssize_t ecp_handle_keyx(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size) {
    ssize_t rv;
    int _rv;

    if (msg_size < 1+ECP_SIZE_ECDH_PUB) return ECP_ERR_SIZE;

    _rv = ecp_conn_dhkey_set_pub(conn, msg[0], (ecp_ecdh_public_t *)(msg+1));
    if (_rv) return _rv;

    if (mtype == ECP_MTYPE_KEYX_REP) {
        if (ecp_conn_is_inb(conn)) return ECP_ERR;

        ecp_conn_dhkey_set_curr(conn);
    } else {
        if (ecp_conn_is_outb(conn)) return ECP_ERR;

        rv = ecp_send_keyx_rep(conn);
        if (rv < 0) return rv;
    }

    return 1+ECP_SIZE_ECDH_PUB;
}

ssize_t ecp_msg_handle(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *bufs) {
    if (mtype & ECP_MTYPE_FLAG_SYS) {
        switch (mtype) {
            case ECP_MTYPE_OPEN_REQ:
            case ECP_MTYPE_OPEN_REP:
                return ecp_handle_open(conn, mtype, msg, msg_size, bufs);

            case ECP_MTYPE_KEYX_REQ:
            case ECP_MTYPE_KEYX_REP:
                return ecp_handle_keyx(conn, mtype, msg, msg_size);

            default:
                return ecp_ext_msg_handle(conn, seq, mtype, msg, msg_size, bufs);
        }
    } else {
        ecp_msg_handler_t handler;

        handler = ecp_get_msg_handler(conn);
        if (handler) {
            return handler(conn, seq, mtype, msg, msg_size, bufs);
        } else {
            return ECP_ERR_HANDLER;
        }
    }
}

ssize_t ecp_pld_handle_one(ECPConnection *conn, ecp_seq_t seq, unsigned char *payload, size_t pld_size, ECP2Buffer *bufs) {
    unsigned char mtype;
    unsigned char *msg;
    size_t hdr_size, msg_size;
    ssize_t rv;
    int _rv;

    _rv = ecp_pld_get_type(payload, pld_size, &mtype);
    if (_rv) return _rv;

    ecp_timer_pop(conn, mtype);

    msg = ecp_pld_get_msg(payload, pld_size);
    if (msg == NULL) return ECP_ERR;
    hdr_size = msg - payload;
    msg_size = pld_size - hdr_size;

    rv = ecp_msg_handle(conn, seq, mtype, msg, msg_size, bufs);
    if (rv < 0) return rv;

    rv += hdr_size;
    if (rv > pld_size) return ECP_ERR_SIZE;
    return rv;
}

ssize_t ecp_pld_handle(ECPConnection *conn, ecp_seq_t seq, unsigned char *payload, size_t _pld_size, ECP2Buffer *bufs) {
    size_t pld_size = _pld_size;
    ssize_t rv;

    rv = ecp_ext_pld_handle(conn, seq, payload, pld_size, bufs);
    if (rv < 0) return rv;

    payload += rv;
    pld_size -= rv;

    while (pld_size) {
        rv = ecp_pld_handle_one(conn, seq, payload, pld_size, bufs);
        if (rv == ECP_ERR_HANDLER) return _pld_size - pld_size;
        if (rv < 0) return rv;

        payload += rv;
        pld_size -= rv;
    }

    return _pld_size;
}

ssize_t ecp_unpack(ECPSocket *sock, ECPConnection *parent, ecp_tr_addr_t *addr, ECP2Buffer *bufs, size_t _pkt_size, ECPConnection **_conn, unsigned char **_payload, ecp_seq_t *_seq) {
    ECPConnection *conn = NULL;
    unsigned char idx;
    unsigned char s_idx;
    unsigned char c_idx;
    unsigned char *payload;
    unsigned char *packet;
    ssize_t pld_size;
    size_t pkt_size = _pkt_size;
    ecp_aead_key_t shkey;
    unsigned char *public_buf = NULL;
    unsigned char *nonce_buf = NULL;
    unsigned char is_open = 0;
    unsigned char is_inb = 0;
    ecp_nonce_t nonce_pkt, nonce_conn, nonce_in;
    ecp_ack_t nonce_map;
    ssize_t rv;
    int _rv = ECP_OK;

    if (pkt_size < ECP_MIN_PKT) return ECP_ERR_SIZE;

    *_conn = NULL;
    *_payload = NULL;
    *_seq = 0;

    packet = bufs->packet->buffer;
    idx = packet[ECP_SIZE_PROTO];
    s_idx = (idx & 0xF0) >> 4;
    c_idx = (idx & 0x0F);
    if (idx != ECP_ECDH_IDX_INV) {
        public_buf = packet+ECP_SIZE_PROTO+1;
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn_table.mutex);
#endif

    conn = conn_table_search(sock, c_idx, (ecp_ecdh_public_t *)public_buf, addr);

#ifdef ECP_WITH_PTHREAD
    if (conn) pthread_mutex_lock(&conn->mutex);
    pthread_mutex_unlock(&sock->conn_table.mutex);
#endif

    if (conn) {
        is_inb = ecp_conn_is_inb(conn);
        is_open = ecp_conn_is_open(conn);

        if (is_open) {
            nonce_buf = packet+ECP_SIZE_PROTO+1+ECP_SIZE_ECDH_PUB;
            packet += ECP_SIZE_PKT_HDR;
            pkt_size -= ECP_SIZE_PKT_HDR;

            nonce_conn = conn->nonce_in;
            nonce_map = conn->nonce_map;
        } else if (!is_inb && (idx == ECP_ECDH_IDX_INV)) {
            nonce_buf = packet+ECP_SIZE_PROTO+1;
            packet += ECP_SIZE_PROTO+1+ECP_SIZE_NONCE;
            pkt_size -= ECP_SIZE_PROTO+1+ECP_SIZE_NONCE;

            s_idx = ECP_ECDH_IDX_PERMA;
            c_idx = conn->key_curr;
        } else {
            _rv = ECP_ERR;
        }
        if (!_rv) _rv = conn_shkey_get(conn, s_idx, c_idx, &shkey);
        if (!_rv) conn->refcount++;

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn->mutex);
#endif

        if (_rv) return _rv;
    } else {
        ECPDHKey key;

        is_inb = 1;
        public_buf = packet+ECP_SIZE_PROTO+1;

        if (s_idx == ECP_ECDH_IDX_PERMA) {
            nonce_buf = public_buf+ECP_SIZE_ECDH_PUB;
            packet += ECP_SIZE_PKT_HDR;
            pkt_size -= ECP_SIZE_PKT_HDR;

            _rv = ecp_sock_dhkey_get(sock, s_idx, &key);
            if (_rv) return _rv;

            ecp_ecdh_shkey(&shkey, (ecp_ecdh_public_t *)public_buf, &key.private);
        } else if (pkt_size >= (ECP_MIN_PKT+ECP_SIZE_COOKIE)) {
            unsigned char *cookie_buf;

            cookie_buf = public_buf+ECP_SIZE_ECDH_PUB;
            nonce_buf = cookie_buf+ECP_SIZE_COOKIE;
            packet += ECP_SIZE_PKT_HDR+ECP_SIZE_COOKIE;
            pkt_size -= ECP_SIZE_PKT_HDR+ECP_SIZE_COOKIE;

            _rv = ecp_cookie_verify(sock, cookie_buf, public_buf);
            if (_rv) return _rv;

            _rv = ecp_sock_dhkey_get(sock, s_idx, &key);
            if (_rv) return _rv;

            ecp_ecdh_shkey(&shkey, (ecp_ecdh_public_t *)public_buf, &key.private);
        } else {
            return ECP_ERR;
        }
    }

    ecp_buf2nonce(&nonce_pkt, nonce_buf);
    if (conn && is_open) {
        if (ECP_NONCE_LTE(nonce_pkt, nonce_conn)) {
            ecp_nonce_t nonce_offset = nonce_conn - nonce_pkt;

            if (nonce_offset < ECP_SIZE_ACKB) {
                ecp_ack_t nonce_mask = ((ecp_ack_t)1 << nonce_offset);

                if (nonce_mask & nonce_map) _rv = ECP_ERR_SEQ;
                if (!_rv) nonce_in = nonce_conn;
            } else {
                _rv = ECP_ERR_SEQ;
            }
        } else {
            ecp_nonce_t nonce_offset = nonce_pkt - nonce_conn;

            if (nonce_offset < ECP_MAX_SEQ_FWD) {
                if (nonce_offset < ECP_SIZE_ACKB) {
                    nonce_map = nonce_map << nonce_offset;
                } else {
                    nonce_map = 0;
                }
                nonce_map |= 1;
                nonce_in = nonce_pkt;
            } else {
                _rv = ECP_ERR_SEQ;
            }
        }
        if (_rv) {
            rv = _rv;
            goto unpack_err;
        }
    }

    payload = bufs->payload->buffer;
    rv = ecp_aead_dec(payload, bufs->payload->size, packet, pkt_size, &shkey, &nonce_pkt, is_inb ? ECP_NTYPE_INB : ECP_NTYPE_OUTB);
    if (rv < 0) goto unpack_err;

    pld_size = rv;
    if (pld_size < ECP_MIN_PLD) {
        rv = ECP_ERR_SIZE;
        goto unpack_err;
    }

    if (conn == NULL) {
        unsigned char mtype;
        unsigned char *msg;
        size_t hdr_size, msg_size;

        _rv = ecp_pld_get_type(payload, pld_size, &mtype);
        if (_rv) return _rv;

        msg = ecp_pld_get_msg(payload, pld_size);
        if (msg == NULL) return ECP_ERR;
        hdr_size = msg - payload;
        msg_size = pld_size - hdr_size;

        switch (mtype) {
            case ECP_MTYPE_INIT_REQ: {
                rv = ecp_handle_init_req(sock, parent, addr, public_buf, &shkey);
                if (rv < 0) return rv;

                rv += hdr_size;
                break;
            }

            case ECP_MTYPE_OPEN_REQ: {
                rv = ecp_handle_open_req(sock, parent, s_idx, c_idx, public_buf, msg, msg_size, &shkey, &conn);
                if (rv < 0) return rv;

                /* pass to payload handler */
                nonce_in = nonce_pkt;
                nonce_map = ECP_ACK_FULL;
                is_open = 1;
                rv = 0;
                break;
            }

            default:
                return ECP_ERR_MTYPE;
        }

        payload += rv;
        pld_size -= rv;
    } else if (!is_open) {
        unsigned char mtype;
        unsigned char *msg;
        size_t hdr_size, msg_size;

        _rv = ecp_pld_get_type(payload, pld_size, &mtype);
        if (_rv) {
            rv = _rv;
            goto unpack_err;
        }

        msg = ecp_pld_get_msg(payload, pld_size);
        if (msg == NULL) {
            rv = ECP_ERR;
            goto unpack_err;
        }
        hdr_size = msg - payload;
        msg_size = pld_size - hdr_size;

        switch (mtype) {
            case ECP_MTYPE_INIT_REP: {
                rv = ecp_handle_init_rep(conn, msg, msg_size);
                if (rv < 0) goto unpack_err;

                rv += hdr_size;
                break;
            }

            case ECP_MTYPE_OPEN_REP: {
                /* pass to payload handler */
                nonce_in = nonce_pkt;
                nonce_map = ECP_ACK_FULL;
                is_open = 1;
                rv = 0;
                break;
            }

            default:
                rv = ECP_ERR_MTYPE;
                goto unpack_err;
        }

        payload += rv;
        pld_size -= rv;
    }

    if (conn && is_open) {
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&conn->mutex);
#endif

        conn->nonce_in = nonce_in;
        conn->nonce_map = nonce_map;
        if (is_inb && addr) conn->remote.addr = *addr;

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn->mutex);
#endif

        *_conn = conn;
        *_payload = payload;
        *_seq = (ecp_seq_t)nonce_pkt;
    }

    return _pkt_size - pld_size;

unpack_err:
    if (conn) ecp_conn_refcount_dec(conn);
    return rv;
}

ssize_t ecp_pkt_handle(ECPSocket *sock, ECPConnection *parent, ecp_tr_addr_t *addr, ECP2Buffer *bufs, size_t pkt_size) {
    ECPConnection *conn = NULL;
    unsigned char *payload;
    ecp_seq_t seq;
    size_t pld_size;
    ssize_t rv;

    rv = ecp_unpack(sock, parent, addr, bufs, pkt_size, &conn, &payload, &seq);
    if (rv < 0) return rv;

    pld_size = pkt_size - rv;
    if (conn == NULL) {
        if (pld_size) return ECP_ERR;
        return pkt_size;
    }

    if (pld_size) {
        rv = ecp_ext_pld_store(conn, seq, payload, pld_size, bufs);
        if (rv < 0) goto pkt_handle_fin;

        payload += rv;
        pld_size -= rv;
    }

    if (pld_size) {
        rv = ecp_pld_handle(conn, seq, payload, pld_size, bufs);
        if (rv < 0) goto pkt_handle_fin;

        payload += rv;
        pld_size -= rv;
    }

    rv = pkt_size - pld_size;

pkt_handle_fin:
    ecp_conn_refcount_dec(conn);
    return rv;
}

ssize_t ecp_pkt_send(ECPSocket *sock, ECPBuffer *packet, size_t pkt_size, unsigned char flags, ECPTimerItem *ti, ecp_tr_addr_t *addr) {
    ssize_t rv;

    if (ti) {
        int _rv;

        _rv = ecp_timer_push(ti);
        if (_rv) return _rv;
    }

    rv = ecp_tr_send(sock, packet, pkt_size, addr, flags);
    if (rv < 0) return rv;

    return rv;
}

void ecp_nonce2buf(unsigned char *b, ecp_nonce_t *n) {
    b[0] = *n >> 56;
    b[1] = *n >> 48;
    b[2] = *n >> 40;
    b[3] = *n >> 32;
    b[4] = *n >> 24;
    b[5] = *n >> 16;
    b[6] = *n >> 8;
    b[7] = *n;
}

void ecp_buf2nonce(ecp_nonce_t *n, unsigned char *b) {
    *n  = (ecp_nonce_t)b[0] << 56;
    *n |= (ecp_nonce_t)b[1] << 48;
    *n |= (ecp_nonce_t)b[2] << 40;
    *n |= (ecp_nonce_t)b[3] << 32;
    *n |= (ecp_nonce_t)b[4] << 24;
    *n |= (ecp_nonce_t)b[5] << 16;
    *n |= (ecp_nonce_t)b[6] << 8;
    *n |= (ecp_nonce_t)b[7];
}

int ecp_pkt_get_seq(unsigned char *pkt, size_t pkt_size, ecp_seq_t *s) {
    *s = 0;

    if (pkt_size < ECP_MIN_PKT) return ECP_ERR_SIZE;

    pkt += ECP_SIZE_PROTO+1+ECP_SIZE_ECDH_PUB;
    *s |= (ecp_seq_t)pkt[4] << 24;
    *s |= (ecp_seq_t)pkt[5] << 16;
    *s |= (ecp_seq_t)pkt[6] << 8;
    *s |= (ecp_seq_t)pkt[7];

    return ECP_OK;
}

static ssize_t _pack(ECPBuffer *packet, ECPPktMeta *pkt_meta, ECPBuffer *payload, size_t pld_size) {
    ssize_t rv;
    unsigned char s_idx, c_idx;
    unsigned char *pkt_buf;
    size_t pkt_size;
    size_t hdr_size;

    pkt_size = ECP_MIN_PKT;
    if (pkt_meta->public == NULL) pkt_size -= ECP_SIZE_ECDH_PUB;
    if (pkt_meta->cookie) pkt_size += ECP_SIZE_COOKIE;

    if (packet->size < pkt_size) return ECP_ERR_SIZE;

    pkt_buf = packet->buffer;
    pkt_size = packet->size;

    // ECP_SIZE_PROTO
    pkt_buf[0] = 0;
    pkt_buf[1] = 0;
    pkt_buf[ECP_SIZE_PROTO] = (pkt_meta->s_idx << 4) | pkt_meta->c_idx;
    pkt_buf += 3;

    if (pkt_meta->public) {
        memcpy(pkt_buf, &pkt_meta->public, ECP_SIZE_ECDH_PUB);
        pkt_buf += ECP_SIZE_ECDH_PUB;
    }
    if (pkt_meta->cookie) {
        memcpy(pkt_buf, pkt_meta->cookie, ECP_SIZE_COOKIE);
        pkt_buf += ECP_SIZE_COOKIE;
    }
    ecp_nonce2buf(pkt_buf, pkt_meta->nonce);
    pkt_buf += ECP_SIZE_NONCE;

    hdr_size = pkt_buf - packet->buffer;
    rv = ecp_aead_enc(pkt_buf, packet->size-hdr_size, payload->buffer, pld_size, pkt_meta->shkey, pkt_meta->nonce, pkt_meta->ntype);
    if (rv < 0) return rv;

    return rv+hdr_size;
}

static ssize_t _pack_conn(ECPConnection *conn, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, unsigned char *cookie, ecp_nonce_t *_nonce, ECPBuffer *payload, size_t pld_size, ecp_tr_addr_t *addr) {
    ECPPktMeta pkt_meta;
    ecp_ecdh_public_t public;
    ecp_aead_key_t shkey;
    ecp_nonce_t nonce;
    int rv = ECP_OK;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif

    if (s_idx == ECP_ECDH_IDX_INV) {
        if (ecp_conn_is_inb(conn)) {
            s_idx = conn->key_curr;
        } else {
            s_idx = conn->rkey_curr;
        }
    }
    if (c_idx == ECP_ECDH_IDX_INV) {
        if (ecp_conn_is_outb(conn)) {
            c_idx = conn->key_curr;
        } else {
            c_idx = conn->rkey_curr;
        }
    }
    rv = conn_shkey_get(conn, s_idx, c_idx, &shkey);
    if (rv) goto pack_conn_fin;

    if (ecp_conn_is_outb(conn)) {
        ECPDHKey *key = conn_dhkey_get(conn, c_idx);

        if (key) {
            memcpy(&public, &key->public, sizeof(public));
        } else {
            rv = ECP_ERR_ECDH_IDX;
        }
    } else {
        ECPDHPub *key = conn_dhkey_get_remote(conn, c_idx);

        if (key) {
            memcpy(&public, &key->public, sizeof(public));
        } else {
            rv = ECP_ERR_ECDH_IDX;
        }
        memcpy(&public, &key->public, sizeof(public));
    }
    if (rv) goto pack_conn_fin;

    if (_nonce) {
        nonce = *_nonce;
    } else {
        nonce = conn->nonce_out;
        conn->nonce_out++;
    }
#ifdef ECP_WITH_VCONN
    if ((conn->parent == NULL) && addr) *addr = conn->remote.addr;
#else
    if (addr) *addr = conn->remote.addr;
#endif

pack_conn_fin:

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif
    if (rv) return rv;

    pkt_meta.cookie = cookie;
    pkt_meta.public = &public;
    pkt_meta.shkey = &shkey;
    pkt_meta.nonce = &nonce;
    pkt_meta.ntype = (ecp_conn_is_inb(conn) ? ECP_NTYPE_INB : ECP_NTYPE_OUTB);
    pkt_meta.s_idx = s_idx;
    pkt_meta.c_idx = c_idx;
    return _pack(packet, &pkt_meta, payload, pld_size);
}

ssize_t ecp_pack(ECPConnection *parent, ECPBuffer *packet, ECPPktMeta *pkt_meta, ECPBuffer *payload, size_t pld_size, ecp_tr_addr_t *addr) {
    ssize_t rv;

    rv = _pack(packet, pkt_meta, payload, pld_size);
    if (rv < 0) return rv;

#ifdef ECP_WITH_VCONN
    if (parent) {
        rv = ecp_vconn_pack_parent(parent, packet, payload, rv, addr);
    }
#endif

    return rv;
}

ssize_t ecp_pack_conn(ECPConnection *conn, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, unsigned char *cookie, ecp_nonce_t *nonce, ECPBuffer *payload, size_t pld_size, ecp_tr_addr_t *addr) {
    ssize_t rv;

    rv = _pack_conn(conn, packet, s_idx, c_idx, cookie, nonce, payload, pld_size, addr);
    if (rv < 0) return rv;

#ifdef ECP_WITH_VCONN
    if (conn->parent) {
        rv = ecp_vconn_pack_parent(conn->parent, packet, payload, rv, addr);
    }
#endif

    return rv;
}

int ecp_pld_get_type(unsigned char *pld, size_t pld_size, unsigned char *mtype) {
    if (pld_size < ECP_SIZE_MTYPE) return ECP_ERR_SIZE;

    *mtype = pld[0];
    return ECP_OK;
}

int ecp_pld_set_type(unsigned char *pld, size_t pld_size, unsigned char mtype) {
    if (pld_size < ECP_SIZE_MTYPE) return ECP_ERR_SIZE;

    pld[0] = mtype;
    return ECP_OK;
}

int ecp_pld_get_frag(unsigned char *pld, size_t pld_size, unsigned char *frag_cnt, unsigned char *frag_tot, uint16_t *frag_size) {
    if (pld_size < ECP_SIZE_MTYPE) return ECP_ERR_SIZE;

    if (!(pld[0] & ECP_MTYPE_FLAG_FRAG)) return ECP_ERR;
    if (pld_size < (ECP_SIZE_MTYPE + ECP_SIZE_MT_FLAG(pld[0]))) return ECP_ERR_SIZE;
    if (pld[2] == 0) return ECP_ERR;

    *frag_cnt = pld[1];
    *frag_tot = pld[2];
    *frag_size = \
        (pld[3] << 8) | \
        (pld[4]);

    return ECP_OK;
}

int ecp_pld_set_frag(unsigned char *pld, size_t pld_size, unsigned char frag_cnt, unsigned char frag_tot, uint16_t frag_size) {
    if (pld_size < ECP_SIZE_MTYPE) return ECP_ERR_SIZE;

    if (!(pld[0] & ECP_MTYPE_FLAG_FRAG)) return ECP_ERR;
    if (pld_size < (ECP_SIZE_MTYPE + ECP_SIZE_MT_FLAG(pld[0]))) return ECP_ERR_SIZE;

    pld[1] = frag_cnt;
    pld[2] = frag_tot;
    pld[3] = frag_size >> 8;
    pld[4] = frag_size;

    return ECP_OK;
}

int ecp_pld_get_pts(unsigned char *pld, size_t pld_size, ecp_pts_t *pts) {
    size_t offset;

    if (pld_size < ECP_SIZE_MTYPE) return ECP_ERR_SIZE;

    if (!(pld[0] & ECP_MTYPE_FLAG_PTS)) return ECP_ERR;
    if (pld_size < (ECP_SIZE_MTYPE + ECP_SIZE_MT_FLAG(pld[0]))) return ECP_ERR_SIZE;

    offset = ECP_SIZE_MTYPE + ECP_SIZE_MT_FRAG(pld[0]);
    *pts = \
        ((ecp_pts_t)pld[offset]     << 24) | \
        ((ecp_pts_t)pld[offset + 1] << 16) | \
        ((ecp_pts_t)pld[offset + 2] << 8)  | \
        ((ecp_pts_t)pld[offset + 3]);

    return ECP_OK;
}

int ecp_pld_set_pts(unsigned char *pld, size_t pld_size, ecp_pts_t pts) {
    size_t offset;

    if (pld_size < ECP_SIZE_MTYPE) return ECP_ERR_SIZE;

    if (!(pld[0] & ECP_MTYPE_FLAG_PTS)) return ECP_ERR;
    if (pld_size < (ECP_SIZE_MTYPE + ECP_SIZE_MT_FLAG(pld[0]))) return ECP_ERR_SIZE;

    offset = ECP_SIZE_MTYPE + ECP_SIZE_MT_FRAG(pld[0]);
    pld[offset]     = pts >> 24;
    pld[offset + 1] = pts >> 16;
    pld[offset + 2] = pts >> 8;
    pld[offset + 3] = pts;

    return ECP_OK;
}

unsigned char *ecp_pld_get_msg(unsigned char *pld, size_t pld_size) {
    size_t offset;

    if (pld_size < ECP_SIZE_MTYPE) return NULL;
    if (pld_size < (ECP_SIZE_MTYPE + ECP_SIZE_MT_FLAG(pld[0]))) return NULL;

    return pld + ECP_SIZE_MTYPE + ECP_SIZE_MT_FLAG(pld[0]);
}

ssize_t _ecp_pld_send(ECPConnection *conn, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, unsigned char *cookie, ecp_nonce_t *n, ECPBuffer *payload, size_t pld_size, unsigned char flags, ECPTimerItem *ti) {
    ecp_tr_addr_t addr;
    size_t pkt_size;
    ssize_t rv;

    rv = ecp_pack_conn(conn, packet, s_idx, c_idx, cookie, n, payload, pld_size, &addr);
    if (rv < 0) return rv;

    pkt_size = rv;
    rv = ecp_ext_pld_send(conn, payload, pld_size, packet, pkt_size, flags, ti, &addr);
    if (rv) return rv;

    rv = ecp_pkt_send(conn->sock, packet, pkt_size, flags, ti, &addr);
    if (rv < 0) return rv;

    return pld_size;
}

ssize_t ecp_pld_send(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pld_size, unsigned char flags) {
    return _ecp_pld_send(conn, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, NULL, NULL, payload, pld_size, flags, NULL);
}

ssize_t ecp_pld_send_wtimer(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pld_size, unsigned char flags, ECPTimerItem *ti) {
    return _ecp_pld_send(conn, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, NULL, NULL, payload, pld_size, flags, ti);
}

ssize_t ecp_pld_send_wcookie(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pld_size, unsigned char flags, unsigned char *cookie) {
    return _ecp_pld_send(conn, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, cookie, NULL, payload, pld_size, flags, NULL);
}

ssize_t ecp_pld_send_wnonce(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pld_size, unsigned char flags, ecp_nonce_t *nonce) {
    return _ecp_pld_send(conn, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, NULL, nonce, payload, pld_size, flags, NULL);
}

ssize_t ecp_pld_send_irep(ECPSocket *sock, ECPConnection *parent, ecp_tr_addr_t *addr, ECPBuffer *packet, ECPPktMeta *pkt_meta, ECPBuffer *payload, size_t pld_size, unsigned char flags) {
    ecp_tr_addr_t _addr;
    ssize_t rv;

    rv = ecp_pack(parent, packet, pkt_meta, payload, pld_size, addr ? NULL : &_addr);
    if (rv < 0) return rv;

    rv = ecp_pkt_send(sock, packet, rv, flags, NULL, addr ? addr : &_addr);
    if (rv < 0) return rv;

    return pld_size;
}

ssize_t ecp_msg_send(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_MAX_PKT];
    unsigned char pld_buf[ECP_MAX_PLD];
    unsigned char *msg_buf;
    ssize_t rv;

    packet.buffer = pkt_buf;
    packet.size = ECP_MAX_PKT;
    payload.buffer = pld_buf;
    payload.size = ECP_MAX_PLD;

    rv = ecp_ext_msg_send(conn, mtype, msg, msg_size, &packet, &payload);
    if (rv) return rv;

    if (ECP_SIZE_PKT_BUF(msg_size, mtype, conn) <= ECP_MAX_PKT) {
        ecp_pld_set_type(pld_buf, ECP_MAX_PLD, mtype);
        msg_buf = ecp_pld_get_msg(pld_buf, ECP_MAX_PLD);

        memcpy(msg_buf, msg, msg_size);
        rv = ecp_pld_send(conn, &packet, &payload, ECP_SIZE_PLD(msg_size, mtype), 0);
        if (rv < 0) return rv;
    } else {
        return ECP_ERR_SIZE;
    }

    return msg_size;
}

static int recv_p(ECPSocket *sock, ecp_tr_addr_t *addr, ECPBuffer *packet, size_t size) {
    ECP2Buffer bufs;
    ECPBuffer payload;
    unsigned char pld_buf[ECP_MAX_PLD];
    ssize_t rv;

    bufs.packet = packet;
    bufs.payload = &payload;

    payload.buffer = pld_buf;
    payload.size = ECP_MAX_PLD;

    rv = ecp_pkt_handle(sock, NULL, addr, &bufs, size);
    if (rv < 0) return rv;

    return ECP_OK;
}

int ecp_receiver(ECPSocket *sock) {
    ecp_tr_addr_t addr;
    ECPBuffer packet;
    unsigned char pkt_buf[ECP_MAX_PKT];
    ecp_sts_t next = 0;
    ssize_t rv;
    int _rv;

    sock->running = 1;
    while(sock->running) {
        packet.buffer = pkt_buf;
        packet.size = ECP_MAX_PKT;

        rv = ecp_tr_recv(sock, &packet, &addr, next ? next : ECP_POLL_TIMEOUT);
        if (rv > 0) {
            _rv = recv_p(sock, &addr, &packet, rv);
#ifdef ECP_DEBUG
            if (_rv) {
                printf("RCV ERR:%d\n", _rv);
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
#endif
