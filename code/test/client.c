#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "core.h"
#include "util.h"

ECPContext ctx_c;
ECPSocket sock_c;
ECPConnHandler handler_c;

ECPNode node;
ECPConnection conn;

#define CTYPE_TEST  0
#define MTYPE_MSG   8

ssize_t handle_open_c(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *p, ssize_t s) {
    uint32_t seq = 0;
    
    ecp_conn_handle_open(conn, sq, t, p, s);
    if (s < 0) {
        printf("OPEN ERR:%ld\n", s);
        return s;
    }
    
    unsigned char payload[ECP_SIZE_PLD(1000)];
    unsigned char *buf = ecp_pld_get_buf(payload);
    char *msg = "PERA JE CAR!";

    ecp_pld_set_type(payload, MTYPE_MSG);
    strcpy((char *)buf, msg);
    ssize_t _rv = ecp_send(conn, payload, sizeof(payload));
    return s;
}

ssize_t handle_msg_c(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *p, ssize_t s) {
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
    
    rv = ecp_init(&ctx_c);
    printf("ecp_init RV:%d\n", rv);

    rv = ecp_conn_handler_init(&handler_c);
    handler_c.msg[ECP_MTYPE_OPEN] = handle_open_c;
    handler_c.msg[MTYPE_MSG] = handle_msg_c;
    ctx_c.handler[CTYPE_TEST] = &handler_c;
    
    rv = ecp_sock_create(&sock_c, &ctx_c, NULL);
    printf("ecp_sock_create RV:%d\n", rv);

    rv = ecp_sock_open(&sock_c, NULL);
    printf("ecp_sock_open RV:%d\n", rv);
    
    rv = ecp_start_receiver(&sock_c);
    printf("ecp_start_receiver RV:%d\n", rv);

    rv = ecp_util_node_load(&ctx_c, &node, argv[1]);
    printf("ecp_util_node_load RV:%d\n", rv);

    rv = ecp_conn_create(&conn, &sock_c, CTYPE_TEST);
    printf("ecp_conn_create RV:%d\n", rv);

    rv = ecp_conn_open(&conn, &node);
    printf("ecp_conn_open RV:%d\n", rv);
    
    while (1) sleep(1);
}