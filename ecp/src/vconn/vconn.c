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

static ECPConnHandler handler_vc;
static ECPConnHandler handler_vl;

#ifdef ECP_WITH_HTABLE

static int key_is_null(unsigned char *key) {
    int i;

    for (i=0; i<ECP_ECDH_SIZE_KEY; i++) {
        if (key[i] != 0) return 0;
    }
    return 1;
}

static void vconn_remove(ECPConnection *conn) {
    ECPVConnIn *conn_v = (ECPVConnIn *)conn;
    int i;

    if (conn->type != ECP_CTYPE_VCONN) return;
    if (ecp_conn_is_outb(conn)) return;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&key_next_mutex);
    pthread_mutex_lock(&conn->mutex);
#endif
    for (i=0; i<ECP_MAX_NODE_KEY; i++) {
        if (!key_is_null(conn_v->key_next[i])) ecp_ht_remove(key_next_table, conn_v->key_next[i]);
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

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_KGET_REQ);
    return _ecp_pld_send(conn, &packet, ECP_ECDH_IDX_PERMA, ECP_ECDH_IDX_INV, &payload, ECP_SIZE_PLD(0, ECP_MTYPE_KGET_REQ), 0, ti);
}

static ssize_t vconn_send_open(ECPConnection *conn) {
    ECPTimerItem ti;
    ECPVConnOut *conn_v = (ECPVConnOut *)conn;
    ECPConnection *conn_next;
    ssize_t rv;

    if (conn->type != ECP_CTYPE_VCONN) return ECP_ERR;
    if (ecp_conn_is_inb(conn)) return ECP_ERR;

    conn_next = conn_v->next;
    if (conn_next == NULL) return ECP_ERR;

    rv = ecp_timer_send(conn_next, _vconn_send_open, ECP_MTYPE_KGET_REP, 3, 1000);
    if (rv < 0) return rv;

    return rv;
}

static ssize_t vconn_handle_kget(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    return _ecp_conn_handle_kget(conn, seq, mtype, msg, size, b, vconn_send_open);
}

static ssize_t vconn_handle_open(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    ssize_t rv;
    int _rv;

    if (conn->type != ECP_CTYPE_VCONN) return ECP_ERR;

    if (mtype & ECP_MTYPE_FLAG_REP) {
        if (ecp_conn_is_inb(conn)) return ECP_ERR;
        if (size < 0) {
            ecp_conn_msg_handler_t handler;
            while (conn && (conn->type == ECP_CTYPE_VCONN)) {
                ECPVConnOut *conn_v = (ECPVConnOut *)conn;
                conn = conn_v->next;
            }
            if (conn) handler = ecp_conn_get_msg_handler(conn, ECP_MTYPE_OPEN);
            return handler ? handler(conn, seq, mtype, msg, size, b) : size;
        }

        rv = ecp_conn_handle_open(conn, seq, mtype, msg, size, b);
    } else {

#ifdef ECP_WITH_HTABLE

        ECPVConnIn *conn_v = (ECPVConnIn *)conn;
        unsigned char ctype;
        int is_new, do_ins;

        if (ecp_conn_is_outb(conn)) return ECP_ERR;
        if (size < 0) return size;
        if (size < 1+2*ECP_ECDH_SIZE_KEY) return ECP_ERR;

        ctype = msg[0];
        msg++;

        is_new = ecp_conn_is_new(conn);
        do_ins = 0;
        if (is_new) {
            conn_v->key_next_curr = 0;
            memset(conn_v->key_next, 0, sizeof(conn_v->key_next));
            memset(conn_v->key_out, 0, sizeof(conn_v->key_out));
            memcpy(conn_v->key_next[conn_v->key_next_curr], msg, ECP_ECDH_SIZE_KEY);
            memcpy(conn_v->key_out, msg+ECP_ECDH_SIZE_KEY, ECP_ECDH_SIZE_KEY);
            do_ins = 1;

            _rv = ecp_conn_insert(conn);
            if (_rv) return rv;
        }

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_lock(&key_next_mutex);
        pthread_mutex_lock(&conn->mutex);
#endif

        _rv = ECP_OK;
        if (!is_new && memcmp(conn_v->key_next[conn_v->key_next_curr], msg, ECP_ECDH_SIZE_KEY)) {
            conn_v->key_next_curr = (conn_v->key_next_curr + 1) % ECP_MAX_NODE_KEY;
            if (!key_is_null(conn_v->key_next[conn_v->key_next_curr])) ecp_ht_remove(key_next_table, conn_v->key_next[conn_v->key_next_curr]);
            memcpy(conn_v->key_next[conn_v->key_next_curr], msg, ECP_ECDH_SIZE_KEY);
            do_ins = 1;
        }
        if (do_ins) _rv = ecp_ht_insert(key_next_table, conn_v->key_next[conn_v->key_next_curr], conn);
        if (!_rv && !ecp_conn_is_open(conn)) ecp_conn_set_open(conn);

#ifdef ECP_WITH_PTHREAD
        pthread_mutex_unlock(&conn->mutex);
        pthread_mutex_unlock(&key_next_mutex);
#endif

        if (_rv) {
            ecp_conn_close(conn);
            return _rv;
        }

        rv = 1+2*ECP_ECDH_SIZE_KEY;

#else   /* ECP_WITH_HTABLE */

        ecp_conn_close(conn);
        rv = ECP_ERR_NOT_IMPLEMENTED;

#endif  /* ECP_WITH_HTABLE */

    }

    return rv;
}

#ifdef ECP_WITH_HTABLE
static ssize_t vconn_handle_relay(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    ECPBuffer payload;
    ECPConnection *conn_out = NULL;
    ECPVConnIn *conn_v = (ECPVConnIn *)conn;
    ssize_t rv;

    if (conn->type != ECP_CTYPE_VCONN) return ECP_ERR;
    if (ecp_conn_is_outb(conn)) return ECP_ERR;
    if (b == NULL) return ECP_ERR;

    if (size < 0) return size;
    if (size < ECP_MIN_PKT) return ECP_ERR;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&key_perma_mutex);
#endif
    conn_out = ecp_ht_search(key_perma_table, conn_v->key_out);
    if (conn_out) {
        ecp_conn_refcount_inc(conn_out);
    }
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&key_perma_mutex);
#endif

    if (conn_out == NULL) return ECP_ERR;

    payload.buffer = msg - ECP_SIZE_PLD_HDR - 1;
    payload.size = b->payload->size - (payload.buffer - b->payload->buffer);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_EXEC);
    rv = ecp_pld_send(conn_out, b->packet, &payload, ECP_SIZE_PLD_HDR+1+size, ECP_SEND_FLAG_REPLY);

    ecp_conn_refcount_dec(conn_out);

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

#endif  /* ECP_WITH_HTABLE */

static ssize_t _vlink_send_open(ECPConnection *conn, ECPTimerItem *ti) {
    ECPSocket *sock = conn->sock;
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_OPEN_REQ, conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_OPEN_REQ, conn)];
    unsigned char *buf;

    packet.buffer = pkt_buf;
    packet.size = ECP_SIZE_PKT_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_OPEN_REQ, conn);
    payload.buffer = pld_buf;
    payload.size = ECP_SIZE_PLD_BUF(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_OPEN_REQ, conn);

    // XXX server should verify perma_key
    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_OPEN_REQ);
    buf = ecp_pld_get_buf(payload.buffer, payload.size);
    buf[0] = conn->type;
    memcpy(buf+1, ecp_cr_dh_pub_get_buf(&sock->key_perma.public), ECP_ECDH_SIZE_KEY);

    return ecp_pld_send_wtimer(conn, &packet, &payload, ECP_SIZE_PLD(ECP_ECDH_SIZE_KEY+1, ECP_MTYPE_OPEN_REQ), 0, ti);
}

static ssize_t vlink_send_open(ECPConnection *conn) {
    return ecp_timer_send(conn, _vlink_send_open, ECP_MTYPE_OPEN_REP, 3, 500);
}

static ssize_t vlink_handle_kget(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    return _ecp_conn_handle_kget(conn, seq, mtype, msg, size, b, vlink_send_open);
}

static ssize_t vlink_handle_open(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    ssize_t rv;
    int _rv;
    int is_open;

    if (conn->type != ECP_CTYPE_VLINK) return ECP_ERR;

    if (size < 0) return size;

    if (ecp_conn_is_new(conn) && (size >= 1+ECP_ECDH_SIZE_KEY)) {
        // XXX we should verify perma_key
        ecp_cr_dh_pub_from_buf(&conn->node.public, msg+1);
    }

    rv = _ecp_conn_handle_open(conn, seq, mtype, msg, size, b, &is_open);
    if (rv < 0) return rv;

    if (mtype & ECP_MTYPE_FLAG_REP) {

#ifdef ECP_WITH_HTABLE

        if (!is_open) {
            _rv = vlink_insert(conn);
            if (_rv) return _rv;
        }

#endif  /* ECP_WITH_HTABLE */

    } else {
        if (size < rv+ECP_ECDH_SIZE_KEY) return ECP_ERR;

#ifdef ECP_WITH_HTABLE

        msg += rv;

        if (!is_open) {
            _rv = vlink_insert(conn);
            if (_rv) {
                ecp_conn_close(conn);
                return _rv;
            }
        }

        rv = rv+ECP_ECDH_SIZE_KEY;

#else   /* ECP_WITH_HTABLE */

        ecp_conn_close(conn);
        rv = ECP_ERR_NOT_IMPLEMENTED;

#endif  /* ECP_WITH_HTABLE */

    }

    return rv;
}

#ifdef ECP_WITH_HTABLE
static ssize_t vlink_handle_relay(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    ECPBuffer payload;
    ssize_t rv;

    if (conn->type != ECP_CTYPE_VLINK) return ECP_ERR;
    if (b == NULL) return ECP_ERR;

    if (size < 0) return size;
    if (size < ECP_MIN_PKT) return ECP_ERR;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&key_next_mutex);
#endif
    conn = ecp_ht_search(key_next_table, msg+ECP_SIZE_PROTO+1);
    if (conn) {
        ecp_conn_refcount_inc(conn);
    }
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&key_next_mutex);
#endif

    if (conn == NULL) return ECP_ERR;

    payload.buffer = msg - ECP_SIZE_PLD_HDR - 1;
    payload.size = b->payload->size - (payload.buffer - b->payload->buffer);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_EXEC);
    rv = ecp_pld_send(conn, b->packet, &payload, ECP_SIZE_PLD_HDR+1+size, ECP_SEND_FLAG_REPLY);

    ecp_conn_refcount_dec(conn);

    if (rv < 0) return rv;
    return size;
}
#endif  /* ECP_WITH_HTABLE */

#ifdef ECP_MEM_TINY
/* Memory limited version */

static ssize_t vconn_handle_exec(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    if (size < 0) return size;
    if (b == NULL) return ECP_ERR;
    if (b->packet->buffer == NULL) return ECP_ERR;

    memcpy(b->packet->buffer, msg, size);
    return ecp_pkt_handle(conn->sock, NULL, conn, b, size);
}

#else

static ssize_t vconn_handle_exec(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
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

static ssize_t vconn_set_msg(ECPConnection *conn, ECPBuffer *payload, unsigned char mtype) {
    if (ecp_conn_is_outb(conn) && (conn->type == ECP_CTYPE_VCONN) && ((mtype == ECP_MTYPE_OPEN_REQ) || (mtype == ECP_MTYPE_KGET_REQ))) {
        ECPVConnOut *conn_v = (ECPVConnOut *)conn;
        ECPConnection *conn_next = conn_v->next;
        unsigned char *buf = NULL;
        int rv;

        if (payload->size < ECP_SIZE_PLD_HDR + 3 + 2 * ECP_ECDH_SIZE_KEY) return ECP_ERR;
        if (conn_next == NULL) return ECP_ERR;

        ecp_pld_set_type(payload->buffer, payload->size, ECP_MTYPE_OPEN_REQ);
        buf = ecp_pld_get_buf(payload->buffer, payload->size);

        buf[0] = ECP_CTYPE_VCONN;
        rv = ecp_conn_dhkey_get_curr(conn_next, NULL, buf+1);
        if (rv) return rv;

        memcpy(buf+1+ECP_ECDH_SIZE_KEY, ecp_cr_dh_pub_get_buf(&conn_next->node.public), ECP_ECDH_SIZE_KEY);
        buf[1+2*ECP_ECDH_SIZE_KEY] = ECP_MTYPE_RELAY;

        return ECP_SIZE_PLD_HDR + 3 + 2 * ECP_ECDH_SIZE_KEY;
    } else {
        if (payload->size < ECP_SIZE_PLD_HDR + 1) return ECP_ERR;

        ecp_pld_set_type(payload->buffer, payload->size, ECP_MTYPE_RELAY);

        return ECP_SIZE_PLD_HDR + 1;
    }

}

#ifdef ECP_MEM_TINY
/* Memory limited version */

ssize_t ecp_pack(ECPContext *ctx, ECPConnection *parent, ECPBuffer *packet, ECPPktMeta *pkt_meta, ECPBuffer *payload, size_t pld_size, ECPNetAddr *addr) {
    if ((packet == NULL) || (packet->buffer == NULL)) return ECP_ERR;
    if ((payload == NULL) || (payload->buffer == NULL)) return ECP_ERR;

    if (parent) {
        unsigned char mtype;
        ssize_t rv, hdr_size;
        int _rv;

        _rv = ecp_pld_get_type(payload->buffer, payload->size, &mtype);
        if (_rv) return _rv;

        hdr_size = vconn_set_msg(parent, packet, mtype);
        if (hdr_size < 0) return hdr_size;

        rv = _ecp_pack(ctx, packet->buffer+hdr_size, packet->size-hdr_size, pkt_meta, payload->buffer, pld_size);
        if (rv < 0) return rv;

        if (payload->size < rv+hdr_size) return ECP_ERR;
        memcpy(payload->buffer, packet->buffer, rv+hdr_size);

        return ecp_pack_conn(parent, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, payload, rv+hdr_size, addr, NULL);
    } else {
        return _ecp_pack(ctx, packet->buffer, packet->size, pkt_meta, payload->buffer, pld_size);
    }
}

#else

ssize_t ecp_pack(ECPContext *ctx, ECPConnection *parent, ECPBuffer *packet, ECPPktMeta *pkt_meta, ECPBuffer *payload, size_t pld_size, ECPNetAddr *addr) {
    if ((packet == NULL) || (packet->buffer == NULL)) return ECP_ERR;
    if ((payload == NULL) || (payload->buffer == NULL)) return ECP_ERR;

    if (parent) {
        ECPBuffer _payload;
        unsigned char pld_buf[ECP_MAX_PLD];
        unsigned char mtype;
        ssize_t rv, hdr_size;
        int _rv;

        _payload.buffer = pld_buf;
        _payload.size = ECP_MAX_PLD;

        _rv = ecp_pld_get_type(payload->buffer, payload->size, &mtype);
        if (_rv) return _rv;

        hdr_size = vconn_set_msg(parent, &_payload, mtype);
        if (hdr_size < 0) return hdr_size;

        rv = _ecp_pack(ctx, _payload.buffer+hdr_size, _payload.size-hdr_size, pkt_meta, payload->buffer, pld_size);
        if (rv < 0) return rv;

        return ecp_pack_conn(parent, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, &_payload, rv+hdr_size, addr, NULL);
    } else {
        return _ecp_pack(ctx, packet->buffer, packet->size, pkt_meta, payload->buffer, pld_size);
    }
}

#endif

#ifdef ECP_MEM_TINY
/* Memory limited version */

ssize_t ecp_pack_conn(ECPConnection *conn, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, ECPBuffer *payload, size_t pld_size, ECPNetAddr *addr, ECPSeqItem *si) {
    if ((packet == NULL) || (packet->buffer == NULL)) return ECP_ERR;
    if ((payload == NULL) || (payload->buffer == NULL)) return ECP_ERR;

    if (conn->parent) {
        unsigned char mtype;
        ssize_t rv, hdr_size;
        int _rv;

        _rv = ecp_pld_get_type(payload->buffer, payload->size, &mtype);
        if (_rv) return _rv;

        hdr_size = vconn_set_msg(conn->parent, packet, mtype);
        if (hdr_size < 0) return hdr_size;

        rv = _ecp_pack_conn(conn, packet->buffer+hdr_size, packet->size-hdr_size, s_idx, c_idx, payload->buffer, pld_size, NULL, si);
        if (rv < 0) return rv;

        if (payload->size < rv+hdr_size) return ECP_ERR;
        memcpy(payload->buffer, packet->buffer, rv+hdr_size);

        return ecp_pack_conn(conn->parent, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, payload, rv+hdr_size, addr, NULL);
    } else {
        return _ecp_pack_conn(conn, packet->buffer, packet->size, s_idx, c_idx, payload->buffer, pld_size, addr, si);
    }
}

#else

ssize_t ecp_pack_conn(ECPConnection *conn, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, ECPBuffer *payload, size_t pld_size, ECPNetAddr *addr, ECPSeqItem *si) {
    if ((packet == NULL) || (packet->buffer == NULL)) return ECP_ERR;
    if ((payload == NULL) || (payload->buffer == NULL)) return ECP_ERR;

    if (conn->parent) {
        ECPBuffer _payload;
        unsigned char pld_buf[ECP_MAX_PLD];
        unsigned char mtype;
        ssize_t rv, hdr_size;
        int _rv;

        _payload.buffer = pld_buf;
        _payload.size = ECP_MAX_PLD;

        _rv = ecp_pld_get_type(payload->buffer, payload->size, &mtype);
        if (_rv) return _rv;

        hdr_size = vconn_set_msg(conn->parent, &_payload, mtype);
        if (hdr_size < 0) return hdr_size;

        rv = _ecp_pack_conn(conn, _payload.buffer+hdr_size, _payload.size-hdr_size, s_idx, c_idx, payload->buffer, pld_size, NULL, si);
        if (rv < 0) return rv;

        return ecp_pack_conn(conn->parent, packet, ECP_ECDH_IDX_INV, ECP_ECDH_IDX_INV, &_payload, rv+hdr_size, addr, NULL);
    } else {
        return _ecp_pack_conn(conn, packet->buffer, packet->size, s_idx, c_idx, payload->buffer, pld_size, addr, si);
    }
}

#endif

int ecp_vconn_ctx_init(ECPContext *ctx) {
    int rv;

    rv = ecp_conn_handler_init(&handler_vc);
    if (rv) return rv;

    handler_vc.msg[ECP_MTYPE_OPEN] = vconn_handle_open;
    handler_vc.msg[ECP_MTYPE_KGET] = vconn_handle_kget;
    handler_vc.msg[ECP_MTYPE_EXEC] = vconn_handle_exec;
#ifdef ECP_WITH_HTABLE
    handler_vc.msg[ECP_MTYPE_RELAY] = vconn_handle_relay;
    handler_vc.conn_close = vconn_remove;
#endif  /* ECP_WITH_HTABLE */
    ctx->handler[ECP_CTYPE_VCONN] = &handler_vc;

    rv = ecp_conn_handler_init(&handler_vl);
    if (rv) return rv;

    handler_vl.msg[ECP_MTYPE_OPEN] = vlink_handle_open;
    handler_vl.msg[ECP_MTYPE_KGET] = vlink_handle_kget;
    handler_vl.msg[ECP_MTYPE_EXEC] = vconn_handle_exec;
#ifdef ECP_WITH_HTABLE
    handler_vl.msg[ECP_MTYPE_RELAY] = vlink_handle_relay;
    handler_vl.conn_close = vlink_remove;
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

static ssize_t _vconn_send_kget(ECPConnection *conn, ECPTimerItem *ti) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(0, ECP_MTYPE_KGET_REQ, conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(0, ECP_MTYPE_KGET_REQ, conn)];

    packet.buffer = pkt_buf;
    packet.size = ECP_SIZE_PKT_BUF(0, ECP_MTYPE_KGET_REQ, conn);
    payload.buffer = pld_buf;
    payload.size = ECP_SIZE_PLD_BUF(0, ECP_MTYPE_KGET_REQ, conn);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_KGET_REQ);
    return _ecp_pld_send(conn, &packet, ECP_ECDH_IDX_PERMA, ECP_ECDH_IDX_INV, &payload, ECP_SIZE_PLD(0, ECP_MTYPE_KGET_REQ), 0, ti);
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