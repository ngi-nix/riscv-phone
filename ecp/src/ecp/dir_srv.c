#include <stdlib.h>

#include "core.h"
#include "cr.h"

#include "dir.h"
#include "dir_srv.h"

int ecp_dir_init(ECPContext *ctx, ECPDirList *dir_online, ECPDirList *dir_shadow) {
    ctx->dir_online = dir_online;
    ctx->dir_shadow = dir_shadow;

    return ECP_OK;
}

ssize_t ecp_dir_send_list(ECPConnection *conn, unsigned char mtype, ECPDirList *list) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_MAX_PKT];
    unsigned char pld_buf[ECP_MAX_PLD];
    unsigned char *msg;
    size_t hdr_size;
    size_t msg_size;
    ssize_t rv;

    packet.buffer = pkt_buf;
    packet.size = ECP_MAX_PKT;
    payload.buffer = pld_buf;
    payload.size = ECP_MAX_PLD;

    ecp_pld_set_type(payload.buffer, payload.size, mtype);
    msg = ecp_pld_get_msg(payload.buffer, payload.size);
    hdr_size = msg - payload.buffer;
    msg_size = payload.size - hdr_size;

    rv = ecp_dir_serialize(list, msg, msg_size);
    if (rv < 0) return rv;

    rv = ecp_pld_send(conn, &packet, &payload, ECP_SIZE_PLD(rv, mtype), 0);
    return rv;
}

ssize_t ecp_dir_send_upd(ECPConnection *conn) {
    ECPContext *ctx = conn->sock->ctx;
    ssize_t rv;

    rv = ecp_dir_send_list(conn, ECP_MTYPE_DIR_UPD, ctx->dir_shadow);
    return rv;
}

ssize_t ecp_dir_handle_upd(ECPConnection *conn, unsigned char *msg, size_t msg_size) {
    ECPContext *ctx = conn->sock->ctx;
    ssize_t rsize;
    ssize_t rv;

    rsize = ecp_dir_parse(ctx->dir_shadow, msg, msg_size);
    if (rsize < 0) return rsize;

    rv = ecp_dir_send_list(conn, ECP_MTYPE_DIR_REP, ctx->dir_shadow);
    if (rv < 0) return rv;

    return rsize;
}

ssize_t ecp_dir_handle_req(ECPConnection *conn, unsigned char *msg, size_t msg_size) {
    ssize_t rv;

    rv = ecp_dir_send_rep(conn);
    if (rv < 0) return rv;

    return 0;
}

ssize_t ecp_dir_send_rep(ECPConnection *conn) {
    ECPContext *ctx = conn->sock->ctx;
    ssize_t rv;

    rv = ecp_dir_send_list(conn, ECP_MTYPE_DIR_REP, ctx->dir_online);
    return rv;
}

ssize_t ecp_dir_handle_rep(ECPConnection *conn, unsigned char *msg, size_t msg_size) {
    ECPContext *ctx = conn->sock->ctx;
    ssize_t rv;

    rv = ecp_dir_parse(ctx->dir_shadow, msg, msg_size);
    return rv;
}