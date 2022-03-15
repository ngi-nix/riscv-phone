#include <stdlib.h>

#include <core.h>
#include <cr.h>

#include "dir.h"
#include "dir_srv.h"

int ecp_dir_create(ECPContext *ctx, ECPDirSrv *dir_srv, ECPDirList *dir_online, ECPDirList *dir_shadow) {
    int rv;

#ifdef ECP_WITH_PTHREAD
    rv = pthread_mutex_init(&dir_srv->online.mutex, NULL);
    if (rv) return ECP_ERR;

    rv = pthread_mutex_init(&dir_srv->shadow.mutex, NULL);
    if (rv) {
        pthread_mutex_destroy(&dir_srv->online.mutex);
        return ECP_ERR;
    }
#endif

    dir_srv->online.list = dir_online;
    dir_srv->shadow.list = dir_shadow;
    ctx->dir_srv = dir_srv;

    return ECP_OK;
}

void ecp_dir_destroy(ECPContext *ctx) {
    ECPDirSrv *dir_srv = ctx->dir_srv;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_destroy(&dir_srv->shadow.mutex);
    pthread_mutex_destroy(&dir_srv->online.mutex);
#endif

}

ssize_t ecp_dir_send_list(ECPConnection *conn, unsigned char mtype, ECPDirTable *dir_table) {
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

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&dir_table->mutex);
#endif

    rv = ecp_dir_serialize(dir_table->list, msg, msg_size);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&dir_table->mutex);
#endif

    if (rv < 0) return rv;

    rv = ecp_pld_send(conn, &packet, &payload, ECP_SIZE_PLD(rv, mtype), 0);
    return rv;
}

ssize_t ecp_dir_send_upd(ECPConnection *conn) {
    ECPDirSrv *dir_srv = conn->sock->ctx->dir_srv;
    ECPDirTable *dir_table = &dir_srv->shadow;
    ssize_t rv;

    rv = ecp_dir_send_list(conn, ECP_MTYPE_DIR_UPD, dir_table);
    return rv;
}

ssize_t ecp_dir_handle_upd(ECPConnection *conn, unsigned char *msg, size_t msg_size) {
    ECPDirSrv *dir_srv = conn->sock->ctx->dir_srv;
    ECPDirTable *dir_table = &dir_srv->shadow;
    ssize_t rsize;
    ssize_t rv;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&dir_table->mutex);
#endif

    rsize = ecp_dir_parse(dir_table->list, msg, msg_size);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&dir_table->mutex);
#endif

    if (rsize < 0) return rsize;

    rv = ecp_dir_send_list(conn, ECP_MTYPE_DIR_REP, dir_table);
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
    ECPDirSrv *dir_srv = conn->sock->ctx->dir_srv;
    ECPDirTable *dir_table = &dir_srv->online;
    ssize_t rv;

    rv = ecp_dir_send_list(conn, ECP_MTYPE_DIR_REP, dir_table);
    return rv;
}

ssize_t ecp_dir_handle_rep(ECPConnection *conn, unsigned char *msg, size_t msg_size) {
    ECPDirSrv *dir_srv = conn->sock->ctx->dir_srv;
    ECPDirTable *dir_table = &dir_srv->shadow;
    ssize_t rv;

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_lock(&dir_table->mutex);
#endif

    rv = ecp_dir_parse(dir_table->list, msg, msg_size);

#ifdef ECP_WITH_PTHREAD
    pthread_mutex_unlock(&dir_table->mutex);
#endif

    return rv;
}
