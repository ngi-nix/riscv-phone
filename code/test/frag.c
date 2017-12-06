#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "core.h"

ECPContext ctx_s;
ECPSocket sock_s;
ECPDHKey key_perma_s;
ECPConnHandler handler_s;

ECPContext ctx_c;
ECPSocket sock_c;
ECPDHKey key_perma_c;
ECPConnHandler handler_c;

ECPNode node;
ECPConnection conn;

ECPRBRecv rbuf_recv;
ECPRBMessage rbuf_r_msg[128];
ECPFragIter frag_iter;
unsigned char frag_buffer[8192];

#define CTYPE_TEST  0
#define MTYPE_MSG   8

ssize_t handle_open_c(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *p, ssize_t s, ECP2Buffer *b) {
    uint32_t seq = 0;
    
    ecp_conn_handle_open(conn, sq, t, p, s, b);
    if (s < 0) {
        printf("OPEN ERR:%ld\n", s);
        return s;
    }
    
    unsigned char content[1000];
    char *msg = "PERA JE CAR!";

    strcpy((char *)content, msg);
    ssize_t _rv = ecp_send(conn, MTYPE_MSG, content, sizeof(content));
    return s;
}

ssize_t handle_msg_c(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *p, ssize_t s, ECP2Buffer *b) {
    printf("MSG C:%s size:%ld\n", p, s);
    
    ECPRBuffer *rbuf = &conn->rbuf.recv->rbuf;
    printf("RBUF: %d %d %d %d\n", rbuf->seq_start, rbuf->seq_max, rbuf->msg_start, rbuf->msg_size);
    return s;
}

ssize_t handle_msg_s(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *p, ssize_t s, ECP2Buffer *b) {
    printf("MSG S:%s size:%ld\n", p, s);

    unsigned char content[5000];
    char *msg = "VAISTINU JE CAR!";

    strcpy((char *)content, msg);
    ssize_t _rv = ecp_send(conn, MTYPE_MSG, content, sizeof(content));
    return s;
}

int main(int argc, char *argv[]) {
    int rv;
    
    rv = ecp_init(&ctx_s);
    printf("ecp_init RV:%d\n", rv);
    
    rv = ecp_conn_handler_init(&handler_s);
    handler_s.msg[MTYPE_MSG] = handle_msg_s;
    ctx_s.handler[CTYPE_TEST] = &handler_s;
    
    rv = ecp_dhkey_generate(&ctx_s, &key_perma_s);
    printf("ecp_dhkey_generate RV:%d\n", rv);
    
    rv = ecp_sock_create(&sock_s, &ctx_s, &key_perma_s);
    printf("ecp_sock_create RV:%d\n", rv);

    rv = ecp_sock_open(&sock_s, "0.0.0.0:3000");
    printf("ecp_sock_open RV:%d\n", rv);
    
    rv = ecp_start_receiver(&sock_s);
    printf("ecp_start_receiver RV:%d\n", rv);

    rv = ecp_init(&ctx_c);
    printf("ecp_init RV:%d\n", rv);

    rv = ecp_conn_handler_init(&handler_c);
    handler_c.msg[ECP_MTYPE_OPEN] = handle_open_c;
    handler_c.msg[MTYPE_MSG] = handle_msg_c;
    ctx_c.handler[CTYPE_TEST] = &handler_c;
    
    rv = ecp_dhkey_generate(&ctx_c, &key_perma_c);
    printf("ecp_dhkey_generate RV:%d\n", rv);
    
    rv = ecp_sock_create(&sock_c, &ctx_c, &key_perma_c);
    printf("ecp_sock_create RV:%d\n", rv);

    rv = ecp_sock_open(&sock_c, NULL);
    printf("ecp_sock_open RV:%d\n", rv);
    
    rv = ecp_start_receiver(&sock_c);
    printf("ecp_start_receiver RV:%d\n", rv);

    rv = ecp_node_init(&ctx_c, &node, &key_perma_s.public, "127.0.0.1:3000");
    printf("ecp_node_init RV:%d\n", rv);

    rv = ecp_conn_create(&conn, &sock_c, CTYPE_TEST);
    printf("ecp_conn_create RV:%d\n", rv);

    rv = ecp_rbuf_create(&conn, NULL, NULL, 0, &rbuf_recv, rbuf_r_msg, 128);
    printf("ecp_rbuf_create RV:%d\n", rv);
    
    ecp_frag_iter_init(&frag_iter, frag_buffer, 8192);
    rbuf_recv.frag_iter = &frag_iter;
    
    rv = ecp_conn_open(&conn, &node);
    printf("ecp_conn_open RV:%d\n", rv);
    
    while (1) sleep(1);
}