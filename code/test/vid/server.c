#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "server.h"

static ECPContext ctx;
static ECPSocket sock;
static ECPDHKey key_perma;
static ECPConnHandler handler;

static ECPConnection *conn_in;
static int is_open = 0;

#define CTYPE_TEST  0
#define MTYPE_MSG   8

ECPNode node;
ECPConnection conn;

static ssize_t handle_open(ECPConnection *c, ecp_seq_t sq, unsigned char t, unsigned char *m, ssize_t sz, ECP2Buffer *b) {
    fprintf(stderr, "IS OPEN!\n");

    ssize_t rv = ecp_conn_handle_open(c, sq, t, m, sz, b);
    if (rv < 0) return rv;

    conn_in = c;
    is_open = 1;
    
    return rv;
}

ssize_t send_frame(unsigned char *buffer, size_t size, ecp_pts_t pts) {
    return ecp_send(conn_in, MTYPE_MSG, buffer, size);
}

int conn_is_open(void) {
    return is_open;
}

int init_server(char *address, char *my_key, char *vcs_key) {
    int rv;
    
    rv = ecp_init(&ctx);
    fprintf(stderr, "ecp_init RV:%d\n", rv);
    
    if (!rv) rv = ecp_conn_handler_init(&handler);
    if (!rv) {
        handler.msg[ECP_MTYPE_OPEN] = handle_open;
        ctx.handler[CTYPE_TEST] = &handler;
    }
    
    if (!rv) rv = ecp_util_key_load(&ctx, &key_perma, my_key);
    fprintf(stderr, "ecp_util_key_load RV:%d\n", rv);
    
    if (!rv) rv = ecp_sock_create(&sock, &ctx, &key_perma);
    fprintf(stderr, "ecp_sock_create RV:%d\n", rv);

    if (!rv) rv = ecp_sock_open(&sock, address);
    fprintf(stderr, "ecp_sock_open RV:%d\n", rv);
    
    if (!rv) rv = ecp_start_receiver(&sock);
    fprintf(stderr, "ecp_start_receiver RV:%d\n", rv);

    if (!rv) rv = ecp_util_node_load(&ctx, &node, vcs_key);
    printf("ecp_util_node_load RV:%d\n", rv);

    if (!rv) rv = ecp_conn_create(&conn, &sock, ECP_CTYPE_VLINK);
    printf("ecp_conn_create RV:%d\n", rv);

    if (!rv) rv = ecp_conn_open(&conn, &node);
    printf("ecp_conn_open RV:%d\n", rv);

    return rv;
}
