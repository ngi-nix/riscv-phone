#include <core.h>

#ifdef ECP_WITH_HTABLE
#include <ht.h>
#endif
#include <cr.h>

#include "vconn.h"

#ifdef ECP_WITH_HTABLE
static void *key_perma_table;
static void *key_next_table;
#ifdef ECP_WITH_PTHREAD
static pthread_mutex_t key_perma_mutex;
static pthread_mutex_t key_next_mutex;
#endif
#endif

static unsigned char key_null[ECP_ECDH_SIZE_KEY] = { 0 };

static ECPConnHandler handler_vc;
static ECPConnHandler handler_vl;

#ifdef ECP_WITH_HTABLE

static int vconn_create(ECPConnection *conn, unsigned char *payload, size_t size) {
    ECPVConnIn *conn_v = (ECPVConnIn *)conn;
    int rv;

    if (conn->out) return ECP_ERR;
    if (conn->type != ECP_CTYPE_VCONN) return ECP_ERR;
    if (size < 2*ECP_ECDH_SIZE_KEY) return ECP_ERR;

    conn_v->key_next_curr = 0;
    memset(conn_v->key_next, 0, sizeof(conn_v->key_next));
    memset(conn_v->key_out, 0, sizeof(conn_v->key_out));
    memcpy(conn_v->key_next[conn_v->key_next_curr], payload, ECP_ECDH_SIZE_KEY);
    memcpy(conn_v->key_out, payload+ECP_ECDH_SIZE_KEY, ECP_ECDH_SIZE_KEY);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&key_next_mutex);
#endif
    rv = ecp_ht_insert(key_next_table, conn_v->key_next[conn_v->key_next_curr], conn);
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&key_next_mutex);
#endif

    return rv;
}

static void vconn_destroy(ECPConnection *conn) {
    ECPVConnIn *conn_v = (ECPVConnIn *)conn;

    if (conn->out) return;
    if (conn->type != ECP_CTYPE_VCONN) return;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&key_next_mutex);
    pthread_mutex_lock(&conn->mutex);
#endif
    int i;
    for (i=0; i<ECP_MAX_NODE_KEY; i++) {
        if (memcmp(conn_v->key_next[i], key_null, ECP_ECDH_SIZE_KEY)) ecp_ht_remove(key_next_table, conn_v->key_next[i]);
    }
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
    pthread_mutex_unlock(&key_next_mutex);
#endif
}

#endif  /* ECP_WITH_HTABLE */

static ssize_t _vconn_send_open(ECPConnection *conn, ECPTimerItem *ti) {
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

static ssize_t vconn_open(ECPConnection *conn) {
    ECPTimerItem ti;
    ECPVConnection *conn_v = (ECPVConnection *)conn;
    ECPConnection *conn_next = conn_v->next;
    ssize_t rv

    if (conn_next == NULL) return ECP_ERR;

    rv = ecp_timer_send(conn_next, _vconn_send_open, ECP_MTYPE_KGET_REP, 3, 1000);
    if (rv < 0) return rv;

    return rv;
}

static ssize_t vconn_handle_open(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    if (conn->type != ECP_CTYPE_VCONN) return ECP_ERR;

    if (mtype & ECP_MTYPE_FLAG_REP) {
        if (!conn->out) return ECP_ERR;
        if (size < 0) {
            ecp_conn_handler_msg_t handler = NULL;
            while (conn->type == ECP_CTYPE_VCONN) {
                ECPVConnection *conn_v = (ECPVConnection *)conn;
                conn = conn_v->next;
            }
            handler = conn->sock->ctx->handler[conn->type] ? conn->sock->ctx->handler[conn->type]->msg[ECP_MTYPE_OPEN] : NULL;
            return handler ? handler(conn, seq, mtype, msg, size, b) : size;
        }

        return ecp_conn_handle_open(conn, seq, mtype, msg, size, b);
    } else {
        int rv = ECP_OK;

#ifdef ECP_WITH_HTABLE
        ECPVConnIn *conn_v = (ECPVConnIn *)conn;
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
        if (memcmp(conn_v->key_next[conn_v->key_next_curr], msg, ECP_ECDH_SIZE_KEY)) {
            conn_v->key_next_curr = (conn_v->key_next_curr + 1) % ECP_MAX_NODE_KEY;
            if (memcmp(conn_v->key_next[conn_v->key_next_curr], key_null, ECP_ECDH_SIZE_KEY)) ecp_ht_remove(key_next_table, conn_v->key_next[conn_v->key_next_curr]);
            rv = ecp_ht_insert(key_next_table, conn_v->key_next[conn_v->key_next_curr], conn);
            if (!rv) memcpy(conn_v->key_next[conn_v->key_next_curr], msg, ECP_ECDH_SIZE_KEY);
        }

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn->mutex);
        pthread_mutex_unlock(&key_next_mutex);
#endif
        if (rv) return rv;

        return 1+2*ECP_ECDH_SIZE_KEY;
#else   /* ECP_WITH_HTABLE */

        return ECP_ERR_NOT_IMPLEMENTED;

#endif  /* ECP_WITH_HTABLE */
    }

    return ECP_ERR;
}

#ifdef ECP_WITH_HTABLE
static ssize_t vconn_handle_relay(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    ECPConnection *conn_out = NULL;
    ECPVConnIn *conn_v = (ECPVConnIn *)conn;
    ssize_t rv;

    if (conn->out) return ECP_ERR;
    if (conn->type != ECP_CTYPE_VCONN) return ECP_ERR;
    if (b == NULL) return ECP_ERR;

    if (size < 0) return size;
    if (size < ECP_MIN_PKT) return ECP_ERR_MIN_PKT;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&key_perma_mutex);
#endif
    conn_out = ecp_ht_search(key_perma_table, conn_v->key_out);
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

    ECPBuffer payload;
    payload.buffer = msg - ECP_SIZE_PLD_HDR - 1;
    payload.size = b->payload->size - (payload.buffer - b->payload->buffer);

    ecp_pld_set_type(payload.buffer, ECP_MTYPE_EXEC);
    rv = ecp_pld_send(conn_out, b->packet, &payload, ECP_SIZE_PLD_HDR+1+size, ECP_SEND_FLAG_REPLY);

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

static int vlink_insert(ECPConnection *conn) {
    int rv;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&key_perma_mutex);
#endif
    rv = ecp_ht_insert(key_perma_table, ecp_cr_dh_pub_get_buf(&conn->node.public), conn);
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&key_perma_mutex);
#endif

    return rv;
}

static void vlink_remove(ECPConnection *conn) {
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&key_perma_mutex);
#endif
    ecp_ht_remove(key_perma_table, ecp_cr_dh_pub_get_buf(&conn->node.public));
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&key_perma_mutex);
#endif
}

static int vlink_create(ECPConnection *conn, unsigned char *payload, size_t size) {
    if (conn->out) return ECP_ERR;
    if (conn->type != ECP_CTYPE_VLINK) return ECP_ERR;

    // XXX should verify perma_key
    if (size < ECP_ECDH_SIZE_KEY) return ECP_ERR;
    ecp_cr_dh_pub_from_buf(&conn->node.public, payload);

    return vlink_insert(conn);
}

static void vlink_destroy(ECPConnection *conn) {
    if (conn->out) return;
    if (conn->type != ECP_CTYPE_VLINK) return;

    vlink_remove(conn);
}
#endif  /* ECP_WITH_HTABLE */

static ssize_t _vlink_send_open(ECPConnection *conn, ECPTimerItem *ti) {
    ECPSocket *sock = conn->sock;
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_OPEN_REQ, conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_OPEN_REQ, conn)];
    unsigned char *buf = ecp_pld_get_buf(pld_buf, ECP_MTYPE_OPEN_REQ);

    packet.buffer = pkt_buf;
    packet.size = ECP_SIZE_PKT_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_OPEN_REQ, conn);
    payload.buffer = pld_buf;
    payload.size = ECP_SIZE_PLD_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_OPEN_REQ, conn);

    // XXX server should verify perma_key
    ecp_pld_set_type(pld_buf, ECP_MTYPE_OPEN_REQ);
    buf[0] = conn->type;
    memcpy(buf+1, ecp_cr_dh_pub_get_buf(&sock->key_perma.public), ECP_ECDH_SIZE_KEY);

    return ecp_pld_send_wtimer(conn, &packet, &payload, ECP_SIZE_PLD(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_OPEN_REQ), 0, ti);
}

static ssize_t vlink_open(ECPConnection *conn) {
    return ecp_timer_send(conn, _vlink_send_open, ECP_MTYPE_OPEN_REP, 3, 500);
}

static void vlink_close(ECPConnection *conn) {
#ifdef ECP_WITH_HTABLE
    vlink_remove(conn);
#endif  /* ECP_WITH_HTABLE */
}

static ssize_t vlink_handle_open(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    ssize_t rv;
    int is_open;

    if (conn->type != ECP_CTYPE_VLINK) return ECP_ERR;

    if (size < 0) return size;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif
    is_open = ecp_conn_is_open(conn);
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    rv = ecp_conn_handle_open(conn, seq, mtype, msg, size, b);
    if (rv < 0) return rv;

    if (mtype & ECP_MTYPE_FLAG_REP) {
        if (!conn->out) return ECP_ERR;
#ifdef ECP_WITH_HTABLE
        if (!is_open) {
            int _rv;

            _rv = vlink_insert(conn);
            if (_rv) return _rv;
        }
#endif  /* ECP_WITH_HTABLE */
        return rv;
    } else {
#ifdef ECP_WITH_HTABLE
        if (conn->out) return ECP_ERR;
        if (size < rv+ECP_ECDH_SIZE_KEY) return ECP_ERR;

        msg += rv;

        // XXX should verify perma_key
        return rv+ECP_ECDH_SIZE_KEY;

#else   /* ECP_WITH_HTABLE */

        return ECP_ERR;

#endif  /* ECP_WITH_HTABLE */
    }

    return ECP_ERR;
}

#ifdef ECP_WITH_HTABLE
static ssize_t vlink_handle_relay(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    ssize_t rv;

    if (conn->type != ECP_CTYPE_VLINK) return ECP_ERR;
    if (b == NULL) return ECP_ERR;

    if (size < 0) return size;
    if (size < ECP_MIN_PKT) return ECP_ERR_MIN_PKT;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&key_next_mutex);
#endif
    conn = ecp_ht_search(key_next_table, msg+ECP_SIZE_PROTO+1);
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

    ECPBuffer payload;
    payload.buffer = msg - ECP_SIZE_PLD_HDR - 1;
    payload.size = b->payload->size - (payload.buffer - b->payload->buffer);

    ecp_pld_set_type(payload.buffer, ECP_MTYPE_EXEC);
    rv = ecp_pld_send(conn, b->packet, &payload, ECP_SIZE_PLD_HDR+1+size, ECP_SEND_FLAG_REPLY);

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
#endif  /* ECP_WITH_HTABLE */

static ssize_t vconn_set_msg(ECPConnection *conn, ECPBuffer *payload, unsigned char mtype) {
    if (conn->out && (conn->type == ECP_CTYPE_VCONN) && ((mtype == ECP_MTYPE_OPEN_REQ) || (mtype == ECP_MTYPE_KGET_REQ))) {
        ECPVConnection *conn_v = (ECPVConnection *)conn;
        ECPConnection *conn_next = conn_v->next;
        unsigned char *buf = NULL;
        int rv;

        if (payload->size < ECP_SIZE_PLD_HDR + 3 + 2 * ECP_ECDH_SIZE_KEY) return ECP_ERR;
        if (conn_next == NULL) return ECP_ERR;

        ecp_pld_set_type(payload->buffer, ECP_MTYPE_OPEN_REQ);
        buf = ecp_pld_get_buf(payload->buffer, 0);

        buf[0] = ECP_CTYPE_VCONN;
        rv = ecp_conn_dhkey_get_curr(conn_next, NULL, buf+1);
        if (rv) return rv;

        memcpy(buf+1+ECP_ECDH_SIZE_KEY, ecp_cr_dh_pub_get_buf(&conn_next->node.public), ECP_ECDH_SIZE_KEY);
        buf[1+2*ECP_ECDH_SIZE_KEY] = ECP_MTYPE_RELAY;

        return ECP_SIZE_PLD_HDR + 3 + 2 * ECP_ECDH_SIZE_KEY;
    } else {
        if (payload->size < ECP_SIZE_PLD_HDR + 1) return ECP_ERR;

        ecp_pld_set_type(payload->buffer, ECP_MTYPE_RELAY);

        return ECP_SIZE_PLD_HDR + 1;
    }

}

/* Memory limited version: */
ssize_t ecp_pack(ECPConnection *conn, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, ECPBuffer *payload, size_t pld_size, ECPSeqItem *si, ECPNetAddr *addr) {
    if ((packet == NULL) || (packet->buffer == NULL)) return ECP_ERR;
    if ((payload == NULL) || (payload->buffer == NULL)) return ECP_ERR;

    if (conn->parent) {
        unsigned char mtype = ecp_pld_get_type(payload->buffer);
        ssize_t rv, hdr_size = vconn_set_msg(conn->parent, packet, mtype);
        if (hdr_size < 0) return hdr_size;

        rv = ecp_conn_pack(conn, packet->buffer+hdr_size, packet->size-hdr_size, s_idx, c_idx, payload->buffer, pld_size, si, NULL);
        if (rv < 0) return rv;

        if (payload->size < rv+hdr_size) return ECP_ERR;
        memcpy(payload->buffer, packet->buffer, rv+hdr_size);

        return ecp_pack(conn->parent, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, payload, rv+hdr_size, NULL, addr);
    } else {
        return ecp_conn_pack(conn, packet->buffer, packet->size, s_idx, c_idx, payload->buffer, pld_size, si, addr);
    }
}

ssize_t ecp_pack_raw(ECPSocket *sock, ECPConnection *parent, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, ecp_dh_public_t *public, ecp_aead_key_t *shsec, unsigned char *nonce, ecp_seq_t seq, ECPBuffer *payload, size_t pld_size, ECPNetAddr *addr) {
    ECPContext *ctx = sock->ctx;

    if ((packet == NULL) || (packet->buffer == NULL)) return ECP_ERR;
    if ((payload == NULL) || (payload->buffer == NULL)) return ECP_ERR;

    if (parent) {
        unsigned char mtype = ecp_pld_get_type(payload->buffer);
        ssize_t rv, hdr_size = vconn_set_msg(parent, packet, mtype);
        if (hdr_size < 0) return hdr_size;

        rv = ecp_pkt_pack(ctx, packet->buffer+hdr_size, packet->size-hdr_size, s_idx, c_idx, public, shsec, nonce, seq, payload->buffer, pld_size);
        if (rv < 0) return rv;

        if (payload->size < rv+hdr_size) return ECP_ERR;
        memcpy(payload->buffer, packet->buffer, rv+hdr_size);

        return ecp_pack(parent, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, payload, rv+hdr_size, NULL, addr);
    } else {
        return ecp_pkt_pack(ctx, packet->buffer, packet->size, s_idx, c_idx, public, shsec, nonce, seq, payload->buffer, pld_size);
    }
}

/* Non memory limited version: */
/*
static ssize_t ecp_pack(ECPConnection *conn, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, ECPBuffer *payload, size_t pld_size, ECPSeqItem *si, ECPNetAddr *addr) {
    if (conn->parent) {
        ECPBuffer payload_;
        unsigned char pld_buf[ECP_MAX_PLD];

        payload_.buffer = pld_buf;
        payload_.size = ECP_MAX_PLD;

        unsigned char mtype = ecp_pld_get_type(payload->buffer);
        ssize_t rv, hdr_size = vconn_set_msg(conn->parent, &payload_, mtype);
        if (hdr_size < 0) return hdr_size;

        rv = ecp_conn_pack(conn, payload_.buffer+hdr_size, payload_.size-hdr_size, s_idx, c_idx, payload->buffer, pld_size, si, NULL);
        if (rv < 0) return rv;

        return ecp_pack(conn->parent, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, &payload_, rv+hdr_size, NULL, addr);
    } else {
        return ecp_conn_pack(conn, packet->buffer, packet->size, s_idx, c_idx, payload->buffer, pld_size, si, addr);
    }
}

static ssize_t ecp_pack_raw(ECPSocket *sock, ECPConnection *parent, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, ecp_dh_public_t *public, ecp_aead_key_t *shsec, unsigned char *nonce, ecp_seq_t seq, ECPBuffer *payload, size_t pld_size, ECPNetAddr *addr) {
    ECPContext *ctx = sock->ctx;

    if (parent) {
        ECPBuffer payload_;
        unsigned char pld_buf[ECP_MAX_PLD];

        payload_.buffer = pld_buf;
        payload_.size = ECP_MAX_PLD;

        unsigned char mtype = ecp_pld_get_type(payload->buffer);
        ssize_t rv, hdr_size = vconn_set_msg(parent, &payload_, mtype);
        if (hdr_size < 0) return hdr_size;

        rv = ecp_pkt_pack(ctx, payload_.buffer+hdr_size, payload_.size-hdr_size, s_idx, c_idx, public, shsec, nonce, seq, payload->buffer, pld_size);
        if (rv < 0) return rv;

        return ecp_pack(parent, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, &payload_, rv+hdr_size, NULL, addr);
    } else {
        return ecp_pkt_pack(ctx, packet->buffer, packet->size, s_idx, c_idx, public, shsec, nonce, seq, payload->buffer, pld_size);
    }
}
*/

int ecp_vconn_ctx_init(ECPContext *ctx) {
    int rv;

    rv = ecp_conn_handler_init(&handler_vc);
    if (rv) return rv;

#ifdef ECP_WITH_HTABLE
    handler_vc.conn_create = vconn_create;
    handler_vc.conn_destroy = vconn_destroy;
#endif  /* ECP_WITH_HTABLE */
    handler_vc.conn_open = vconn_open;
    handler_vc.msg[ECP_MTYPE_OPEN] = vconn_handle_open;
    handler_vc.msg[ECP_MTYPE_EXEC] = ecp_conn_handle_exec;
#ifdef ECP_WITH_HTABLE
    handler_vc.msg[ECP_MTYPE_RELAY] = vconn_handle_relay;
#endif  /* ECP_WITH_HTABLE */
    ctx->handler[ECP_CTYPE_VCONN] = &handler_vc;

    rv = ecp_conn_handler_init(&handler_vl);
    if (rv) return rv;

#ifdef ECP_WITH_HTABLE
    handler_vl.conn_create = vlink_create;
    handler_vl.conn_destroy = vlink_destroy;
#endif  /* ECP_WITH_HTABLE */
    handler_vl.conn_open = vlink_open;
    handler_vl.conn_close = vlink_close;
    handler_vl.msg[ECP_MTYPE_OPEN] = vlink_handle_open;
    handler_vl.msg[ECP_MTYPE_EXEC] = ecp_conn_handle_exec;
#ifdef ECP_WITH_HTABLE
    handler_vl.msg[ECP_MTYPE_RELAY] = vlink_handle_relay;
#endif  /* ECP_WITH_HTABLE */
    ctx->handler[ECP_CTYPE_VLINK] = &handler_vl;

#ifdef ECP_WITH_HTABLE
    key_perma_table = ecp_ht_create(ctx);
    if (key_perma_table == NULL) {
        return ECP_ERR;
    }
    key_next_table = ecp_ht_create(ctx);
    if (key_next_table == NULL) {
        ecp_ht_destroy(key_perma_table);
        return ECP_ERR;
    }
#ifdef ECP_WITH_PTHREAD
    rv = pthread_mutex_init(&key_perma_mutex, NULL);
    if (rv) {
        ecp_ht_destroy(key_next_table);
        ecp_ht_destroy(key_perma_table);
        return ECP_ERR;
    }
    rv = pthread_mutex_init(&key_next_mutex, NULL);
    if (rv) {
        pthread_mutex_destroy(&key_perma_mutex);
        ecp_ht_destroy(key_next_table);
        ecp_ht_destroy(key_perma_table);
        return ECP_ERR;
    }
#endif  /* ECP_WITH_PTHREAD */
#endif  /* ECP_WITH_HTABLE */

    return ECP_OK;
}

int ecp_vconn_init(ECPConnection *conn, ECPNode *conn_node, ECPVConnection vconn[], ECPNode vconn_node[], int size) {
    ECPSocket *sock = conn->sock;
    int i, rv;

    rv = ecp_conn_init(conn, conn_node);
    if (rv) return rv;

    conn->parent = (ECPConnection *)&vconn[size-1];
    conn->pcount = size;
    for (i=0; i<size; i++) {
        rv = ecp_conn_create((ECPConnection *)&vconn[i], sock, ECP_CTYPE_VCONN);
        if (rv) return rv;

        rv = ecp_conn_init((ECPConnection *)&vconn[i], &vconn_node[i]);
        if (rv) return rv;

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

static ssize_t _vconn_send_kget(ECPConnection *conn, ECPTimerItem *ti) {
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

int ecp_vconn_open(ECPConnection *conn, ECPNode *conn_node, ECPVConnection vconn[], ECPNode vconn_node[], int size) {
    int rv;
    ssize_t _rv;

    rv = ecp_vconn_init(conn, conn_node, vconn, vconn_node, size);
    if (rv) return rv;

    _rv = ecp_timer_send((ECPConnection *)&vconn[0], _vconn_send_kget, ECP_MTYPE_KGET_REP, 3, 500);
    if (_rv < 0) return _rv;

    return ECP_OK;
}