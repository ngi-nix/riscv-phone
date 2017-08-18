#define ECP_RBUF_FLAG_IN_RBUF       0x01
#define ECP_RBUF_FLAG_IN_MSGQ       0x02
#define ECP_RBUF_FLAG_IN_CCONTROL   0x04
#define ECP_RBUF_FLAG_SYS           0x80

#define ECP_RBUF_FLAG_CCONTROL      0x01
#define ECP_RBUF_FLAG_RELIABLE      0x02
#define ECP_RBUF_FLAG_MSGQ          0x04

#define ECP_MTYPE_RBACK             0x04
#define ECP_MTYPE_RBFLUSH           0x05

#define ECP_ERR_RBUF_DUP            -100
#define ECP_ERR_RBUF_FULL           -101

typedef uint32_t ecp_ack_t;
typedef uint32_t ecp_win_t;

#define ECP_RBUF_SEQ_HALF               ((ecp_seq_t)1 << (sizeof(ecp_seq_t)*8-1))

#define ECP_RBUF_ACK_FULL               (~(ecp_ack_t)0)
#define ECP_RBUF_ACK_SIZE               (sizeof(ecp_ack_t)*8)

#define ECP_RBUF_SEQ_LT(a,b)            ((ecp_seq_t)((ecp_seq_t)(a) - (ecp_seq_t)(b)) > ECP_RBUF_SEQ_HALF)
#define ECP_RBUF_SEQ_LTE(a,b)           ((ecp_seq_t)((ecp_seq_t)(b) - (ecp_seq_t)(a)) < ECP_RBUF_SEQ_HALF)

/* size must be power of 2 */
#define ECP_RBUF_IDX_MASK(idx, size)    ((idx) & ((size) - 1))

#ifdef ECP_WITH_MSGQ
#include "msgq.h"
#endif

typedef struct ECPRBMessage {
    unsigned char msg[ECP_MAX_PKT];
    ssize_t size;
    unsigned char flags;
} ECPRBMessage;

typedef struct ECPRBuffer {
    ecp_seq_t seq_start;
    ecp_seq_t seq_max;
    unsigned int msg_size;
    unsigned int msg_start;
    ECPRBMessage *msg;
} ECPRBuffer;

typedef struct ECPRBRecv {
    unsigned char flags;
    unsigned char flush;
    unsigned short deliver_delay;
    unsigned short hole_max;
    unsigned short ack_rate;
    ecp_seq_t seq_ack;
    ecp_seq_t ack_pkt;
    ecp_ack_t ack_map;
    ecp_ack_t hole_mask_full;
    ecp_ack_t hole_mask_empty;
    ECPRBuffer rbuf;
#ifdef ECP_WITH_MSGQ
    ECPConnMsgQ msgq;
#endif
} ECPRBRecv;

typedef struct ECPRBSend {
    unsigned char flags;
    unsigned char flush;
    ecp_win_t win_size;
    ecp_win_t in_transit;
    ecp_win_t cnt_cc;
    ecp_seq_t seq_cc;
    ecp_seq_t seq_flush;
    unsigned int nack_rate;
    ECPRBuffer rbuf;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_t mutex;
#endif
} ECPRBSend;

typedef struct ECPConnRBuffer {
    ECPRBRecv *recv;
    ECPRBSend *send;
} ECPConnRBuffer;

typedef struct ECPRBInfo {
    ecp_seq_t seq;
    unsigned int idx;
    unsigned char mtype;
    unsigned char seq_force;
} ECPRBInfo;

int ecp_rbuf_init(ECPRBuffer *rbuf, ECPRBMessage *msg, unsigned int msg_size);
int ecp_rbuf_start(ECPRBuffer *rbuf, ecp_seq_t seq);
int ecp_rbuf_msg_idx(ECPRBuffer *rbuf, ecp_seq_t seq);
ssize_t ecp_rbuf_msg_store(ECPRBuffer *rbuf, ecp_seq_t seq, int idx, unsigned char *msg, size_t msg_size, unsigned char test_flags, unsigned char set_flags);
ssize_t ecp_rbuf_pld_send(struct ECPConnection *conn, unsigned char *payload, size_t payload_size, ecp_seq_t seq);

int ecp_rbuf_info_init(ECPRBInfo *rbuf_info);
int ecp_rbuf_pkt_prep(ECPRBSend *buf, ecp_seq_t seq, unsigned char mtype, ECPRBInfo *rbuf_info);
ssize_t ecp_rbuf_pkt_send(ECPRBSend *buf, struct ECPSocket *sock, ECPNetAddr *addr, ECPTimerItem *ti, unsigned char *packet, size_t pkt_size, ECPRBInfo *rbuf_info);

int ecp_rbuf_conn_create(struct ECPConnection *conn, ECPRBSend *buf_s, ECPRBMessage *msg_s, unsigned int msg_s_size, ECPRBRecv *buf_r, ECPRBMessage *msg_r, unsigned int msg_r_size);
void ecp_rbuf_conn_destroy(struct ECPConnection *conn);
int ecp_rbuf_conn_start(struct ECPConnection *conn, ecp_seq_t seq);

int ecp_rbuf_recv_create(struct ECPConnection *conn, ECPRBRecv *buf, ECPRBMessage *msg, unsigned int msg_size);
void ecp_rbuf_recv_destroy(struct ECPConnection *conn);
int ecp_rbuf_recv_set_hole(struct ECPConnection *conn, unsigned short hole_max);
int ecp_rbuf_recv_set_delay(struct ECPConnection *conn, unsigned short delay);
int ecp_rbuf_recv_start(struct ECPConnection *conn, ecp_seq_t seq);
ssize_t ecp_rbuf_recv_store(struct ECPConnection *conn, ecp_seq_t seq, unsigned char *msg, size_t msg_size);

int ecp_rbuf_send_create(struct ECPConnection *conn, ECPRBSend *buf, ECPRBMessage *msg, unsigned int msg_size);
void ecp_rbuf_send_destroy(struct ECPConnection *conn);
int ecp_rbuf_send_start(struct ECPConnection *conn);

ssize_t ecp_rbuf_handle_ack(struct ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size);
ssize_t ecp_rbuf_handle_flush(struct ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size);
