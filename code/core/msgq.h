#ifdef ECP_WITH_PTHREAD

#include <pthread.h>
#include <string.h>

#define ECP_MAX_CONN_MSG        8
#define ECP_ERR_MAX_CONN_MSG    -100

struct ECPConnection;

typedef struct ECPMessage {
    unsigned char msg[ECP_MAX_MSG];
    ssize_t size;
} ECPMessage;

typedef struct ECPConnMsgQ {
    unsigned short empty_idx;
    unsigned short occupied[ECP_MAX_CONN_MSG];
    unsigned short w_idx[ECP_MAX_MTYPE];
    unsigned short r_idx[ECP_MAX_MTYPE];
    unsigned short msg_idx[ECP_MAX_MTYPE][ECP_MAX_CONN_MSG+1];
    ECPMessage msg[ECP_MAX_CONN_MSG];
    pthread_mutex_t mutex;
    pthread_cond_t cond[ECP_MAX_MTYPE];
} ECPConnMsgQ;

int ecp_conn_msgq_create(struct ECPConnection *conn);
void ecp_conn_msgq_destroy(struct ECPConnection *conn);
ssize_t ecp_conn_msgq_push(struct ECPConnection *conn, unsigned char *msg, size_t msg_size);
ssize_t ecp_conn_msgq_pop(struct ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, unsigned int timeout);

#endif  /* ECP_WITH_PTHREAD */