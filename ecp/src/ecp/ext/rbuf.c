#include <stdlib.h>

#include <core.h>

#include "rbuf.h"

ECPRBConn *ecp_rbuf_get_rbconn(ECPConnection *conn) {
    if (ecp_conn_has_rbuf(conn)) return (ECPRBConn *)conn;
    return NULL;
}

ECPConnection *ecp_rbuf_get_conn(ECPRBConn *conn) {
    return &conn->b;
}

void _ecp_rbuf_start(ECPRBuffer *rbuf, ecp_seq_t seq) {
    rbuf->seq_max = seq;
    rbuf->seq_start = seq + 1;
}

int _ecp_rbuf_msg_idx(ECPRBuffer *rbuf, ecp_seq_t seq, unsigned short *idx) {
    ecp_seq_t seq_offset = seq - rbuf->seq_start;

    /* This also checks for seq_start <= seq if seq type range >> rbuf->arr_size */
    if (seq_offset >= rbuf->arr_size) return ECP_ERR_FULL;

    if (idx) *idx = ECP_RBUF_IDX_MASK(rbuf->idx_start + seq_offset, rbuf->arr_size);
    return ECP_OK;
}

void ecp_rbuf_conn_init(ECPRBConn *conn) {
    ECPConnection *_conn = ecp_rbuf_get_conn(conn);

    ecp_conn_set_flags(_conn, ECP_CONN_FLAG_RBUF);
    conn->send = NULL;
    conn->recv = NULL;
    conn->iter = NULL;
}

int ecp_rbuf_conn_create(ECPRBConn *conn, ECPSocket *sock, unsigned char type) {
    ECPConnection *_conn = ecp_rbuf_get_conn(conn);
    int rv;

    rv = ecp_conn_create(_conn, sock, type);
    if (rv) return rv;

    ecp_rbuf_conn_init(conn);
    return ECP_OK;
}

int ecp_rbuf_conn_create_inb(ECPRBConn *conn, ECPSocket *sock, unsigned char type) {
    ECPConnection *_conn = ecp_rbuf_get_conn(conn);
    int rv;

    rv = ecp_conn_create_inb(_conn, sock, type);
    if (rv) return rv;

    ecp_rbuf_conn_init(conn);
    return ECP_OK;
}

void ecp_rbuf_destroy(ECPRBConn *conn) {
    if (conn->send) ecp_rbsend_destroy(conn);
    if (conn->recv) ecp_rbrecv_destroy(conn);
    conn->iter = NULL;
}

void ecp_rbuf_start(ECPRBConn *conn) {
    ECPConnection *_conn = ecp_rbuf_get_conn(conn);

    if (conn->send) {
        ecp_seq_t seq_out;

        seq_out = (ecp_seq_t)(_conn->nonce_out);
        ecp_rbsend_start(conn, seq_out);
    }

    if (conn->recv) {
        ecp_seq_t seq_in;

        seq_in = (ecp_seq_t)(_conn->nonce_in);
        ecp_rbrecv_start(conn, seq_in);
    }
}

ssize_t ecp_rbuf_msg_handle(ECPRBConn *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *bufs) {
    switch (mtype) {
        case ECP_MTYPE_RBACK:
            if (conn->send) return ecp_rbuf_handle_ack(conn, msg, msg_size);
            break;

        case ECP_MTYPE_RBNOP:
            if (conn->recv) return ecp_rbuf_handle_nop(conn, msg, msg_size);
            break;

        case ECP_MTYPE_RBFLUSH:
            if (conn->recv) return ecp_rbuf_handle_flush(conn);
            break;

        default:
            break;
    }

    return ECP_ERR_MTYPE;
}

int ecp_rbuf_err_handle(ECPRBConn *conn, unsigned char mtype, int err) {
    if (conn->recv && (mtype == ECP_MTYPE_RBTIMER)) {
        ecp_rbuf_handle_timer(conn);
        return ECP_OK;
    }
    return ECP_PASS;
}
