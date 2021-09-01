#include "core.h"
#include "cr.h"

#include "dir.h"

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

int ecp_dir_handle_req(ECPSocket *sock, ECPNetAddr *addr, ECPConnection *parent, unsigned char *msg, size_t msg_size, ECPPktMeta *pkt_meta, ECP2Buffer *bufs, ECPConnection **_conn) {
    ECPContext *ctx = sock->ctx;
    ECPBuffer *packet = bufs->packet;
    ECPBuffer *payload = bufs->payload;
    ECPDirList *dir_online = ctx->dir_online;
    ssize_t _rv;
    int rv;

    ecp_pld_set_type(payload->buffer, payload->size, ECP_MTYPE_DIR_REP);
    msg = ecp_pld_get_buf(payload->buffer, payload->size);
    msg_size = payload->size - (msg - payload->buffer);

    rv = ecp_dir_serialize(dir_online, msg, msg_size);
    if (rv) return rv;

    _rv = ecp_pld_send_tr(sock, addr, parent, packet, pkt_meta, payload, ECP_SIZE_PLD(dir_online->count * ECP_SIZE_DIR_ITEM, ECP_MTYPE_DIR_REP), 0);
    if (_rv < 0) return _rv;

    return ECP_OK;
}

#endif  /* ECP_WITH_DIRSRV */