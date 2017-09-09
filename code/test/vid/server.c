#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "server.h"
#include "util.h"

static ECPContext ctx_s;
static ECPSocket sock_s;
static ECPDHKey key_perma_s;
static ECPConnHandler handler_s;

static ECPConnection *conn;
static int is_open = 0;

#define CTYPE_TEST  0
#define MTYPE_MSG   8

static ssize_t handle_open(ECPConnection *c, ecp_seq_t sq, unsigned char t, unsigned char *m, ssize_t sz) {
    ssize_t rv = ecp_conn_handle_open(c, sq, t, m, sz);
    if (rv < 0) return rv;

    conn = c;
    is_open = 1;
    fprintf(stderr, "IS OPEN!\n");
    
    return rv;
}

ssize_t send_frame(unsigned char *buffer, size_t size, ecp_pts_t pts) {
    return ecp_send(conn, MTYPE_MSG, buffer, size);
}

int conn_is_open(void) {
    return is_open;
}

int init_server(char *address, char *key) {
    int rv;
    
    rv = ecp_init(&ctx_s);
    fprintf(stderr, "ecp_init RV:%d\n", rv);
    
    if (!rv) rv = ecp_conn_handler_init(&handler_s);
    if (!rv) {
        handler_s.msg[ECP_MTYPE_OPEN] = handle_open;
        ctx_s.handler[CTYPE_TEST] = &handler_s;
    }
    
    if (!rv) rv = ecp_util_key_load(&ctx_s, &key_perma_s, key);
    fprintf(stderr, "ecp_util_key_load RV:%d\n", rv);
    
    if (!rv) rv = ecp_sock_create(&sock_s, &ctx_s, &key_perma_s);
    fprintf(stderr, "ecp_sock_create RV:%d\n", rv);

    if (!rv) rv = ecp_sock_open(&sock_s, address);
    fprintf(stderr, "ecp_sock_open RV:%d\n", rv);
    
    if (!rv) rv = ecp_start_receiver(&sock_s);
    fprintf(stderr, "ecp_start_receiver RV:%d\n", rv);
    
    return rv;
}
