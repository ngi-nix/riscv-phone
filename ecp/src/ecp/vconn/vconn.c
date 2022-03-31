#include <stdlib.h>
#include <string.h>

#include <core.h>

#ifdef ECP_WITH_HTABLE
#include <ht.h>
#endif

#include "vconn.h"


#ifdef ECP_WITH_HTABLE

static int insert_key_next(ECPConnection *conn, unsigned char c_idx, ecp_ecdh_public_t *public) {
    ECPSocket *sock = conn->sock;
    ECPVConn *_conn = (ECPVConn *)conn;
    ECPDHPub *key = NULL;
    int rv = ECP_OK;

    if (c_idx & ~ECP_ECDH_IDX_MASK) return ECP_ERR_ECDH_IDX;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif

    if (c_idx != _conn->key_next_curr) {
        unsigned char _c_idx;

        _c_idx = c_idx % ECP_MAX_NODE_KEY;
        key = &_conn->key_next[_c_idx];

        if (key->valid && (memcmp(public, &key->public, sizeof(key->public)) == 0)) {
            key = NULL;
        }
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    if (key) {
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&sock->conn_table.mutex);
        pthread_mutex_lock(&conn->mutex);
#endif

        if (key->valid) {
            ecp_ht_remove(sock->conn_table.keys, &key->public);
            key->valid = 0;
        }
        memcpy(&key->public, public, sizeof(key->public));
        rv = ecp_ht_insert(sock->conn_table.keys, &key->public, conn);
        if (!rv) {
            key->valid = 1;
            _conn->key_next_curr = c_idx;
        }

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn->mutex);
        pthread_mutex_unlock(&sock->conn_table.mutex);
#endif
    }

    return rv;
}

static ssize_t handle_next(ECPConnection *conn, unsigned char *msg, size_t msg_size, ECP2Buffer *b) {
    ECPSocket *sock = conn->sock;

    if (msg_size < ECP_SIZE_ECDH_PUB) return ECP_ERR_SIZE;
    if (ecp_conn_is_outb(conn)) return ECP_ERR;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn_table.mutex);
#endif

    conn->next = ecp_ht_search(sock->conn_table.keys, (ecp_ecdh_public_t *)msg);
    if (conn->next) ecp_conn_refcount_inc(conn->next);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&sock->conn_table.mutex);
#endif

    if (conn->next == NULL) return ECP_ERR;

    return ECP_SIZE_ECDH_PUB;
}

static ssize_t handle_relay(ECPConnection *conn, unsigned char *msg, size_t msg_size, ECP2Buffer *b) {
    ECPSocket *sock = conn->sock;
    ECPConnection *conn_next;
    ECPBuffer payload;
    unsigned char idx, _idx;
    unsigned char s_idx, c_idx;
    size_t _msg_size = msg_size;
    ssize_t rv;
    int _rv;

    if (msg_size < ECP_MIN_PKT) return ECP_ERR_SIZE;

    idx = msg[ECP_SIZE_PROTO];
    if (idx == ECP_ECDH_IDX_INV) return ECP_ERR_ECDH_IDX;

    switch (conn->type) {
        /* forward message */
        case ECP_CTYPE_VCONN: {
            if (ecp_conn_is_outb(conn)) return ECP_ERR;

            conn_next = conn->next;
            if (conn_next) {
                _idx = (idx & 0x0F);

                _rv = insert_key_next(conn, _idx, (ecp_ecdh_public_t *)(msg+ECP_SIZE_PROTO+1));
                if (_rv) return _rv;
            }

            break;
        }

        /* back message */
        case ECP_CTYPE_VLINK: {
#ifdef ECP_WITH_PTHREAD
            pthread_mutex_lock(&sock->conn_table.mutex);
#endif

            conn_next = ecp_ht_search(sock->conn_table.keys, (ecp_ecdh_public_t *)(msg+ECP_SIZE_PROTO+1));
            if (conn_next) ecp_conn_refcount_inc(conn_next);

#ifdef ECP_WITH_PTHREAD
            pthread_mutex_unlock(&sock->conn_table.mutex);
#endif

            _idx = (idx & 0xF0) >> 4;
            if (conn_next && (_idx == ECP_ECDH_IDX_PERMA)) {
                /* this is init reply */
                msg[ECP_SIZE_PROTO] = ECP_ECDH_IDX_INV;
                memmove(msg+ECP_SIZE_PROTO+1, msg+ECP_SIZE_PROTO+1+ECP_SIZE_ECDH_PUB, msg_size-(ECP_SIZE_PROTO+1+ECP_SIZE_ECDH_PUB));
                _msg_size -= ECP_SIZE_ECDH_PUB;
            }

            break;
        }

        default:
            return ECP_ERR_CTYPE;

    }

    if (conn_next == NULL) return ECP_ERR;

    payload.buffer = msg - ECP_SIZE_MTYPE;
    payload.size = b->payload->size - (payload.buffer - b->payload->buffer);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_EXEC);
    rv = ecp_pld_send(conn_next, b->packet, &payload, ECP_SIZE_MTYPE + _msg_size, ECP_SEND_FLAG_REPLY);

    if (conn->type == ECP_CTYPE_VLINK) {
        ecp_conn_refcount_dec(conn_next);
    }

    if (rv < 0) return rv;
    return msg_size;
}

#endif  /* ECP_WITH_HTABLE */

static ssize_t handle_exec(ECPConnection *conn, unsigned char *msg, size_t msg_size, ECP2Buffer *b) {
    ECPBuffer *packet;

    if (b == NULL) return ECP_ERR;

    packet = b->packet;
    if (packet->size < msg_size) return ECP_ERR_SIZE;

    memcpy(packet->buffer, msg, msg_size);
    return ecp_pkt_handle(conn->sock, conn, NULL, b, msg_size);
}

ssize_t ecp_vconn_pack_parent(ECPConnection *conn, ECPBuffer *payload, ECPBuffer *packet, size_t pkt_size, ecp_tr_addr_t *addr) {
    unsigned char *msg;
    size_t hdr_size;
    ssize_t rv;
    int _rv;

    _rv = ecp_pld_set_type(payload->buffer, payload->size, ECP_MTYPE_RELAY);
    if (_rv) return _rv;

    msg = ecp_pld_get_msg(payload->buffer, payload->size);
    if (msg == NULL) return ECP_ERR;

    hdr_size = msg - payload->buffer;
    if (payload->size < pkt_size+hdr_size) return ECP_ERR_SIZE;

    memcpy(msg, packet->buffer, pkt_size);
    rv = ecp_pack_conn(conn, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, NULL, NULL, payload, pkt_size+hdr_size, addr);
    return rv;
}

static int vconn_create(ECPConnection vconn[], ecp_ecdh_public_t keys[], size_t vconn_size, ECPSocket *sock) {
    ECPDHPub key;
    int i, j;
    int rv;

    key.valid = 1;
    for (i=0; i<vconn_size; i++) {
        rv = ecp_conn_create(&vconn[i], sock, ECP_CTYPE_VCONN);
        if (rv) {
            for (j=0; j<i; j++) {
                ecp_conn_destroy(&vconn[j]);
            }
            return rv;
        }
        memcpy(&key.public, &keys[i], sizeof(keys[i]));
        ecp_conn_set_remote_key(&vconn[i], &key);
    }

    return ECP_OK;
}

int ecp_vconn_create(ECPConnection vconn[], ecp_ecdh_public_t keys[], size_t vconn_size, ECPConnection *conn) {
    ECPConnection *_conn;
    unsigned short pcount;
    int rv;

    _conn = conn;
    pcount = vconn_size;

    if (pcount > ECP_MAX_PARENT) return ECP_ERR_MAX_PARENT;

    rv = vconn_create(vconn, keys, vconn_size, conn->sock);
    if (rv) return rv;

    while (pcount) {
        _conn->parent = &vconn[pcount-1];
        _conn->parent->next = _conn;
        _conn->pcount = pcount;

        _conn = _conn->parent;
        pcount--;
    }

    return ECP_OK;
}

#ifdef ECP_WITH_HTABLE

int ecp_vconn_create_inb(ECPVConn *conn, ECPSocket *sock) {
    ECPConnection *_conn = &conn->b;
    int rv;

    rv = ecp_conn_create_inb(_conn, sock, ECP_CTYPE_VCONN);
    if (rv) return rv;

    memset(conn->key_next, 0, sizeof(conn->key_next));
    conn->key_next_curr = ECP_ECDH_IDX_INV;

    return ECP_OK;
}

void ecp_vconn_destroy(ECPConnection *conn) {
    ECPSocket *sock = conn->sock;
    ECPVConn *_conn = (ECPVConn *)conn;
    int i;

    if (ecp_conn_is_outb(conn)) return;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn_table.mutex);
#endif

    for (i=0; i<ECP_MAX_NODE_KEY; i++) {
        ECPDHPub *key;

        key = &_conn->key_next[i];
        if (key->valid) {
            ecp_ht_remove(sock->conn_table.keys, &key->public);
        }
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&sock->conn_table.mutex);
#endif

    if (conn->next) ecp_conn_refcount_dec(conn->next);
}

#endif  /* ECP_WITH_HTABLE */

int ecp_vconn_open(ECPConnection *conn, ECPNode *node) {
    ECPConnection *vconn0;
    int rv;

    vconn0 = conn;
    while (vconn0->parent) {
        vconn0 = vconn0->parent;
    }

    if (node) {
        ecp_conn_set_remote_key(conn, &node->key_perma);
        ecp_conn_set_remote_addr(vconn0, &node->addr);
    }

    rv = ecp_conn_open(vconn0, NULL);
    return rv;
}

static int vconn_handle_open(ECPConnection *conn) {
    int rv = ECP_OK;

    if (ecp_conn_is_outb(conn) && conn->next) {
        rv = ecp_conn_open(conn->next, NULL);
        if (rv) ecp_err_handle(conn->next, ECP_MTYPE_INIT_REQ, rv);
    }

    return rv;;
}

int ecp_vlink_create(ECPConnection *conn, ECPSocket *sock) {
    int rv;

    rv = ecp_conn_create(conn, sock, ECP_CTYPE_VLINK);
    if (!rv) ecp_conn_set_flags(conn, ECP_CONN_FLAG_VBOX);
    return rv;
}

#ifdef ECP_WITH_HTABLE

int ecp_vlink_create_inb(ECPConnection *conn, ECPSocket *sock) {
    int rv;

    rv = ecp_conn_create_inb(conn, sock, ECP_CTYPE_VLINK);
    if (!rv) ecp_conn_set_flags(conn, ECP_CONN_FLAG_VBOX);
    return rv;
}

void ecp_vlink_destroy(ECPConnection *conn) {
    ECPSocket *sock = conn->sock;
    ECPDHPub *key = &conn->remote.key_perma;

    if (key->valid) {
#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&sock->conn_table.mutex);
#endif

        ecp_ht_remove(sock->conn_table.keys, &key->public);

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&sock->conn_table.mutex);
#endif
    }
}

static int vlink_handle_open(ECPConnection *conn) {
    ECPSocket *sock = conn->sock;
    ECPDHPub *key;
    int rv = ECP_OK;

    key = &conn->remote.key_perma;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&sock->conn_table.mutex);
    pthread_mutex_lock(&conn->mutex);
#endif

    if (key->valid) {
        ECPConnection *_conn;

        /* check for duplicates */
        _conn = ecp_ht_search(sock->conn_table.keys, &key->public);
        if (_conn) {
#ifdef ECP_WITH_PTHREAD
            pthread_mutex_lock(&_conn->mutex);
#endif

            _conn->remote.key_perma.valid = 0;

#ifdef ECP_WITH_PTHREAD
            pthread_mutex_unlock(&_conn->mutex);
#endif

            ecp_ht_remove(sock->conn_table.keys, &key->public);
        }
        rv = ecp_ht_insert(sock->conn_table.keys, &key->public, conn);
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
    pthread_mutex_unlock(&sock->conn_table.mutex);
#endif

    return rv;
}

#endif  /* ECP_WITH_HTABLE */

int ecp_vconn_handle_open(ECPConnection *conn, ECP2Buffer *b) {
    int rv;

    switch (conn->type) {
        case ECP_CTYPE_VCONN:
            rv = vconn_handle_open(conn);
            break;

        case ECP_CTYPE_VLINK:
#ifdef ECP_WITH_HTABLE
            rv = vlink_handle_open(conn);
#else
            rv = ECP_OK;
#endif
            break;

        default:
            rv = ECP_ERR_CTYPE;
            break;
    }

    return rv;
}

void ecp_vconn_handle_close(ECPConnection *conn) {
    switch (conn->type) {
#ifdef ECP_WITH_HTABLE
        case ECP_CTYPE_VCONN:
            ecp_vconn_destroy(conn);
            break;

        case ECP_CTYPE_VLINK:
            ecp_vlink_destroy(conn);
            break;
#endif
    }
}

ssize_t ecp_vconn_handle_msg(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *b) {
    ssize_t rv;

    switch (mtype) {
#ifdef ECP_WITH_HTABLE
        case ECP_MTYPE_NEXT:
            rv = handle_next(conn, msg, msg_size, b);
            break;

        case ECP_MTYPE_RELAY:
            rv = handle_relay(conn, msg, msg_size, b);
            break;
#endif

        case ECP_MTYPE_EXEC:
            rv = handle_exec(conn, msg, msg_size, b);
            break;

        default:
            rv = ECP_ERR_MTYPE;
            break;
    }

    return rv;
}

ssize_t ecp_vconn_send_open_req(ECPConnection *conn, unsigned char *cookie) {
    if (conn->type == ECP_CTYPE_VCONN) {
        ECPDHPub key_next;
        ECPBuffer packet;
        ECPBuffer payload;
        unsigned char pkt_buf[ECP_SIZE_PKT_BUF(2+ECP_SIZE_VBOX, ECP_MTYPE_OPEN_REQ, conn)+ECP_SIZE_COOKIE+ECP_SIZE_PLD(ECP_SIZE_ECDH_PUB, ECP_MTYPE_NEXT)];
        unsigned char pld_buf[ECP_SIZE_PLD_BUF(2+ECP_SIZE_VBOX, ECP_MTYPE_OPEN_REQ, conn)+ECP_SIZE_COOKIE+ECP_SIZE_PLD(ECP_SIZE_ECDH_PUB, ECP_MTYPE_NEXT)];
        unsigned char *msg;
        unsigned char *_pld_buf;
        size_t _pld_size;
        ssize_t rv;
        int _rv;

        if (conn->next == NULL) return ECP_ERR;

        _rv = ecp_conn_dhkey_get_remote(conn->next, ECP_ECDH_IDX_PERMA, &key_next);
        if (_rv) return _rv;

        packet.buffer = pkt_buf;
        packet.size = sizeof(pkt_buf);
        payload.buffer = pld_buf;
        payload.size = sizeof(pld_buf);

        rv = ecp_write_open_req(conn, &payload);
        if (rv < 0) return rv;

        _pld_buf = payload.buffer + rv;
        _pld_size = payload.size - rv;

        ecp_pld_set_type(_pld_buf, _pld_size, ECP_MTYPE_NEXT);
        msg = ecp_pld_get_msg(_pld_buf, _pld_size);
        memcpy(msg, &key_next.public, ECP_SIZE_ECDH_PUB);

        rv = ecp_pld_send_wcookie(conn, &packet, &payload, rv+ECP_SIZE_PLD(ECP_SIZE_ECDH_PUB, ECP_MTYPE_NEXT), 0, cookie);
        return rv;
    } else {
        return ecp_send_open_req(conn, cookie);
    }
}
