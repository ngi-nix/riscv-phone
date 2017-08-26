#ifdef ECP_WITH_MSGQ

#include <pthread.h>
#include <string.h>

#define ECP_MSGQ_MAX_MSG        32
#define ECP_MSGQ_ERR_MAX_MSG    -110

typedef struct ECPConnMsgQ {
    unsigned short idx_w[ECP_MAX_MTYPE];
    unsigned short idx_r[ECP_MAX_MTYPE];
    ecp_seq_t seq_start;
    ecp_seq_t seq_max;
    ecp_seq_t seq_msg[ECP_MAX_MTYPE][ECP_MSGQ_MAX_MSG];
    pthread_cond_t cond[ECP_MAX_MTYPE];
    pthread_mutex_t mutex;
} ECPConnMsgQ;

int ecp_conn_msgq_create(struct ECPConnection *conn);
void ecp_conn_msgq_destroy(struct ECPConnection *conn);
int ecp_conn_msgq_start(struct ECPConnection *conn, ecp_seq_t seq);

int ecp_conn_msgq_push(struct ECPConnection *conn, ecp_seq_t seq, unsigned char mtype);
ssize_t ecp_conn_msgq_pop(struct ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, ecp_cts_t timeout);

#endif  /* ECP_WITH_MSGQ */