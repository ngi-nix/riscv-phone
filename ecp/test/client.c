#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "core.h"
#include "util.h"

ECPContext ctx;
ECPSocket sock;
ECPConnHandler handler;

ECPNode node;
ECPConnection conn;

#define CTYPE_TEST  0
#define MTYPE_MSG   8

ssize_t handle_open(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *p, ssize_t s, ECP2Buffer *b) {
    uint32_t seq = 0;
    
    ecp_conn_handle_open(conn, sq, t, p, s, b);
    if (s < 0) {
        printf("OPEN ERR:%ld\n", s);
        return s;
    }
    
    char *msg = "PERA JE CAR!";
    unsigned char buf[1000];

    strcpy((char *)buf, msg);
    ssize_t _rv = ecp_send(conn, MTYPE_MSG, buf, 1000);

    return s;
}

ssize_t handle_msg(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *p, ssize_t s, ECP2Buffer *b) {
    printf("MSG C:%s size:%ld\n", p, s);
    return s;
}


static void usage(char *arg) {
    fprintf(stderr, "Usage: %s <node.pub>\n", arg);
    exit(1);
}

int main(int argc, char *argv[]) {
    int rv;
    
    if (argc != 2) usage(argv[0]);
    
    rv = ecp_init(&ctx);
    printf("ecp_init RV:%d\n", rv);

    rv = ecp_conn_handler_init(&handler);
    handler.msg[ECP_MTYPE_OPEN] = handle_open;
    handler.msg[MTYPE_MSG] = handle_msg;
    ctx.handler[CTYPE_TEST] = &handler;
    
    rv = ecp_sock_init(&sock, &ctx, NULL);
    printf("ecp_sock_init RV:%d\n", rv);

    rv = ecp_sock_open(&sock, NULL);
    printf("ecp_sock_open RV:%d\n", rv);
    
    rv = ecp_start_receiver(&sock);
    printf("ecp_start_receiver RV:%d\n", rv);

    rv = ecp_util_node_load(&ctx, &node, argv[1]);
    printf("ecp_util_node_load RV:%d\n", rv);

    rv = ecp_conn_init(&conn, &sock, CTYPE_TEST);
    printf("ecp_conn_init RV:%d\n", rv);

    rv = ecp_conn_open(&conn, &node);
    printf("ecp_conn_open RV:%d\n", rv);
    
    while (1) sleep(1);
}