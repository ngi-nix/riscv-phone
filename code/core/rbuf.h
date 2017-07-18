#define ECP_ERR_RBUF_IDX    -100
#define ECP_ERR_RBUF_DUP    -101

#define ECP_MAX_RBUF_MSGR    256

typedef uint32_t ecp_ack_t;

typedef struct ECPRBMessage {
    unsigned char msg[ECP_MAX_MSG];
    ssize_t size;
    char present;
} ECPRBMessage;

typedef struct ECPRBRecvBuffer {
    unsigned char reliable;
    unsigned char deliver_delay;
    unsigned char hole_max;
    int msg_start;
    ecp_seq_t seq_ack;
    ecp_seq_t seq_max;
    ecp_seq_t seq_start;
    ecp_ack_t ack_map;
    ecp_ack_t hole_mask_full;
    ecp_ack_t hole_mask_empty;
    ECPRBMessage msg[ECP_MAX_RBUF_MSGR];
} ECPRBRecvBuffer;


typedef struct ECPConnRBuffer {
    ECPRBRecvBuffer *recv;
    // ECPSBuffer *send;
} ECPConnRBuffer;

ssize_t ecp_rbuf_recv_store(struct ECPConnection *conn, ecp_seq_t seq, unsigned char *msg, size_t msg_size);