#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <core.h>

ECPContext ctx_s;
ECPSocket sock_s;
ECPConnHandler handler_s;

ECPContext ctx_c;
ECPSocket sock_c;
ECPConnHandler handler_c;

ECPConnection conn;

#define CTYPE_TEST  0
#define MTYPE_MSG   0

static int handle_open_c(ECPConnection *conn, ECP2Buffer *b) {
    char *_msg = "PERA JE CAR!";
    ssize_t rv;

    printf("OPEN\n");
    rv = ecp_msg_send(conn, MTYPE_MSG, (unsigned char *)_msg, strlen(_msg)+1);

    return ECP_OK;
}

static ssize_t handle_msg_c(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *b) {
    printf("MSG C:%s size:%ld\n", msg, msg_size);
    return msg_size;
}

static ssize_t handle_msg_s(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *b) {
    char *_msg = "VAISTINU JE CAR!";
    ssize_t rv;

    printf("MSG S:%s size:%ld\n", msg, msg_size);
    rv = ecp_msg_send(conn, MTYPE_MSG, (unsigned char *)_msg, strlen(_msg)+1);

    return msg_size;
}

int main(int argc, char *argv[]) {
    ECPDHKey key_perma_c;
    ECPDHKey key_perma_s;
    ECPNode node;
    int rv;

    /* server */
    rv = ecp_init(&ctx_s);
    printf("ecp_init RV:%d\n", rv);

    ecp_conn_handler_init(&handler_s, handle_msg_s, NULL, NULL, NULL);
    ecp_ctx_set_handler(&ctx_s, &handler_s, CTYPE_TEST);

    rv = ecp_dhkey_gen(&key_perma_s);
    printf("ecp_dhkey_gen RV:%d\n", rv);

    rv = ecp_sock_create(&sock_s, &ctx_s, &key_perma_s);
    printf("ecp_sock_create RV:%d\n", rv);

    rv = ecp_sock_open(&sock_s, "0.0.0.0:3000");
    printf("ecp_sock_open RV:%d\n", rv);

    rv = ecp_start_receiver(&sock_s);
    printf("ecp_start_receiver RV:%d\n", rv);

    /* client */
    rv = ecp_init(&ctx_c);
    printf("ecp_init RV:%d\n", rv);

    ecp_conn_handler_init(&handler_c, handle_msg_c, handle_open_c, NULL, NULL);
    ecp_ctx_set_handler(&ctx_c, &handler_c, CTYPE_TEST);

    rv = ecp_dhkey_gen(&key_perma_c);
    printf("ecp_dhkey_gen RV:%d\n", rv);

    rv = ecp_sock_create(&sock_c, &ctx_c, &key_perma_c);
    printf("ecp_sock_create RV:%d\n", rv);

    rv = ecp_sock_open(&sock_c, NULL);
    printf("ecp_sock_open RV:%d\n", rv);

    rv = ecp_start_receiver(&sock_c);
    printf("ecp_start_receiver RV:%d\n", rv);

    rv = ecp_node_init(&node, &key_perma_s.public, "127.0.0.1:3000");
    printf("ecp_node_init RV:%d\n", rv);

    rv = ecp_conn_create(&conn, &sock_c, CTYPE_TEST);
    printf("ecp_conn_create RV:%d\n", rv);

    rv = ecp_conn_open(&conn, &node);
    printf("ecp_conn_open RV:%d\n", rv);

    while (1) sleep(1);
}
