#include "core.h"
#include "cr.h"

#include "dir.h"
#include "dir_srv.h"

#ifdef ECP_WITH_DIRSRV

int ecp_dir_init(ECPContext *ctx, ECPDirList *dir_online, ECPDirList *dir_shadow) {
    ctx->dir_online = dir_online;
    ctx->dir_shadow = dir_shadow;

    return ECP_OK;
}

ssize_t ecp_dir_handle_update(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    ECPContext *ctx = conn->sock->ctx;
    ECPDirList *dir_shadow = ctx->dir_shadow;

    if (mtype == ECP_MTYPE_DIR_REQ) {
        return ecp_dir_parse(dir_shadow, msg, size);
    } else {
        return ECP_ERR;
    }
}

ssize_t ecp_dir_handle_req(ECPSocket *sock, ECPNetAddr *addr, ECPConnection *parent, unsigned char *msg, size_t msg_size, ECPPktMeta *pkt_meta, ECP2Buffer *bufs, ECPConnection **_conn) {
    ECPContext *ctx = sock->ctx;
    ECPBuffer *packet = bufs->packet;
    ECPBuffer *payload = bufs->payload;
    ECPDirList *dir_online = ctx->dir_online;
    ssize_t rv;
    int _rv;

    if (msg_size < 0) return msg_size;

    ecp_pld_set_type(payload->buffer, payload->size, ECP_MTYPE_DIR_REP);
    msg = ecp_pld_get_buf(payload->buffer, payload->size);
    msg_size = payload->size - (msg - payload->buffer);

    _rv = ecp_dir_serialize(dir_online, msg, msg_size);
    if (_rv) return _rv;

    rv = ecp_pld_send_tr(sock, addr, parent, packet, pkt_meta, payload, ECP_SIZE_PLD(dir_online->count * ECP_SIZE_DIR_ITEM, ECP_MTYPE_DIR_REP), 0);
    if (rv < 0) return rv;

    return msg_size;
}

#endif  /* ECP_WITH_DIRSRV */