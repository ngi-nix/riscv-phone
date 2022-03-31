#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <core.h>
#include <vconn/vconn.h>

void handle_err(ECPConnection *conn, unsigned char mtype, int err) {
    printf("ERR: CTYPE:0x%x MTYPE:0x%x ERR:%d\n", conn->type, mtype, err);
}

static ECPConnection *conn_alloc(ECPSocket *sock, unsigned char type) {
    ECPConnection *conn;
    int rv;

    switch (type) {
        case ECP_CTYPE_VCONN:
            conn = malloc(sizeof(ECPVConn));
            if (conn) rv = ecp_vconn_create_inb((ECPVConn *)conn, sock);
            break;

        case ECP_CTYPE_VLINK:
            conn = malloc(sizeof(ECPConnection));
            if (conn) rv = ecp_vlink_create_inb(conn, sock);
            break;

        default:
            conn = malloc(sizeof(ECPConnection));
            if (conn) rv = ecp_conn_create_inb(conn, sock, type);
            break;
    }

    if (conn == NULL) return NULL;
    if (rv) {
        free(conn);
        return NULL;
    }

    return conn;
}

static void conn_free(ECPConnection *conn) {
    free(conn);
}

int ecp_init(ECPContext *ctx) {
    int rv;

    rv = ecp_ctx_init(ctx, handle_err, NULL, conn_alloc, conn_free);
    return rv;
}