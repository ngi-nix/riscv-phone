#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "core.h"
#include "vconn/vconn.h"

static int v_rng(void *buf, size_t bufsize) {
    int fd;

    if((fd = open("/dev/urandom", O_RDONLY)) < 0) return -1;
    size_t nb = read(fd, buf, bufsize);
    close(fd);
    if (nb != bufsize) return -1;
    return 0;
}

static ECPConnection *conn_alloc(ECPSocket *sock, unsigned char type) {
    ECPConnection *conn;
    int rv;

    switch (type) {
        case ECP_CTYPE_VCONN:
            conn = malloc(sizeof(ECPVConnIn));
            break;
        default:
            conn = malloc(sizeof(ECPConnection));
            break;
    }
    if (conn == NULL) return NULL;

    rv = ecp_conn_init(conn, sock, type);
    if (rv) {
        printf("free1\n");
        free(conn);
        return NULL;
    }
    return conn;
}

static void conn_free(ECPConnection *conn) {
    printf("free2\n");
    free(conn);
}

int ecp_init(ECPContext *ctx) {
    int rv;

    rv = ecp_ctx_init(ctx);
    if (rv) return rv;

    rv = ecp_vconn_ctx_init(ctx);
    if (rv) return rv;

    ctx->rng = v_rng;
    ctx->conn_alloc = conn_alloc;
    ctx->conn_free = conn_free;

    return ECP_OK;
}