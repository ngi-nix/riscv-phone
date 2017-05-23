#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "core.h"
#include "proxy.h"
#include "util.h"

ECPContext ctx;
ECPSocket sock;
ECPConnHandler handler;

ECPConnection conn;
ECPNode node;

ECPConnProxy conn_proxy;
ECPNode node_proxy;

#define CTYPE_TEST  0
#define MTYPE_MSG   8

ssize_t handle_open(ECPConnection *conn, unsigned char t, unsigned char *p, ssize_t s) {
    uint32_t seq = 0;
    
    ecp_conn_handle_open(conn, t, p, s);
    if (s < 0) {
        printf("OPEN ERR:%ld\n", s);
        return 0;
    }
    
    printf("OPEN!\n");

    unsigned char payload[ECP_SIZE_PLD(1000)];
    unsigned char *buf = ecp_pld_get_buf(payload);
    char *msg = "PERA JE CAR!";

    ecp_pld_set_type(payload, MTYPE_MSG);
    strcpy((char *)buf, msg);
    ssize_t _rv = ecp_send(conn, payload, sizeof(payload));
    return 0;
}

ssize_t handle_msg(ECPConnection *conn, unsigned char t, unsigned char *p, ssize_t s) {
    printf("MSG S:%s size:%ld\n", p, s);
    return s;
}

static void usage(char *arg) {
    fprintf(stderr, "Usage: %s <server.pub> <proxy.pub>\n", arg);
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
    
    rv = ecp_sock_create(&sock, &ctx, NULL);
    printf("ecp_sock_create RV:%d\n", rv);

    rv = ecp_sock_open(&sock, NULL);
    printf("ecp_sock_open RV:%d\n", rv);
    
    rv = ecp_start_receiver(&sock);
    printf("ecp_start_receiver RV:%d\n", rv);

    rv = ecp_util_node_load(&ctx, &node, argv[1]);
    printf("ecp_util_node_load RV:%d\n", rv);

    rv = ecp_util_node_load(&ctx, &node_proxy, argv[2]);
    printf("ecp_util_node_load RV:%d\n", rv);

    rv = ecp_conn_create(&conn, &sock, CTYPE_TEST);
    printf("ecp_conn_create RV:%d\n", rv);

    rv = ecp_conn_proxy_open(&conn, &node, &conn_proxy, &node_proxy, 1);
    printf("ecp_conn_proxy_open RV:%d\n", rv);

    while (1) sleep(1);
}