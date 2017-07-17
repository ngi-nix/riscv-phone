#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>

#include "core.h"

#ifdef __linux__
#define SIGINFO     SIGTSTP
#endif

#define NUM_S       32
#define NUM_C       256
#define MSG_RATE    65

#define CTYPE_TEST  0
#define MTYPE_MSG   8

ECPContext ctx_s;
ECPDHKey key_perma_s;
ECPConnHandler handler_s;
ECPSocket *sock_s;

ECPConnHandler handler_c;
ECPContext *ctx_c;
ECPSocket *sock_c;
ECPDHKey *key_perma_c;

ECPNode *node;
ECPConnection *conn;

pthread_t *s_thd;
pthread_t *r_thd;
pthread_mutex_t *t_mtx;
int *t_sent, *t_rcvd;

uint64_t t_start = 0, t_end = 0;
int c_start = 0;

int num_s = NUM_S, num_c = NUM_C;
int msg_rate = MSG_RATE;
    
static void display(void) {
    int i, s = 0, r = 0;

    for (i=0; i<num_c; i++) {
        pthread_mutex_lock(&t_mtx[i]);
        s += t_sent[i];
        r += t_rcvd[i];
        pthread_mutex_unlock(&t_mtx[i]);
    }

    uint64_t t_time = t_end - t_start;

    printf("TOTAL SENT:%d RCVD:%d\n", s, r);
    printf("L:%f%%\n", (s-r)/(float)s*100);
    printf("T/S S:%f R:%f\n", s/((float)t_time/1000000), r/((float)t_time/1000000));
}

static void catchINFO(int sig) {
    struct timeval tv;

    if (!c_start) {
        gettimeofday(&tv, NULL);
        t_start = tv.tv_sec*(uint64_t)1000000+tv.tv_usec;
        c_start = 1;
        printf("COUNTER START\n");
    } else {
        gettimeofday(&tv, NULL);
        t_end = tv.tv_sec*(uint64_t)1000000+tv.tv_usec;
        display();
    }
}

void *sender(ECPConnection *c) {
    int idx = (int)(c->conn_data);
    unsigned char payload[ECP_SIZE_PLD(1000)];
    
    printf("OPEN:%d\n", idx);
    while(1) {
        uint32_t rnd;
        c->sock->ctx->rng(&rnd, sizeof(uint32_t));
        usleep(rnd % (2000000/msg_rate));

        ecp_pld_set_type(payload, MTYPE_MSG);
        ssize_t _rv = ecp_send(c, payload, sizeof(payload));
        if (c_start && (_rv > 0)) {
            pthread_mutex_lock(&t_mtx[idx]);
            t_sent[idx]++;
            pthread_mutex_unlock(&t_mtx[idx]);
        }
    }
}

ssize_t handle_open_c(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *p, ssize_t s) {
    int idx = (int)(conn->conn_data);
    int rv = 0;
    
    ecp_conn_handle_open(conn, sq, t, p, s);
    rv = pthread_create(&s_thd[idx], NULL, (void *(*)(void *))sender, (void *)conn);
    if (rv) {
        char msg[256];
        sprintf(msg, "THD %d CREATE\n", idx);
        perror(msg);
        exit(1);
    }
    return 0;
}

ssize_t handle_msg_c(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *p, ssize_t s) {
    int idx = (int)(conn->conn_data);
    unsigned char payload[ECP_SIZE_PLD(1000)];

    if (c_start) {
        pthread_mutex_lock(&t_mtx[idx]);
        t_rcvd[idx]++;
        pthread_mutex_unlock(&t_mtx[idx]);
    }
    
    // ecp_pld_set_type(payload, MTYPE_MSG);
    // ssize_t _rv = ecp_send(c, payload, sizeof(payload));
    return s;
}

ssize_t handle_msg_s(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *p, ssize_t s) {
    unsigned char payload[ECP_SIZE_PLD(1000)];
    ecp_pld_set_type(payload, MTYPE_MSG);
    ssize_t _rv = ecp_send(conn, payload, sizeof(payload));
    return s;
}

int main(int argc, char *argv[]) {
    char addr[256];
    int rv;
    int i;
    
    ECPConnHandler handler_c;

    ECPContext *ctx_c;
    ECPSocket *sock_c;
    ECPDHKey *key_perma_c;

    ECPNode *node;
    ECPConnection *conn;
    
    sock_s = malloc(num_s * sizeof(ECPSocket));
    ctx_c = malloc(num_c * sizeof(ECPContext));
    sock_c = malloc(num_c * sizeof(ECPSocket));
    key_perma_c = malloc(num_c * sizeof(ECPDHKey));
    node = malloc(num_c * sizeof(ECPNode));
    conn = malloc(num_c * sizeof(ECPConnection));

    s_thd = malloc(num_c * sizeof(pthread_t));
    r_thd = malloc(num_c * sizeof(pthread_t));
    t_mtx = malloc(num_c * sizeof(pthread_mutex_t));
    t_sent = malloc(num_c * sizeof(int));
    t_rcvd = malloc(num_c * sizeof(int));
    memset(t_rcvd, 0, num_c * sizeof(int));
    memset(t_sent, 0, num_c * sizeof(int));
    
    struct sigaction actINFO;
    memset(&actINFO, 0, sizeof(actINFO));
    actINFO.sa_handler = &catchINFO;
    sigaction(SIGINFO, &actINFO, NULL);

    rv = ecp_init(&ctx_s);
    if (!rv) rv = ecp_conn_handler_init(&handler_s);
    handler_s.msg[MTYPE_MSG] = handle_msg_s;
    ctx_s.handler[CTYPE_TEST] = &handler_s;

    if (!rv) rv = ecp_dhkey_generate(&ctx_s, &key_perma_s);

    for (i=0; i<num_s; i++) {
        if (!rv) rv = ecp_sock_create(&sock_s[i], &ctx_s, &key_perma_s);

        strcpy(addr, "0.0.0.0:");
        sprintf(addr+strlen(addr), "%d", 3000+i);
        if (!rv) rv = ecp_sock_open(&sock_s[i], addr);

        if (!rv) rv = pthread_create(&r_thd[i], NULL, (void *(*)(void *))ecp_receiver, (void *)&sock_s[i]);

        if (rv) {
            char msg[256];
            sprintf(msg, "SERVER %d CREATE:%d\n", i, rv);
            perror(msg);
            exit(1);
        }
    }

    rv = ecp_conn_handler_init(&handler_c);

    handler_c.msg[ECP_MTYPE_OPEN] = handle_open_c;
    handler_c.msg[MTYPE_MSG] = handle_msg_c;
    
    for (i=0; i<num_c; i++) {
        pthread_mutex_init(&t_mtx[i], NULL);
        
        if (!rv) rv = ecp_init(&ctx_c[i]);
        ctx_c[i].handler[CTYPE_TEST] = &handler_c;
        
        if (!rv) rv = ecp_dhkey_generate(&ctx_c[i], &key_perma_c[i]);
        if (!rv) rv = ecp_sock_create(&sock_c[i], &ctx_c[i], &key_perma_c[i]);
        if (!rv) rv = ecp_sock_open(&sock_c[i], NULL);

        if (!rv) rv = pthread_create(&r_thd[i], NULL, (void *(*)(void *))ecp_receiver, (void *)&sock_c[i]);

        strcpy(addr, "127.0.0.1:");
        sprintf(addr+strlen(addr), "%d", 3000 + (i % num_s));
        if (!rv) rv = ecp_node_init(&ctx_c[i], &node[i], &key_perma_s.public, addr);
        
        if (!rv) rv = ecp_conn_create(&conn[i], &sock_c[i], CTYPE_TEST);
        conn[i].conn_data = (void *)i;
        
        if (!rv) rv = ecp_conn_open(&conn[i], &node[i]);
        
        if (rv) {
            char msg[256];
            sprintf(msg, "CLIENT %d CREATE:%d\n", i, rv);
            perror(msg);
            exit(1);
        }
        
    }
    while (1) sleep(1);
}