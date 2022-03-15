#include <stdlib.h>
#include <stdio.h>

#include <core.h>

void handle_err(ECPConnection *conn, unsigned char mtype, int err) {
    printf("ERR: CTYPE:%d MTYPE:%x ERR:%d\n", conn->type, mtype, err);
}

static ECPConnection *conn_alloc(ECPSocket *sock, unsigned char type) {
    ECPConnection *conn;
    int rv;

    conn = malloc(sizeof(ECPConnection));
    if (conn == NULL) return NULL;

    rv = ecp_conn_create_inb(conn, sock, type);
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
