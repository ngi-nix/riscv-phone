#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <core.h>

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

#define PTYPE_MSG       16

ssize_t handle_open_c(ECPConnection *c, unsigned char t, unsigned char *p, ssize_t s) {
    uint32_t seq = 0;
    
    if (s < 0) {
        printf("OPEN ERR:%ld\n", s);
        return 0;
    }
    
    unsigned char payload[ECP_SIZE_PLD(1000)];
    unsigned char *buf = ecp_pld_get_buf(payload);
    char *msg = "PERA JE CAR!";

    strcpy((char *)buf, msg);
    ssize_t _rv = ecp_send(c, PTYPE_MSG, payload, 1000);
    return 0;
}

ssize_t handle_msg_c(ECPConnection *c, unsigned char t, unsigned char *p, ssize_t s) {
    printf("MSG C:%s size:%ld\n", p, s);
    
    return s;
}

ssize_t handle_msg_s(ECPConnection *c, unsigned char t, unsigned char *p, ssize_t s) {
    printf("MSG S:%s size:%ld\n", p, s);

    unsigned char payload[ECP_SIZE_PLD(1000)];
    unsigned char *buf = ecp_pld_get_buf(payload);
    char *msg = "VAISTINU JE CAR!";

    strcpy((char *)buf, msg);
    ssize_t _rv = ecp_send(c, PTYPE_MSG, payload, 1000);

    return s;
}

int conn_create(ECPConnection *c, unsigned char *p, size_t s) {
    c->handler = &handler_s;
    return ECP_OK;
}

int main(int argc, char *argv[]) {
    int rv;
    
    rv = ecp_init(&ctx_s);
    printf("ecp_init RV:%d\n", rv);
    
    rv = ecp_dhkey_generate(&ctx_s, &key_perma_s);
    printf("ecp_dhkey_generate RV:%d\n", rv);
    
    rv = ecp_sock_create(&sock_s, &ctx_s, &key_perma_s);
    printf("ecp_sock_create RV:%d\n", rv);

    rv = ecp_conn_hander_init(&handler_s);
    
    handler_s.f[PTYPE_MSG] = handle_msg_s;
    sock_s.conn_create = conn_create;
    
    rv = ecp_sock_open(&sock_s, "0.0.0.0:3000");
    printf("ecp_sock_open RV:%d\n", rv);
    
    rv = ecp_start_receiver(&sock_s);
    printf("ecp_start_receiver RV:%d\n", rv);

    rv = ecp_init(&ctx_c);
    printf("ecp_init RV:%d\n", rv);
    
    rv = ecp_dhkey_generate(&ctx_c, &key_perma_c);
    printf("ecp_dhkey_generate RV:%d\n", rv);
    
    rv = ecp_sock_create(&sock_c, &ctx_c, &key_perma_c);
    printf("ecp_sock_create RV:%d\n", rv);

    rv = ecp_sock_open(&sock_c, NULL);
    printf("ecp_sock_open RV:%d\n", rv);
    
    rv = ecp_start_receiver(&sock_c);
    printf("ecp_start_receiver RV:%d\n", rv);

    rv = ecp_node_init(&ctx_c, &node, "127.0.0.1:3000", &key_perma_s.public);
    printf("ecp_node_init RV:%d\n", rv);

    rv = ecp_conn_create(&conn, &sock_c);
    printf("ecp_conn_create RV:%d\n", rv);

    rv = ecp_conn_hander_init(&handler_c);
    printf("ecp_conn_hander_init RV:%d\n", rv);

    handler_c.f[ECP_PTYPE_OPEN] = handle_open_c;
    handler_c.f[PTYPE_MSG] = handle_msg_c;
    
    rv = ecp_conn_open(&conn, &node, &handler_c);
    printf("ecp_conn_open RV:%d\n", rv);
    
    while (1) sleep(1);
}