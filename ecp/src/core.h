#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef ECP_WITH_PTHREAD
#include <pthread.h>
#endif

#define ECP_MEM_TINY                1

#define ECP_OK                      0
#define ECP_ITER_NEXT               1

#define ECP_ERR                     -1
#define ECP_ERR_TIMEOUT             -2
#define ECP_ERR_ALLOC               -3
#define ECP_ERR_SIZE                -4
#define ECP_ERR_ITER                -5

#define ECP_ERR_MAX_SOCK_CONN       -10
#define ECP_ERR_MAX_CTYPE           -11
#define ECP_ERR_MAX_MTYPE           -12
#define ECP_ERR_MIN_PKT             -13
#define ECP_ERR_MAX_PLD             -14
// XXX ???
#define ECP_ERR_MIN_MSG             -15
#define ECP_ERR_MAX_MSG             -16
//
#define ECP_ERR_NET_ADDR            -17

#define ECP_ERR_CONN_NOT_FOUND      -20
#define ECP_ERR_ECDH_KEY_DUP        -21
#define ECP_ERR_ECDH_IDX            -22
#define ECP_ERR_ECDH_IDX_LOCAL      -23
#define ECP_ERR_ECDH_IDX_REMOTE     -24
#define ECP_ERR_RNG                 -25
#define ECP_ERR_ENCRYPT             -26
#define ECP_ERR_DECRYPT             -27
#define ECP_ERR_SEND                -28
#define ECP_ERR_RECV                -29
#define ECP_ERR_SEQ                 -30
#define ECP_ERR_CLOSED              -31
#define ECP_ERR_HANDLE              -32
#define ECP_ERR_NOT_IMPLEMENTED     -99

#define ECP_SIZE_PROTO              2
#define ECP_SIZE_SEQ                4

#define ECP_MAX_SOCK_CONN           16
#define ECP_MAX_SOCK_KEY            8
#define ECP_MAX_CONN_KEY            2
#define ECP_MAX_NODE_KEY            2
#define ECP_MAX_CTYPE               8
#define ECP_MAX_MTYPE               16
#define ECP_MAX_MTYPE_SYS           4
#define ECP_MAX_MTYPE_SOCK          ECP_MAX_MTYPE_SYS
#define ECP_MAX_SEQ_FWD             1024

#define ECP_SIZE_PKT_HDR            (ECP_SIZE_PROTO+1+ECP_ECDH_SIZE_KEY+ECP_AEAD_SIZE_NONCE)
#define ECP_SIZE_PLD_HDR            (ECP_SIZE_SEQ)

#define ECP_MAX_PKT                 1412
#define ECP_MAX_PLD                 (ECP_MAX_PKT-ECP_SIZE_PKT_HDR-ECP_AEAD_SIZE_TAG)
#define ECP_MAX_MSG                 (ECP_MAX_PLD-ECP_SIZE_PLD_HDR-1)

#define ECP_MIN_PKT                 (ECP_SIZE_PKT_HDR+ECP_SIZE_PLD_HDR+1+ECP_AEAD_SIZE_TAG)

#define ECP_POLL_TIMEOUT            500
#define ECP_ECDH_IDX_INV            0xFF
#define ECP_ECDH_IDX_PERMA          0x0F

#define ECP_MTYPE_FLAG_FRAG         0x80
#define ECP_MTYPE_FLAG_PTS          0x40
#define ECP_MTYPE_FLAG_REP          0x20
#define ECP_MTYPE_MASK              0x1f

#define ECP_MTYPE_OPEN              0x00
#define ECP_MTYPE_KGET              0x01
#define ECP_MTYPE_KPUT              0x02
#define ECP_MTYPE_DIR               0x03

#define ECP_MTYPE_OPEN_REQ          (ECP_MTYPE_OPEN)
#define ECP_MTYPE_OPEN_REP          (ECP_MTYPE_OPEN | ECP_MTYPE_FLAG_REP)
#define ECP_MTYPE_KGET_REQ          (ECP_MTYPE_KGET)
#define ECP_MTYPE_KGET_REP          (ECP_MTYPE_KGET | ECP_MTYPE_FLAG_REP)
#define ECP_MTYPE_KPUT_REQ          (ECP_MTYPE_KPUT)
#define ECP_MTYPE_KPUT_REP          (ECP_MTYPE_KPUT | ECP_MTYPE_FLAG_REP)
#define ECP_MTYPE_DIR_REQ           (ECP_MTYPE_DIR)
#define ECP_MTYPE_DIR_REP           (ECP_MTYPE_DIR  | ECP_MTYPE_FLAG_REP)

#define ECP_CONN_FLAG_REG           0x01
#define ECP_CONN_FLAG_OPEN          0x02

#define ECP_SEND_FLAG_REPLY         0x01
#define ECP_SEND_FLAG_MORE          0x02

#define ecp_conn_is_reg(conn)       ((conn->flags) & ECP_CONN_FLAG_REG)
#define ecp_conn_is_open(conn)      ((conn->flags) & ECP_CONN_FLAG_OPEN)

// typedef long ssize_t;

typedef uint32_t ecp_ack_t;
#define ECP_SIZE_ACKB               (sizeof(ecp_ack_t)*8)
#define ECP_ACK_FULL                (~(ecp_ack_t)0)

typedef uint32_t ecp_cts_t;
#define ECP_CTS_HALF                ((ecp_cts_t)1 << (sizeof(ecp_cts_t) * 8 - 1))
#define ECP_CTS_LT(a,b)             ((ecp_cts_t)((ecp_cts_t)(a) - (ecp_cts_t)(b)) > ECP_CTS_HALF)
#define ECP_CTS_LTE(a,b)            ((ecp_cts_t)((ecp_cts_t)(b) - (ecp_cts_t)(a)) < ECP_CTS_HALF)

typedef uint32_t ecp_pts_t;
#define ECP_PTS_HALF                ((ecp_pts_t)1 << (sizeof(ecp_pts_t) * 8 - 1))
#define ECP_PTS_LT(a,b)             ((ecp_pts_t)((ecp_pts_t)(a) - (ecp_pts_t)(b)) > ECP_PTS_HALF)
#define ECP_PTS_LTE(a,b)            ((ecp_pts_t)((ecp_pts_t)(b) - (ecp_pts_t)(a)) < ECP_PTS_HALF)

typedef uint32_t ecp_seq_t;
#define ECP_SEQ_HALF                ((ecp_seq_t)1 << (sizeof(ecp_seq_t) * 8 - 1))
#define ECP_SEQ_LT(a,b)             ((ecp_seq_t)((ecp_seq_t)(a) - (ecp_seq_t)(b)) > ECP_SEQ_HALF)
#define ECP_SEQ_LTE(a,b)            ((ecp_seq_t)((ecp_seq_t)(b) - (ecp_seq_t)(a)) < ECP_SEQ_HALF)


#define ECP_SIZE_MT_FRAG(F)         ((F) & ECP_MTYPE_FLAG_FRAG ? 2 + sizeof(uint16_t) : 0)
#define ECP_SIZE_MT_PTS(F)          ((F) & ECP_MTYPE_FLAG_PTS ? sizeof(ecp_pts_t) : 0)
#define ECP_SIZE_MT_FLAG(F)         (ECP_SIZE_MT_FRAG(F)+ECP_SIZE_MT_PTS(F))
#define ECP_SIZE_PLD(X,F)           ((X) + ECP_SIZE_PLD_HDR+1+ECP_SIZE_MT_FLAG(F))
#define ECP_SIZE_PKT(X,F)           (ECP_SIZE_PKT_HDR+ECP_SIZE_PLD(X,F)+ECP_AEAD_SIZE_TAG)

#define ECP_SIZE_MSG_BUF(T,P)       ((P) && (P)->out && (((T) == ECP_MTYPE_OPEN_REQ) || ((T) == ECP_MTYPE_KGET_REQ)) ? ECP_SIZE_PLD_HDR+3+2*ECP_ECDH_SIZE_KEY : ECP_SIZE_PLD_HDR+1)

#define ECP_SIZE_PLD_BUF(X,T,C)     (ECP_SIZE_PLD(X,T)+(C)->pcount*(ECP_SIZE_PKT_HDR+ECP_SIZE_MSG_BUF(T,(C)->parent)+ECP_AEAD_SIZE_TAG))
#define ECP_SIZE_PKT_BUF(X,T,C)     (ECP_SIZE_PLD_BUF(X,T,C)+ECP_SIZE_PKT_HDR+ECP_AEAD_SIZE_TAG)

#define ECP_SIZE_PLD_BUF_TR(X,T,P)  (ECP_SIZE_PLD(X,T)+((P) ? ((P)->pcount+1)*(ECP_SIZE_PKT_HDR+ECP_SIZE_MSG_BUF(T,P)+ECP_AEAD_SIZE_TAG) : 0))
#define ECP_SIZE_PKT_BUF_TR(X,T,P)  (ECP_SIZE_PLD_BUF_TR(X,T,P)+ECP_SIZE_PKT_HDR+ECP_AEAD_SIZE_TAG)

#ifdef ECP_DEBUG
#include <stdio.h>
#endif

struct ECPBuffer;
struct ECP2Buffer;
struct ECPContext;
struct ECPSocket;
struct ECPConnection;
struct ECPSeqItem;
struct ECPPktMeta;
struct ECPDirList;

#include "platform/transport.h"
#include "crypto/crypto.h"
#include "timer.h"
#include "dir_srv.h"

#ifdef ECP_WITH_RBUF
#include "rbuf.h"
#endif

typedef int (*ecp_rng_t) (void *, size_t);

typedef int (*ecp_sock_handler_msg_t) (struct ECPSocket *s, ECPNetAddr *a, struct ECPConnection *p, unsigned char *msg, size_t sz, struct ECPPktMeta *m, struct ECP2Buffer *b, struct ECPConnection **c);
typedef ssize_t (*ecp_conn_handler_msg_t) (struct ECPConnection *c, ecp_seq_t s, unsigned char t, unsigned char *msg, ssize_t sz, struct ECP2Buffer *b);

typedef struct ECPConnection * (*ecp_conn_alloc_t) (unsigned char t);
typedef void (*ecp_conn_free_t) (struct ECPConnection *c);
typedef int (*ecp_conn_create_t) (struct ECPConnection *c, unsigned char *msg, size_t sz);
typedef void (*ecp_conn_destroy_t) (struct ECPConnection *c);
typedef ssize_t (*ecp_conn_open_t) (struct ECPConnection *c);
typedef void (*ecp_conn_close_t) (struct ECPConnection *c);

typedef struct ECPBuffer {
    unsigned char *buffer;
    size_t size;
} ECPBuffer;

typedef struct ECP2Buffer {
    ECPBuffer *packet;
    ECPBuffer *payload;
} ECP2Buffer;

typedef struct ECPDHKey {
    ecp_dh_public_t public;
    ecp_dh_private_t private;
    unsigned char valid;
} ECPDHKey;

typedef struct ECPDHRKey {
    unsigned char idx;
    ecp_dh_public_t public;
} ECPDHRKey;

typedef struct ECPDHShared {
    ecp_aead_key_t secret;
    unsigned char valid;
} ECPDHShared;

typedef struct ECPDHRKeyBucket {
    ECPDHRKey key[ECP_MAX_NODE_KEY];
    unsigned char key_curr;
    unsigned char key_idx_map[ECP_MAX_SOCK_KEY];
} ECPDHRKeyBucket;

typedef struct ECPNode {
    ECPNetAddr addr;
    ecp_dh_public_t public;
} ECPNode;

typedef struct ECPSeqItem {
    ecp_seq_t seq;
    unsigned char seq_w;
#ifdef ECP_WITH_RBUF
    unsigned char rb_pass;
    unsigned char rb_mtype;
    unsigned int rb_idx;
#endif
} ECPSeqItem;

typedef struct ECPFragIter {
    ecp_seq_t seq;
    unsigned char frag_cnt;
    unsigned char *buffer;
    size_t buf_size;
    size_t content_size;
} ECPFragIter;

typedef struct ECPPktMeta {
    ecp_aead_key_t shsec;
    ecp_dh_public_t public;
    ecp_seq_t seq;
    unsigned char nonce[ECP_AEAD_SIZE_NONCE];
    unsigned char s_idx;
    unsigned char c_idx;
} ECPPktMeta;

typedef struct ECPConnHandler {
    ecp_conn_handler_msg_t msg[ECP_MAX_MTYPE];
    ecp_conn_create_t conn_create;
    ecp_conn_destroy_t conn_destroy;
    ecp_conn_open_t conn_open;
    ecp_conn_close_t conn_close;
} ECPConnHandler;

typedef struct ECPSockCTable {
#ifdef ECP_WITH_HTABLE
    void *htable;
#else
    struct ECPConnection *array[ECP_MAX_SOCK_CONN];
    unsigned short size;
#endif
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_t mutex;
#endif
} ECPSockCTable;

typedef struct ECPContext {
    ecp_rng_t rng;
    ecp_conn_alloc_t conn_alloc;
    ecp_conn_free_t conn_free;
    ECPConnHandler *handler[ECP_MAX_CTYPE];
#ifdef ECP_WITH_DIRSRV
    struct ECPDirList *dir_online;
    struct ECPDirList *dir_shadow;
#endif
} ECPContext;

typedef struct ECPSocket {
    ECPContext *ctx;
    unsigned char running;
    int poll_timeout;
    ECPNetSock sock;
    ECPDHKey key_perma;
    ECPDHKey key[ECP_MAX_SOCK_KEY];
    unsigned char key_curr;
    ECPSockCTable conn;
    ECPTimer timer;
    ecp_sock_handler_msg_t handler[ECP_MAX_MTYPE_SOCK];
#ifdef ECP_WITH_PTHREAD
    pthread_t rcvr_thd;
    pthread_mutex_t mutex;
#endif
} ECPSocket;

typedef struct ECPConnection {
    unsigned char type;
    unsigned char out;
    unsigned char flags;
    unsigned short refcount;
    ecp_seq_t seq_out;
    ecp_seq_t seq_in;
    ecp_ack_t seq_in_map;
    ECPSocket *sock;
    ECPNode node;
    ECPDHRKeyBucket remote;
    ECPDHKey key[ECP_MAX_CONN_KEY];
    unsigned char key_curr;
    unsigned char key_idx[ECP_MAX_NODE_KEY];
    unsigned char key_idx_curr;
    unsigned char key_idx_map[ECP_MAX_SOCK_KEY];
    ECPDHShared shared[ECP_MAX_NODE_KEY][ECP_MAX_NODE_KEY];
    unsigned char nonce[ECP_AEAD_SIZE_NONCE];
#ifdef ECP_WITH_RBUF
    ECPConnRBuffer rbuf;
#endif
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_t mutex;
#endif
    struct ECPConnection *parent;
    unsigned short pcount;
    void *conn_data;
} ECPConnection;

int ecp_init(ECPContext *ctx);
int ecp_ctx_init(ECPContext *ctx);
int ecp_node_init(ECPNode *node, ecp_dh_public_t *public, void *addr);
int ecp_seq_item_init(ECPSeqItem *seq_item);
int ecp_frag_iter_init(ECPFragIter *iter, unsigned char *buffer, size_t buf_size);
void ecp_frag_iter_reset(ECPFragIter *iter);
int ecp_dhkey_gen(ECPContext *ctx, ECPDHKey *key);

int ecp_sock_create(ECPSocket *sock, ECPContext *ctx, ECPDHKey *key);
void ecp_sock_destroy(ECPSocket *sock);
int ecp_sock_open(ECPSocket *sock, void *myaddr);
void ecp_sock_close(ECPSocket *sock);
int ecp_sock_dhkey_get_curr(ECPSocket *sock, unsigned char *idx, unsigned char *public);
int ecp_sock_dhkey_new(ECPSocket *sock);

int ecp_conn_create(ECPConnection *conn, ECPSocket *sock, unsigned char ctype);
void ecp_conn_destroy(ECPConnection *conn);
int ecp_conn_register(ECPConnection *conn);
void ecp_conn_unregister(ECPConnection *conn);

int ecp_conn_set_remote(ECPConnection *conn, ECPNode *node);
int ecp_conn_get_dirlist(ECPConnection *conn, ECPNode *node);
int ecp_conn_open(ECPConnection *conn, ECPNode *node);
int ecp_conn_close(ECPConnection *conn, ecp_cts_t timeout);
int ecp_conn_reset(ECPConnection *conn);
int ecp_conn_handler_init(ECPConnHandler *handler);

int ecp_conn_dhkey_new(ECPConnection *conn);
int ecp_conn_dhkey_new_pub(ECPConnection *conn, unsigned char idx, unsigned char *public);
int ecp_conn_dhkey_get_curr(ECPConnection *conn, unsigned char *idx, unsigned char *public);

ssize_t ecp_conn_send_open(ECPConnection *conn);
ssize_t ecp_conn_send_kget(ECPConnection *conn);
ssize_t ecp_conn_send_kput(ECPConnection *conn);
ssize_t ecp_conn_send_dir(ECPConnection *conn);

ssize_t ecp_conn_handle_open(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b);
ssize_t ecp_conn_handle_kget(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b);
ssize_t ecp_conn_handle_kput(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b);
ssize_t ecp_conn_handle_exec(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b);
ssize_t ecp_conn_handle_msg(ECPConnection *conn, ecp_seq_t seq, unsigned char *msg, size_t msg_size, ECP2Buffer *bufs);
int ecp_sock_handle_open(ECPSocket *sock, ECPNetAddr *addr, ECPConnection *parent, unsigned char *msg, size_t msg_size, ECPPktMeta *pkt_meta, ECP2Buffer *bufs, ECPConnection **_conn);
int ecp_sock_handle_kget(ECPSocket *sock, ECPNetAddr *addr, ECPConnection *parent, unsigned char *msg, size_t msg_size, ECPPktMeta *pkt_meta, ECP2Buffer *bufs, ECPConnection **_conn);

ssize_t _ecp_pack(ECPContext *ctx, unsigned char *packet, size_t pkt_size, ECPPktMeta *pkt_meta, unsigned char *payload, size_t pld_size);
ssize_t ecp_pack(ECPContext *ctx, ECPConnection *parent, ECPBuffer *packet, ECPPktMeta *pkt_meta, ECPBuffer *payload, size_t pld_size, ECPNetAddr *addr) ;

ssize_t _ecp_pack_conn(ECPConnection *conn, unsigned char *packet, size_t pkt_size, unsigned char s_idx, unsigned char c_idx, ECPSeqItem *si, unsigned char *payload, size_t pld_size, ECPNetAddr *addr);
ssize_t ecp_pack_conn(ECPConnection *conn, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, ECPSeqItem *si, ECPBuffer *payload, size_t pld_size, ECPNetAddr *addr);

ssize_t ecp_unpack(ECPSocket *sock, ECPNetAddr *addr, ECPConnection *parent, ECP2Buffer *bufs, size_t pkt_size, ECPConnection **_conn, ecp_seq_t *_seq);
ssize_t ecp_pkt_handle(ECPSocket *sock, ECPNetAddr *addr, ECPConnection *parent, ECP2Buffer *bufs, size_t pkt_size);
ssize_t ecp_pkt_send(ECPSocket *sock, ECPNetAddr *addr, ECPBuffer *packet, size_t pkt_size, unsigned char flags);

int ecp_msg_get_type(unsigned char *msg, size_t msg_size, unsigned char *mtype);
int ecp_msg_set_type(unsigned char *msg, size_t msg_size, unsigned char mtype);
int ecp_msg_get_frag(unsigned char *msg, size_t msg_size, unsigned char *frag_cnt, unsigned char *frag_tot, uint16_t *frag_size);
int ecp_msg_set_frag(unsigned char *msg, size_t msg_size, unsigned char frag_cnt, unsigned char frag_tot, uint16_t frag_size);
int ecp_msg_get_pts(unsigned char *msg, size_t msg_size, ecp_pts_t *pts);
int ecp_msg_set_pts(unsigned char *msg, size_t msg_size, ecp_pts_t pts);
unsigned char *ecp_msg_get_content(unsigned char *msg, size_t msg_size);
int ecp_msg_defrag(ECPFragIter *iter, ecp_seq_t seq, unsigned char mtype, unsigned char *msg_in, size_t msg_in_size, unsigned char **msg_out, size_t *msg_out_size);

int ecp_pld_get_type(unsigned char *payload, size_t pld_size, unsigned char *mtype);
int ecp_pld_set_type(unsigned char *payload, size_t pld_size, unsigned char mtype);
int ecp_pld_get_frag(unsigned char *payload, size_t pld_size, unsigned char *frag_cnt, unsigned char *frag_tot, uint16_t *frag_size);
int ecp_pld_set_frag(unsigned char *payload, size_t pld_size, unsigned char frag_cnt, unsigned char frag_tot, uint16_t frag_size);
int ecp_pld_get_pts(unsigned char *payload, size_t pld_size, ecp_pts_t *pts);
int ecp_pld_set_pts(unsigned char *payload, size_t pld_size, ecp_pts_t pts);
unsigned char *ecp_pld_get_buf(unsigned char *payload, size_t pld_size);

ssize_t _ecp_pld_send(ECPConnection *conn, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, ECPSeqItem *si, ECPBuffer *payload, size_t pld_size, unsigned char flags, ECPTimerItem *ti);
ssize_t ecp_pld_send(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pld_size, unsigned char flags);
ssize_t ecp_pld_send_wtimer(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pld_size, unsigned char flags, ECPTimerItem *ti);
ssize_t ecp_pld_send_tr(ECPSocket *sock, ECPNetAddr *addr, ECPConnection *parent, ECPBuffer *packet, ECPPktMeta *pkt_meta, ECPBuffer *payload, size_t pld_size, unsigned char flags);

ssize_t ecp_send(ECPConnection *conn, unsigned char mtype, unsigned char *content, size_t content_size);
ssize_t ecp_receive(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, ecp_cts_t timeout);

int ecp_receiver(ECPSocket *sock);
int ecp_start_receiver(ECPSocket *sock);
int ecp_stop_receiver(ECPSocket *sock);
