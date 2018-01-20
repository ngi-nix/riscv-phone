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

static ECPConnection *conn_alloc(unsigned char type) {
    switch (type) {
        case ECP_CTYPE_VCONN:
            return malloc(sizeof(ECPVConnIn));
        default:
            return malloc(sizeof(ECPConnection));
    }
}

static void conn_free(ECPConnection *conn) {
    free(conn);
}

int ecp_init(ECPContext *ctx) {
    int rv;
    
    rv = ecp_ctx_create_vconn(ctx);
    if (rv) return rv;
    
    ctx->rng = v_rng;
    ctx->conn_alloc = conn_alloc;
    ctx->conn_free = conn_free;
    
    return ECP_OK;
}