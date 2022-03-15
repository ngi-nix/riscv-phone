#include <stdlib.h>

#include <core.h>
#include <ext.h>

#include "rbuf.h"
#include "frag.h"

int ecp_ext_err_handle(ECPConnection *conn, unsigned char mtype, int err) {
    ECPRBConn *_conn = ecp_rbuf_get_rbconn(conn);

    if (_conn) return ecp_rbuf_err_handle(_conn, mtype, err);
    return ECP_PASS;
}

int ecp_ext_conn_open(ECPConnection *conn) {
    ECPRBConn *_conn = ecp_rbuf_get_rbconn(conn);

    if (_conn) ecp_rbuf_start(_conn);
    return ECP_OK;
}

void ecp_ext_conn_destroy(ECPConnection *conn) {
    ECPRBConn *_conn = ecp_rbuf_get_rbconn(conn);
    if (_conn) ecp_rbuf_destroy(_conn);
}

ssize_t ecp_ext_msg_handle(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *bufs) {
    ECPRBConn *_conn = ecp_rbuf_get_rbconn(conn);

    if (_conn) return ecp_rbuf_msg_handle(_conn, seq, mtype, msg, msg_size, bufs);
    return 0;
}

ssize_t ecp_ext_pld_store(ECPConnection *conn, ecp_seq_t seq, unsigned char *payload, size_t pld_size, ECP2Buffer *bufs) {
    ECPRBConn *_conn = ecp_rbuf_get_rbconn(conn);

    if (_conn && _conn->recv) return ecp_rbuf_store(_conn, seq, payload, pld_size);
    return 0;
}

ssize_t ecp_ext_pld_handle(ECPConnection *conn, ecp_seq_t seq, unsigned char *payload, size_t pld_size, ECP2Buffer *bufs) {
    unsigned char mtype;
    int rv;

    rv = ecp_pld_get_type(payload, pld_size, &mtype);
    if (rv) return rv;

    if (mtype & ECP_MTYPE_FLAG_FRAG) {
        return ecp_pld_defrag(conn, seq, mtype, payload, pld_size);
    } else {
        return 0;
    }
}

ssize_t ecp_ext_pld_send(ECPConnection *conn, ECPBuffer *payload, size_t pld_size, ECPBuffer *packet, size_t pkt_size, unsigned char flags, ECPTimerItem *ti, ecp_tr_addr_t *addr) {
    ECPRBConn *_conn = ecp_rbuf_get_rbconn(conn);

    if (_conn && _conn->send) return ecp_rbuf_pld_send(_conn, payload, pld_size, packet, pkt_size, ti);
    return 0;
}

ssize_t ecp_ext_msg_send(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, ECPBuffer *packet, ECPBuffer *payload) {
    if (ECP_SIZE_PKT_BUF(msg_size, mtype, conn) > ECP_MAX_PKT) {
        return ecp_msg_frag(conn, mtype, msg, msg_size, packet, payload);
    } else {
        return 0;
    }
}
