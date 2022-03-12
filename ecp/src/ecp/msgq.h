#define ECP_MSGQ_MAX_MSG        32

#define ECP_MSGQ_MAX_MTYPE      (ECP_MAX_MTYPE)

typedef struct ECPMsgQ {
    unsigned short idx_w[ECP_MSGQ_MAX_MTYPE];
    unsigned short idx_r[ECP_MSGQ_MAX_MTYPE];
    ecp_seq_t seq_start;
    ecp_seq_t seq_max;
    ecp_seq_t seq_msg[ECP_MSGQ_MAX_MTYPE][ECP_MSGQ_MAX_MSG];
    pthread_cond_t cond[ECP_MSGQ_MAX_MTYPE];
} ECPMsgQ;

int ecp_msgq_create(ECPRBConn *conn, ECPMsgQ *msgq);
void ecp_msgq_destroy(ECPRBConn *conn);
void ecp_msgq_start(ECPRBConn *conn, ecp_seq_t seq);
int ecp_msgq_push(ECPRBConn *conn, ecp_seq_t seq, unsigned char mtype);
ssize_t ecp_msgq_pop(ECPRBConn *conn, unsigned char mtype, unsigned char *msg, size_t _msg_size, ecp_sts_t timeout);
ssize_t ecp_msg_receive(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, ecp_sts_t timeout);