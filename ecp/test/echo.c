#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "core.h"
#include "util.h"

ECPContext ctx_s;
ECPSocket sock_s;
ECPDHKey key_perma_s;
ECPConnHandler handler_s;

#define CTYPE_TEST  0
#define MTYPE_MSG   8

ssize_t handle_msg_s(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *p, ssize_t s, ECP2Buffer *b) {
    unsigned char buf[ECP_MAX_PLD];

    memcpy(buf, p, s);
    ssize_t _rv = ecp_send(conn, t, buf, s);

    return s;
}

static void usage(char *arg) {
    fprintf(stderr, "Usage: %s <address> <node.priv>\n", arg);
    exit(1);
}

int main(int argc, char *argv[]) {
    int rv;
    
    if (argc != 3) usage(argv[0]);
    
    rv = ecp_init(&ctx_s);
    printf("ecp_init RV:%d\n", rv);
    
    rv = ecp_conn_handler_init(&handler_s);
    handler_s.msg[MTYPE_MSG] = handle_msg_s;
    ctx_s.handler[CTYPE_TEST] = &handler_s;
    
    rv = ecp_util_key_load(&ctx_s, &key_perma_s, argv[2]);
    printf("ecp_util_key_load RV:%d\n", rv);
    
    rv = ecp_sock_create(&sock_s, &ctx_s, &key_perma_s);
    printf("ecp_sock_create RV:%d\n", rv);

    rv = ecp_sock_open(&sock_s, argv[1]);
    printf("ecp_sock_open RV:%d\n", rv);
    
    rv = ecp_start_receiver(&sock_s);
    printf("ecp_start_receiver RV:%d\n", rv);

    while (1) sleep(1);
}