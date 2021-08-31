#include "core.h"
#include "cr.h"

#include "dir.h"

#ifdef ECP_WITH_DIRSRV

int ecp_dir_init(ECPContext *ctx, ECPDirList *dir_online, ECPDirList *dir_shadow) {
    ctx->dir_online = dir_online;
    ctx->dir_shadow = dir_shadow;

    return ECP_OK;
}

int ecp_dir_update(ECPDirList *list, ECPDirItem *item) {
    int i;

    for (i=0; i<list->count; i++) {
        if (memcmp(ecp_cr_dh_pub_get_buf(&list->item[i].node.public), ecp_cr_dh_pub_get_buf(&item->node.public), ECP_ECDH_SIZE_KEY) == 0) {
            return ECP_OK;
        }
    }

    if (list->count == ECP_MAX_DIR_ITEM) return ECP_ERR_SIZE;

    list->item[list->count] = *item;
    list->count++;

    return ECP_OK;
}

ssize_t ecp_dir_handle_update(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    ECPContext *ctx = conn->sock->ctx;
    ECPDirList *dir_shadow = ctx->dir_shadow;
    ECPDirItem item;
    size_t _size;

    _size = size;
    if (mtype == ECP_MTYPE_DIR_REQ) {
        int rv;

        while (_size >= ECP_SIZE_DIR_ITEM) {
            ecp_dir_parse_item(msg, &item);

            rv = ecp_dir_update(dir_shadow, &item);
            if (rv) return rv;

            msg += ECP_SIZE_DIR_ITEM;
            _size -= ECP_SIZE_DIR_ITEM;
        };
    }

    return size - _size;
}

int ecp_dir_handle_req(ECPSocket *sock, ECPNetAddr *addr, ECPConnection *parent, unsigned char *msg, size_t msg_size, ECPPktMeta *pkt_meta, ECP2Buffer *bufs, ECPConnection **_conn) {
    ECPContext *ctx = sock->ctx;
    ECPBuffer *packet = bufs->packet;
    ECPBuffer *payload = bufs->payload;
    ECPDirList *dir_online = ctx->dir_online;
    ssize_t rv;
    int i;

    ecp_pld_set_type(payload->buffer, payload->size, ECP_MTYPE_DIR_REP);
    msg = ecp_pld_get_buf(payload->buffer, payload->size);
    msg_size = payload->size - (msg - payload->buffer);

    for (i=0; i<dir_online->count; i++) {
        if (msg_size < ECP_SIZE_DIR_ITEM) return ECP_ERR_SIZE;

        ecp_dir_serialize_item(msg, &dir_online->item[i]);
        msg += ECP_SIZE_DIR_ITEM;
        msg_size -= ECP_SIZE_DIR_ITEM;
    }

    rv = ecp_pld_send_tr(sock, addr, parent, packet, pkt_meta, payload, ECP_SIZE_PLD(i * ECP_SIZE_DIR_ITEM, ECP_MTYPE_DIR_REP), 0);
    if (rv < 0) return rv;

    return ECP_OK;
}

#endif  /* ECP_WITH_DIRSRV */