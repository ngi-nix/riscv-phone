#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "core.h"
#include "vconn/vconn.h"
#include "util.h"

ECPContext ctx;
ECPSocket sock;
ECPDHKey key_perma;
ECPConnHandler handler;

ECPNode node;
ECPConnection conn;

#define CTYPE_TEST  0
#define MTYPE_MSG   8

ssize_t handle_open(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *p, ssize_t s, ECP2Buffer *b) {
    printf("OPEN RECEIVED\n");
    return ecp_conn_handle_open(conn, sq, t, p, s, b);
}

ssize_t handle_msg(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *p, ssize_t s, ECP2Buffer *b) {
    // printf("MSG S:%s size:%ld\n", p, s);

    char *msg = "VAISTINU JE CAR!";
    unsigned char buf[1000];

    strcpy((char *)buf, msg);
    ssize_t _rv = ecp_send(conn, MTYPE_MSG, buf, strlen(msg)+1);

    return s;
}

static void usage(char *arg) {
    fprintf(stderr, "Usage: %s <node.priv> <vcs.pub>\n", arg);
    exit(1);
}

int main(int argc, char *argv[]) {
    int rv;
    
    if (argc != 3) usage(argv[0]);
    
    rv = ecp_init(&ctx);
    printf("ecp_init RV:%d\n", rv);
    
    rv = ecp_conn_handler_init(&handler);
    handler.msg[ECP_MTYPE_OPEN] = handle_open;
    handler.msg[MTYPE_MSG] = handle_msg;
    ctx.handler[CTYPE_TEST] = &handler;
    
    rv = ecp_util_key_load(&ctx, &key_perma, argv[1]);
    printf("ecp_util_key_load RV:%d\n", rv);
    
    rv = ecp_sock_create(&sock, &ctx, &key_perma);
    printf("ecp_sock_create RV:%d\n", rv);

    rv = ecp_sock_open(&sock, NULL);
    printf("ecp_sock_open RV:%d\n", rv);
    
    rv = ecp_start_receiver(&sock);
    printf("ecp_start_receiver RV:%d\n", rv);

    rv = ecp_util_node_load(&ctx, &node, argv[2]);
    printf("ecp_util_node_load RV:%d\n", rv);

    rv = ecp_conn_create(&conn, &sock, ECP_CTYPE_VLINK);
    printf("ecp_conn_create RV:%d\n", rv);

    rv = ecp_conn_open(&conn, &node);
    printf("ecp_conn_open RV:%d\n", rv);

    while (1) sleep(1);
}