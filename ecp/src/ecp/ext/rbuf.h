#define ECP_RBUF_FLAG_IN_RBUF       0x01
#define ECP_RBUF_FLAG_IN_MSGQ       0x02
#define ECP_RBUF_FLAG_IN_TIMER      0x04
#define ECP_RBUF_FLAG_SKIP          0x08

#define ECP_RBUF_FLAG_CCONTROL      0x01
#define ECP_RBUF_FLAG_RELIABLE      0x02


#define ECP_MTYPE_RBNOP             (0x08 | ECP_MTYPE_FLAG_SYS)
#define ECP_MTYPE_RBACK             (0x09 | ECP_MTYPE_FLAG_SYS)
#define ECP_MTYPE_RBFLUSH           (0x0a | ECP_MTYPE_FLAG_SYS)
#define ECP_MTYPE_RBTIMER           (0x0b | ECP_MTYPE_FLAG_SYS)

#define ECP_ERR_RBUF_DUP            -100
#define ECP_ERR_RBUF_TIMER          -101

#define ecp_rbuf_skip(mtype)        (mtype & ECP_MTYPE_FLAG_SYS)

/* size must be power of 2 */
#define ECP_RBUF_IDX_MASK(idx, size)    ((idx) & ((size) - 1))

typedef uint32_t ecp_win_t;

struct ECPMsgQ;
struct ECPFragIter;

typedef struct ECPRBPayload {
    unsigned char buf[ECP_MAX_PLD];
    size_t size;
    unsigned char flags;
} ECPRBPayload;

typedef struct ECPRBPacket {
    unsigned char buf[ECP_MAX_PKT];
    size_t size;
    unsigned char flags;
} ECPRBPacket;

typedef struct ECPRBuffer {
    ecp_seq_t seq_start;
    ecp_seq_t seq_max;
    unsigned short arr_size;
    unsigned short idx_start;
    union {
        ECPRBPayload *pld;
        ECPRBPacket *pkt;
    } arr;
} ECPRBuffer;

typedef struct ECPRBRecv {
    unsigned char start;
    unsigned char flags;
    ecp_sts_t deliver_delay;
    unsigned short hole_max;
    unsigned short ack_rate;
    unsigned short ack_pkt;
    ecp_seq_t seq_ack;
    ecp_ack_t ack_map;
    ECPRBuffer rbuf;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_t mutex;
#endif
#ifdef ECP_WITH_MSGQ
    struct ECPMsgQ *msgq;
#endif
} ECPRBRecv;

typedef struct ECPRBSend {
    unsigned char start;
    unsigned char flags;
    ecp_win_t win_size;
    ecp_win_t in_transit;
    ecp_win_t cnt_cc;
    ecp_seq_t seq_cc;
    ecp_seq_t seq_flush;
    ecp_seq_t seq_nack;
    unsigned char flush;
    unsigned int nack_rate;
    ECPRBuffer rbuf;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_t mutex;
#endif
} ECPRBSend;

typedef struct ECPRBConn {
    ECPConnection b;
    ECPRBRecv *recv;
    ECPRBSend *send;
    struct ECPFragIter *iter;
} ECPRBConn;

ECPRBConn *ecp_rbuf_get_rbconn(ECPConnection *conn);
ECPConnection *ecp_rbuf_get_conn(ECPRBConn *conn);
void _ecp_rbuf_start(ECPRBuffer *rbuf, ecp_seq_t seq);
int _ecp_rbuf_msg_idx(ECPRBuffer *rbuf, ecp_seq_t seq, unsigned short *idx);
void ecp_rbuf_conn_init(ECPRBConn *conn);
int ecp_rbuf_conn_create(ECPRBConn *conn, ECPSocket *sock, unsigned char type);
int ecp_rbuf_conn_create_inb(ECPRBConn *conn, ECPSocket *sock, unsigned char type);
void ecp_rbuf_destroy(ECPRBConn *conn);
void ecp_rbuf_start(ECPRBConn *conn);
ssize_t ecp_rbuf_msg_handle(ECPRBConn *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *bufs);
int ecp_rbuf_err_handle(ECPRBConn *conn, unsigned char mtype, int err);

/* send */
ssize_t ecp_rbuf_send_flush(ECPRBConn *conn);
ssize_t ecp_rbuf_handle_ack(ECPRBConn *conn, unsigned char *msg, size_t msg_size);
int ecp_rbsend_create(ECPRBConn *conn, ECPRBSend *buf, ECPRBPacket *pkt, unsigned short pkt_size);
void ecp_rbsend_destroy(ECPRBConn *conn);
void ecp_rbsend_start(ECPRBConn *conn, ecp_seq_t seq);
int ecp_rbuf_set_wsize(ECPRBConn *conn, ecp_win_t size);
int ecp_rbuf_flush(ECPRBConn *conn);
ssize_t ecp_rbuf_pld_send(ECPRBConn *conn, ECPBuffer *payload, size_t pld_size, ECPBuffer *packet, size_t pkt_size, ECPTimerItem *ti);

ssize_t ecp_rbuf_handle_nop(ECPRBConn *conn, unsigned char *msg, size_t msg_size);
ssize_t ecp_rbuf_handle_flush(ECPRBConn *conn);
void ecp_rbuf_handle_timer(ECPRBConn *conn) ;
int ecp_rbrecv_create(ECPRBConn *conn, ECPRBRecv *buf, ECPRBPayload *pld, unsigned short pld_size);
void ecp_rbrecv_destroy(ECPRBConn *conn);
void ecp_rbrecv_start(ECPRBConn *conn, ecp_seq_t seq);
int ecp_rbuf_set_hole(ECPRBConn *conn, unsigned short hole_max);
int ecp_rbuf_set_delay(ECPRBConn *conn, ecp_sts_t delay);
ssize_t ecp_rbuf_store(ECPRBConn *conn, ecp_seq_t seq, unsigned char *pld, size_t pld_size);
