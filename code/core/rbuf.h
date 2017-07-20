#define ECP_RBUF_FLAG_PRESENT        1

#define ECP_ERR_RBUF_FLAG   -100
#define ECP_ERR_RBUF_IDX    -101
#define ECP_ERR_RBUF_DUP    -102

typedef uint32_t ecp_ack_t;
typedef uint32_t ecp_win_t;

#define ECP_RBUF_SEQ_HALF            ((ecp_seq_t)1 << (sizeof(ecp_seq_t)*8-1))

#define ECP_RBUF_ACK_FULL            (~(ecp_ack_t)0)
#define ECP_RBUF_ACK_SIZE            (sizeof(ecp_ack_t)*8)

#define ECP_RBUF_SEQ_LT(a,b)         ((ecp_seq_t)((ecp_seq_t)(a) - (ecp_seq_t)(b)) > ECP_RBUF_SEQ_HALF)
#define ECP_RBUF_SEQ_LTE(a,b)        ((ecp_seq_t)((ecp_seq_t)(b) - (ecp_seq_t)(a)) < ECP_RBUF_SEQ_HALF)

#define ECP_RBUF_IDX_MASK(idx, size) ((idx) & ((size) - 1))
/* If size not 2^x:
#define ECP_RBUF_IDX_MASK(idx, size) ((idx) % (size))
*/

typedef struct ECPRBMessage {
    unsigned char msg[ECP_MAX_PKT];
    ssize_t size;
    unsigned char flags;
} ECPRBMessage;

typedef struct ECPRBuffer {
    ecp_seq_t seq_start;
    unsigned int msg_size;
    unsigned int msg_start;
    ECPRBMessage *msg;
} ECPRBuffer;

typedef struct ECPRBRecv {
    unsigned char reliable;
    unsigned short deliver_delay;
    unsigned short hole_max;
    unsigned short ack_rate;
    ecp_seq_t seq_ack;
    ecp_seq_t seq_max;
    ecp_seq_t ack_pkt;
    ecp_ack_t ack_map;
    ecp_ack_t hole_mask_full;
    ecp_ack_t hole_mask_empty;
    ECPRBuffer rbuf;
} ECPRBRecv;

typedef struct ECPRBSend {
    unsigned char reliable;
    ecp_win_t win_size;
    ecp_win_t in_transit;
    unsigned int nack_rate;
    ECPRBuffer rbuf;
} ECPRBSend;

typedef struct ECPConnRBuffer {
    ECPRBRecv *recv;
    ECPRBSend *send;
} ECPConnRBuffer;


int ecp_rbuf_init(ECPRBuffer *rbuf, ECPRBMessage *msg, unsigned int msg_size);
int ecp_rbuf_msg_idx(ECPRBuffer *rbuf, ecp_seq_t seq);
ssize_t ecp_rbuf_msg_store(ECPRBuffer *rbuf, ecp_seq_t seq, unsigned char *msg, size_t msg_size, unsigned char test_flags, unsigned char set_flags);

int ecp_rbuf_recv_create(ECPRBRecv *buf, ECPRBMessage *msg, unsigned int msg_size);
int ecp_rbuf_recv_start(ECPRBRecv *buf, ecp_seq_t seq);
int ecp_rbuf_recv_set_hole(ECPRBRecv *buf, unsigned short hole_max);
int ecp_rbuf_recv_set_delay(ECPRBRecv *buf, unsigned short delay);
ssize_t ecp_rbuf_recv_store(struct ECPConnection *conn, ecp_seq_t seq, unsigned char *msg, size_t msg_size);


ssize_t ecp_rbuf_send_store(struct ECPConnection *conn, ecp_seq_t seq, unsigned char *msg, size_t msg_size);

