#ifdef ECP_WITH_PTHREAD

#include <pthread.h>
#include <string.h>

#define ECP_MAX_CONN_MSG        8
#define ECP_ERR_MAX_CONN_MSG    -100

struct ECPConnection;

typedef struct ECPMessage {
    unsigned char payload[ECP_MAX_PKT];
    ssize_t size;
} ECPMessage;

typedef struct ECPConnMsgQ {
    unsigned short empty_idx;
    unsigned short occupied[ECP_MAX_CONN_MSG];
    unsigned short w_idx[ECP_MAX_PTYPE];
    unsigned short r_idx[ECP_MAX_PTYPE];
    unsigned short msg_idx[ECP_MAX_PTYPE][ECP_MAX_CONN_MSG+1];
    ECPMessage msg[ECP_MAX_CONN_MSG];
    pthread_mutex_t mutex;
    pthread_cond_t cond[ECP_MAX_PTYPE];
} ECPConnMsgQ;

int ecp_conn_msgq_create(struct ECPConnection *conn);
void ecp_conn_msgq_destroy(struct ECPConnection *conn);
ssize_t ecp_conn_msgq_push(struct ECPConnection *conn, unsigned char *payload, size_t payload_size);
ssize_t ecp_conn_msgq_pop(struct ECPConnection *conn, unsigned char ptype, unsigned char *payload, size_t payload_size, unsigned int timeout);

#endif  /* ECP_WITH_PTHREAD */