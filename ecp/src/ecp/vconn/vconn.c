#include <stdlib.h>
#include <string.h>

#include <core.h>

#include "vconn.h"

ssize_t ecp_vconn_send_open_req(ECPConnection *conn, unsigned char *cookie) {
    if (conn->type == ECP_CTYPE_VCONN) {
        ECPDHPub *key;
        ECPBuffer packet;
        ECPBuffer payload;
        unsigned char pkt_buf[ECP_SIZE_PKT_BUF_WCOOKIE(2+ECP_SIZE_VBOX, ECP_MTYPE_OPEN_REQ, conn)+ECP_SIZE_PLD(ECP_SIZE_ECDH_PUB, ECP_MTYPE_NEXT)];
        unsigned char pld_buf[ECP_SIZE_PLD_BUF(2+ECP_SIZE_VBOX, ECP_MTYPE_OPEN_REQ, conn)+ECP_SIZE_PLD(ECP_SIZE_ECDH_PUB, ECP_MTYPE_NEXT)];
        unsigned char *msg;
        unsigned char *_pld_buf;
        size_t _pld_size;
        ssize_t rv;

        if (conn->next == NULL) return ECP_ERR;

        key = &conn->next->remote.key_perma;
        if (!key->valid) return ECP_ERR;

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
        memcpy(msg, &key->public, ECP_SIZE_ECDH_PUB);

        rv = ecp_pld_send_wcookie(conn, &packet, &payload, rv+ECP_SIZE_PLD(ECP_SIZE_ECDH_PUB, ECP_MTYPE_NEXT), 0, cookie);
        return rv;
    } else {
        return ecp_send_open_req(conn, cookie);
    }
}

static void insert_key_next(ECPConnection *conn, unsigned char idx, ecp_ecdh_public_t *public) {
    ECPVConn *_conn = (ECPVConn *)conn;
    ecp_ecdh_public_t to_remove;
    unsigned char c_idx;
    int do_insert = 0, do_remove = 0;

    if (idx == ECP_ECDH_IDX_INV) return;

    c_idx = (idx & 0x0F);
    if (c_idx & ~ECP_ECDH_IDX_MASK) return;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif

    if (c_idx != _conn->key_next_curr) {
        ECPDHPub *key;
        unsigned char _c_idx;

        _c_idx = c_idx % ECP_MAX_NODE_KEY;
        key = &_conn->key_next[_c_idx];

        if (!key->valid || (memcmp(public, &key->public, sizeof(key->public)) != 0)) {
            if (key->valid) {
                memcpy(&to_remove, &key->public, sizeof(key->public));
                do_remove = 1;
            }
            memcpy(&key->public, public, sizeof(key->public));
            key->valid = 1;

            _conn->key_next_curr = c_idx;
            do_insert = 1;
        }
    }

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    if (do_remove) ecp_sock_keys_remove(conn->sock, &to_remove);
    if (do_insert) ecp_sock_keys_insert(conn->sock, public, conn);
}

static ssize_t handle_next(ECPConnection *conn, unsigned char *msg, size_t msg_size, ECP2Buffer *b) {
    ECPConnection *conn_next;

    if (msg_size < ECP_SIZE_ECDH_PUB) return ECP_ERR_SIZE;

    conn_next = ecp_sock_keys_search(conn->sock, (ecp_ecdh_public_t *)msg);
    if (conn_next == NULL) return ECP_ERR;

    conn->next = conn_next;

    return ECP_SIZE_ECDH_PUB;
}

static ssize_t handle_exec(ECPConnection *conn, unsigned char *msg, size_t msg_size, ECP2Buffer *b) {
    ECPBuffer *packet;

    if (b == NULL) return ECP_ERR;

    packet = b->packet;
    if (packet->size < msg_size) return ECP_ERR_SIZE;

    memcpy(packet->buffer, msg, msg_size);
    return ecp_pkt_handle(conn->sock, conn, NULL, b, msg_size);
}

static ssize_t handle_relay(ECPConnection *conn, unsigned char *msg, size_t msg_size, ECP2Buffer *b) {
    ECPBuffer payload;
    ECPConnection *conn_next;
    ssize_t rv;

    if (msg_size < ECP_MIN_PKT) return ECP_ERR_SIZE;

    if (conn->type == ECP_CTYPE_VCONN) {
        if (ecp_conn_is_outb(conn)) return ECP_ERR;
        conn_next = conn->next;
        insert_key_next(conn, msg[ECP_SIZE_PROTO], (ecp_ecdh_public_t *)(msg+ECP_SIZE_PROTO+1));
    } else if (conn->type == ECP_CTYPE_VLINK) {
        conn_next = ecp_sock_keys_search(conn->sock, (ecp_ecdh_public_t *)(msg+ECP_SIZE_PROTO+1));
    }

    if (conn_next == NULL) return ECP_ERR;

    payload.buffer = msg - ECP_SIZE_MTYPE;
    payload.size = b->payload->size - (payload.buffer - b->payload->buffer);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_EXEC);
    rv = ecp_pld_send(conn_next, b->packet, &payload, ECP_SIZE_MTYPE + msg_size, ECP_SEND_FLAG_REPLY);

    if (conn->type == ECP_CTYPE_VLINK) {
        ecp_conn_refcount_dec(conn_next);
    }

    if (rv < 0) return rv;
    return msg_size;
}

ssize_t ecp_vconn_pack_parent(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pkt_size, ecp_tr_addr_t *addr) {
    unsigned char *msg;
    size_t hdr_size;
    int rv;

    rv = ecp_pld_set_type(payload->buffer, payload->size, ECP_MTYPE_RELAY);
    if (rv) return rv;

    msg = ecp_pld_get_msg(payload->buffer, payload->size);
    if (msg == NULL) return ECP_ERR;

    hdr_size = msg - payload->buffer;
    if (payload->size < pkt_size+hdr_size) return ECP_ERR_SIZE;

    memcpy(msg, packet->buffer, pkt_size);
    return ecp_pack_conn(conn, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, NULL, NULL, payload, pkt_size+hdr_size, addr);
}

int ecp_vconn_handle_open(ECPConnection *conn, ECP2Buffer *b) {
    if (conn->type == ECP_CTYPE_VCONN) {
        if (ecp_conn_is_outb(conn)) {
            if (conn->next) {
                int rv;

                rv = ecp_conn_open(conn->next, NULL);
                if (rv) ecp_err_handle(conn->next, ECP_MTYPE_INIT_REQ, rv);
            }
        }
    } else if (conn->type == ECP_CTYPE_VLINK) {
        ECPDHPub *key = &conn->remote.key_perma;

        if (key->valid) {
            int rv;

            rv = ecp_sock_keys_insert(conn->sock, &key->public, conn);
            if (rv) return rv;
        }
    }

    return ECP_OK;
}

void ecp_vconn_handle_close(ECPConnection *conn) {
    if (conn->type == ECP_CTYPE_VCONN) {
        if (ecp_conn_is_inb(conn)) {
            ecp_vconn_destroy_inb((ECPVConn *)conn);
        }
    } else if (conn->type == ECP_CTYPE_VLINK) {
        ECPDHPub *key = &conn->remote.key_perma;

        if (key->valid) {
            ecp_sock_keys_remove(conn->sock, &key->public);
        }
    }
}

ssize_t ecp_vconn_handle_msg(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *b) {
    switch (mtype) {
        case ECP_MTYPE_NEXT:
            return handle_next(conn, msg, msg_size, b);

        case ECP_MTYPE_EXEC:
            return handle_exec(conn, msg, msg_size, b);

        case ECP_MTYPE_RELAY:
            return handle_relay(conn, msg, msg_size, b);

        default:
            return ECP_ERR_MTYPE;
    }
}

int ecp_vconn_create_inb(ECPVConn *conn, ECPSocket *sock) {
    ECPConnection *_conn = &conn->b;
    int rv;

    rv = ecp_conn_create_inb(_conn, sock, ECP_CTYPE_VCONN);
    if (rv) return rv;

    memset(conn->key_next, 0, sizeof(conn->key_next));
    conn->key_next_curr = ECP_ECDH_IDX_INV;

    return ECP_OK;
}

void ecp_vconn_destroy_inb(ECPVConn *conn) {
    ECPConnection *_conn = &conn->b;
    int i;

    for (i=0; i<ECP_MAX_NODE_KEY; i++) {
        ECPDHPub *key;

        key = &conn->key_next[i];
        if (key->valid) {
            ecp_sock_keys_remove(_conn->sock, &key->public);
        }
    }
    if (_conn->next) ecp_conn_refcount_dec(_conn->next);
}

static int vconn_create(ECPConnection vconn[], ecp_ecdh_public_t keys[], size_t vconn_size, ECPSocket *sock) {
    ECPDHPub key;
    int i, j;
    int rv;

    key.valid = 1;
    for (i=0; i<vconn_size; j++) {
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

    if (pcount >= ECP_MAX_PARENT) return ECP_ERR_MAX_PARENT;

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

/*
int ecp_vconn_create_parent(ECPConnection *conn, ECPNode *conn_node, ECPVConnOut vconn[], ECPNode vconn_node[], int size) {
    ECPSocket *sock = conn->sock;
    int i, j, rv;

    conn->parent = (ECPConnection *)&vconn[size-1];
    conn->pcount = size;
    for (i=0; i<size; i++) {
        rv = ecp_conn_init((ECPConnection *)&vconn[i], sock, ECP_CTYPE_VCONN);
        if (!rv) rv = ecp_conn_create_outb((ECPConnection *)&vconn[i], &vconn_node[i]);
        if (!rv) {
            rv = ecp_conn_insert((ECPConnection *)&vconn[i]);
            if (rv) ecp_conn_destroy((ECPConnection *)&vconn[i]);
        }
        if (rv) {
            for (j=0; j<i; j++) ecp_conn_close((ECPConnection *)&vconn[j]);
            return rv;
        }

        if (i == 0) {
            vconn[i].b.parent = NULL;
        } else {
            vconn[i].b.parent = (ECPConnection *)&vconn[i-1];
        }
        vconn[i].b.pcount = i;

        if (i == size - 1) {
            vconn[i].next = conn;
        } else {
            vconn[i].next = (ECPConnection *)&vconn[i+1];
        }
    }

    return ECP_OK;
}

int ecp_vconn_open(ECPConnection *conn, ECPNode *conn_node, ECPVConnOut vconn[], ECPNode vconn_node[], int size) {
    int rv;
    ssize_t _rv;

    rv = ecp_conn_create_outb(conn, conn_node);
    if (rv) return rv;

    rv = ecp_conn_insert(conn);
    if (rv) {
        ecp_conn_destroy(conn);
        return rv;
    }

    rv = ecp_vconn_create_parent(conn, conn_node, vconn, vconn_node, size);
    if (rv) {
        ecp_conn_close(conn);
        return rv;
    }

    _rv = ecp_timer_send((ECPConnection *)&vconn[0], _vconn_send_kget, ECP_MTYPE_KGET_REP, 3, 500);
    if (_rv < 0) return _rv;

    return ECP_OK;
}
*/