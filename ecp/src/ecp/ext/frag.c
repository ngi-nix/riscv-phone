#include <stdlib.h>
#include <string.h>

#include <core.h>

#include "rbuf.h"
#include "frag.h"

int ecp_frag_iter_init(ECPRBConn *conn, ECPFragIter *iter, unsigned char *buffer, size_t buf_size) {
    memset(iter, 0, sizeof(ECPFragIter));
    iter->buffer = buffer;
    iter->buf_size = buf_size;

    conn->iter = iter;
    return ECP_OK;
}

ECPFragIter *ecp_frag_iter_get(ECPRBConn *conn) {
    return conn->iter;
}

void ecp_frag_iter_reset(ECPFragIter *iter) {
    iter->seq = 0;
    iter->frag_cnt = 0;
    iter->pld_size = 0;
}

ssize_t ecp_msg_frag(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, ECPBuffer *packet, ECPBuffer *payload) {
    unsigned char *msg_buf;
    unsigned char *pld_buf;
    size_t pld_size;
    size_t frag_size, frag_size_final;
    ecp_nonce_t nonce, nonce_start;
    int pkt_cnt = 0;
    int i;

    mtype |= ECP_MTYPE_FLAG_FRAG;
    frag_size = ECP_MAX_PKT - ECP_SIZE_PKT_BUF(0, mtype, conn);
    pkt_cnt = msg_size / frag_size;
    frag_size_final = msg_size - frag_size * pkt_cnt;
    if (frag_size_final) pkt_cnt++;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&conn->mutex);
#endif

    nonce_start = conn->nonce_out;
    conn->nonce_out += pkt_cnt;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&conn->mutex);
#endif

    pld_buf = payload->buffer;
    pld_size = payload->size;
    for (i=0; i<pkt_cnt; i++) {
        ssize_t rv;

        ecp_pld_set_type(pld_buf, pld_size, mtype);
        ecp_pld_set_frag(pld_buf, pld_size, i, pkt_cnt, frag_size);
        msg_buf = ecp_pld_get_msg(pld_buf, pld_size);

        if ((i == pkt_cnt - 1) && frag_size_final) frag_size = frag_size_final;
        memcpy(msg_buf, msg, frag_size);
        msg += frag_size;
        nonce = nonce_start + i;

        rv = ecp_pld_send_wnonce(conn, packet, payload, ECP_SIZE_PLD(frag_size, mtype), 0, &nonce);
        if (rv < 0) return rv;
    }

    return msg_size;
}

ssize_t ecp_pld_defrag(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *payload, size_t pld_size) {
    ECPRBConn *_conn = NULL;
    ECPFragIter *iter = NULL;
    unsigned char *msg;
    unsigned char frag_cnt, frag_tot;
    uint16_t frag_size;
    size_t hdr_size, msg_size;
    size_t buf_offset;
    int rv;

    _conn = ecp_rbuf_get_rbconn(conn);
    if (conn) iter = ecp_frag_iter_get(_conn);
    if (iter == NULL) ECP_ERR_ITER;

    rv = ecp_pld_get_frag(payload, pld_size, &frag_cnt, &frag_tot, &frag_size);
    if (rv) return ECP_ERR;

    msg = ecp_pld_get_msg(payload, pld_size);
    if (msg == NULL) return ECP_ERR;
    hdr_size = msg - payload;

    msg_size = pld_size - hdr_size;
    if (msg_size == 0) return ECP_ERR;

    if (iter->pld_size && (iter->seq + frag_cnt != seq)) ecp_frag_iter_reset(iter);

    if (iter->pld_size == 0) {
        iter->seq = seq - frag_cnt;
        iter->frag_cnt = 0;
    }

    mtype &= ~ECP_MTYPE_FLAG_FRAG;
    buf_offset = ECP_SIZE_MTYPE + ECP_SIZE_MT_FLAG(mtype) + frag_size * frag_cnt;
    if (buf_offset + msg_size > iter->buf_size) return ECP_ERR_SIZE;
    memcpy(iter->buffer + buf_offset, msg, msg_size);

    if (frag_cnt == 0) {
        if (ECP_SIZE_MTYPE + ECP_SIZE_MT_FLAG(mtype) > iter->buf_size) return ECP_ERR_SIZE;

        iter->buffer[0] = mtype;
        if (ECP_SIZE_MT_FLAG(mtype)) {
            memcpy(iter->buffer + ECP_SIZE_MTYPE, payload + ECP_SIZE_MTYPE, ECP_SIZE_MT_FLAG(mtype));
        }
        msg_size += ECP_SIZE_MTYPE + ECP_SIZE_MT_FLAG(mtype);
    }

    iter->frag_cnt++;
    iter->pld_size += msg_size;
    if (iter->frag_cnt == frag_tot) {
        ecp_pld_handle_one(conn, iter->seq, iter->buffer, iter->pld_size, NULL);
        ecp_frag_iter_reset(iter);
    }

    return pld_size;
}
