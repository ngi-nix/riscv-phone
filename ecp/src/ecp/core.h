#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>

#ifdef ECP_WITH_PTHREAD
#include <pthread.h>
#endif

#define ECP_OK                      0
#define ECP_PASS                    1

#define ECP_ERR                     -1
#define ECP_ERR_TIMEOUT             -2
#define ECP_ERR_ALLOC               -3
#define ECP_ERR_SIZE                -4
#define ECP_ERR_ITER                -5
#define ECP_ERR_BUSY                -6
#define ECP_ERR_EMPTY               -7
#define ECP_ERR_FULL                -8
#define ECP_ERR_MTYPE               -9
#define ECP_ERR_CTYPE               -10
#define ECP_ERR_HANDLER             -11
#define ECP_ERR_COOKIE              -12

#define ECP_ERR_NET_ADDR            -13
#define ECP_ERR_MAX_PARENT          -14
#define ECP_ERR_NEXT                -15

#define ECP_ERR_ECDH_KEY_DUP        -21
#define ECP_ERR_ECDH_IDX            -22
#define ECP_ERR_ENCRYPT             -26
#define ECP_ERR_DECRYPT             -27
#define ECP_ERR_SIGN                -28
#define ECP_ERR_VERIFY              -29
#define ECP_ERR_SEND                -30
#define ECP_ERR_RECV                -31
#define ECP_ERR_SEQ                 -32
#define ECP_ERR_VBOX                -33
#define ECP_ERR_CLOSED              -34

#define ECP_MAX_SOCK_CONN           4
#define ECP_MAX_SOCK_KEY            2
#define ECP_MAX_CONN_KEY            2
#define ECP_MAX_NODE_KEY            2
#define ECP_MAX_CTYPE               8
#define ECP_MAX_MTYPE               16
#define ECP_MAX_PARENT              3
#define ECP_MAX_SEQ_FWD             1024

#define ECP_SIZE_PROTO              2
#define ECP_SIZE_NONCE              8
#define ECP_SIZE_MTYPE              1
#define ECP_SIZE_COOKIE             (ECP_SIZE_ECDH_PUB)

#define ECP_SIZE_PKT_HDR            (ECP_SIZE_PROTO+1+ECP_SIZE_ECDH_PUB+ECP_SIZE_NONCE)

#define ECP_MAX_PKT                 1412
#define ECP_MAX_PLD                 (ECP_MAX_PKT-ECP_SIZE_PKT_HDR-ECP_SIZE_AEAD_TAG)
#define ECP_MAX_MSG                 (ECP_MAX_PLD-ECP_SIZE_MTYPE)

#define ECP_MIN_PKT                 (ECP_SIZE_PKT_HDR+ECP_SIZE_MTYPE+ECP_SIZE_AEAD_TAG)
#define ECP_MIN_PLD                 (ECP_SIZE_MTYPE)

#define ECP_SIZE_VBOX                   (ECP_SIZE_ECDH_PUB+ECP_SIZE_NONCE+ECP_SIZE_ECDH_PUB+ECP_SIZE_AEAD_TAG)

#define ECP_SIZE_MT_FRAG(T)             ((T) & ECP_MTYPE_FLAG_FRAG ? 2 + sizeof(uint16_t) : 0)
#define ECP_SIZE_MT_PTS(T)              ((T) & ECP_MTYPE_FLAG_PTS ? sizeof(ecp_pts_t) : 0)
#define ECP_SIZE_MT_FLAG(T)             (ECP_SIZE_MT_FRAG(T)+ECP_SIZE_MT_PTS(T))
#define ECP_SIZE_PLD(X,T)               ((X)+ECP_SIZE_MTYPE+ECP_SIZE_MT_FLAG(T))
#define ECP_SIZE_PKT(X,T)               (ECP_SIZE_PKT_HDR+ECP_SIZE_PLD(X,T)+ECP_SIZE_AEAD_TAG)

#ifdef ECP_WITH_VCONN
#define ECP_SIZE_PLD_BUF(X,T,C)         (ECP_SIZE_PLD(X,T)+(C)->pcount*(ECP_SIZE_PKT_HDR+ECP_SIZE_MTYPE+ECP_SIZE_AEAD_TAG))
#else
#define ECP_SIZE_PLD_BUF(X,T,C)         (ECP_SIZE_PLD(X,T))
#endif
#define ECP_SIZE_PKT_BUF(X,T,C)         (ECP_SIZE_PKT_HDR+ECP_SIZE_PLD_BUF(X,T,C)+ECP_SIZE_AEAD_TAG)

#ifdef ECP_WITH_VCONN
#define ECP_SIZE_PLD_BUF_IREP(X,T,P)    (ECP_SIZE_PLD(X,T)+((P) ? ((P)->pcount+1)*(ECP_SIZE_PKT_HDR+ECP_SIZE_MTYPE+ECP_SIZE_AEAD_TAG) : 0))
#else
#define ECP_SIZE_PLD_BUF_IREP(X,T,P)    (ECP_SIZE_PLD(X,T))
#endif
#define ECP_SIZE_PKT_BUF_IREP(X,T,P)    (ECP_SIZE_PKT_HDR+ECP_SIZE_PLD_BUF_IREP(X,T,P)+ECP_SIZE_AEAD_TAG)

#ifdef ECP_WITH_VCONN
#define ecp_conn_is_root(C)         ((C)->parent == NULL)
#else
#define ecp_conn_is_root(C)         1
#endif

#define ECP_SEND_TRIES              3
#define ECP_SEND_TIMEOUT            500
#define ECP_POLL_TIMEOUT            500

#define ECP_ECDH_IDX_INV            0xFF
#define ECP_ECDH_IDX_PERMA          0x0F
#define ECP_ECDH_IDX_MASK           0x07

#define ECP_NTYPE_INB               1
#define ECP_NTYPE_OUTB              2
#define ECP_NTYPE_VBOX              3

#define ECP_MTYPE_FLAG_SYS          0x80
#define ECP_MTYPE_FLAG_FRAG         0x40
#define ECP_MTYPE_FLAG_PTS          0x20
#define ECP_MTYPE_MASK              0x1F

#define ECP_MTYPE_INIT_REQ          (0x00 | ECP_MTYPE_FLAG_SYS)
#define ECP_MTYPE_INIT_REP          (0x01 | ECP_MTYPE_FLAG_SYS)
#define ECP_MTYPE_OPEN_REQ          (0x02 | ECP_MTYPE_FLAG_SYS)
#define ECP_MTYPE_OPEN_REP          (0x03 | ECP_MTYPE_FLAG_SYS)
#define ECP_MTYPE_KEYX_REQ          (0x04 | ECP_MTYPE_FLAG_SYS)
#define ECP_MTYPE_KEYX_REP          (0x05 | ECP_MTYPE_FLAG_SYS)

#define ECP_CTYPE_FLAG_SYS          0x80
#define ECP_CTYPE_MASK              0x7F

/* immutable flags */
#define ECP_CONN_FLAG_INB           0x80
#define ECP_CONN_FLAG_VBOX          0x01
#define ECP_CONN_FLAG_RBUF          0x02

#define ECP_CONN_FLAG_MASK          0x7F

/* mutable flags */
#define ECP_CONN_FLAG_REG           0x04
#define ECP_CONN_FLAG_OPEN          0x08

#define ECP_SEND_FLAG_REPLY         0x01
#define ECP_SEND_FLAG_MORE          0x02

#define ecp_conn_has_vbox(conn)     ((conn)->flags_im & ECP_CONN_FLAG_VBOX)
#define ecp_conn_has_rbuf(conn)     ((conn)->flags_im & ECP_CONN_FLAG_RBUF)
#define ecp_conn_is_inb(conn)       ((conn)->flags_im & ECP_CONN_FLAG_INB)
#define ecp_conn_is_outb(conn)    (!((conn)->flags_im & ECP_CONN_FLAG_INB))
#define ecp_conn_is_reg(conn)       ((conn)->flags    & ECP_CONN_FLAG_REG)
#define ecp_conn_is_open(conn)      ((conn)->flags    & ECP_CONN_FLAG_OPEN)
#define ecp_conn_is_sys(conn)       ((conn)->type     & ECP_CTYPE_FLAG_SYS)

#define ecp_conn_set_inb(conn)      ((conn)->flags_im |=  ECP_CONN_FLAG_INB)
#define ecp_conn_set_outb(conn)     ((conn)->flags_im &= ~ECP_CONN_FLAG_INB)
#define ecp_conn_set_reg(conn)      ((conn)->flags    |=  ECP_CONN_FLAG_REG)
#define ecp_conn_set_open(conn)     ((conn)->flags    |=  ECP_CONN_FLAG_OPEN)

#define ecp_conn_clr_reg(conn)      ((conn)->flags    &= ~ECP_CONN_FLAG_REG)
#define ecp_conn_clr_open(conn)     ((conn)->flags    &= ~ECP_CONN_FLAG_OPEN)

typedef uint32_t ecp_ack_t;
#define ECP_SIZE_ACKB               (sizeof(ecp_ack_t)*8)
#define ECP_ACK_FULL                (~((ecp_ack_t)0))

typedef uint32_t ecp_sts_t;
#define ECP_STS_HALF                ((ecp_sts_t)1 << (sizeof(ecp_sts_t) * 8 - 1))
#define ECP_STS_LT(a,b)             ((ecp_sts_t)((ecp_sts_t)(a) - (ecp_sts_t)(b)) > ECP_STS_HALF)
#define ECP_STS_LTE(a,b)            ((ecp_sts_t)((ecp_sts_t)(b) - (ecp_sts_t)(a)) < ECP_STS_HALF)

typedef uint32_t ecp_pts_t;
#define ECP_PTS_HALF                ((ecp_pts_t)1 << (sizeof(ecp_pts_t) * 8 - 1))
#define ECP_PTS_LT(a,b)             ((ecp_pts_t)((ecp_pts_t)(a) - (ecp_pts_t)(b)) > ECP_PTS_HALF)
#define ECP_PTS_LTE(a,b)            ((ecp_pts_t)((ecp_pts_t)(b) - (ecp_pts_t)(a)) < ECP_PTS_HALF)

typedef uint32_t ecp_seq_t;
#define ECP_SEQ_HALF                ((ecp_seq_t)1 << (sizeof(ecp_seq_t) * 8 - 1))
#define ECP_SEQ_LT(a,b)             ((ecp_seq_t)((ecp_seq_t)(a) - (ecp_seq_t)(b)) > ECP_SEQ_HALF)
#define ECP_SEQ_LTE(a,b)            ((ecp_seq_t)((ecp_seq_t)(b) - (ecp_seq_t)(a)) < ECP_SEQ_HALF)

typedef uint64_t ecp_nonce_t;
#define ECP_NONCE_HALF              ((ecp_nonce_t)1 << (sizeof(ecp_nonce_t) * 8 - 1))
#define ECP_NONCE_LT(a,b)           ((ecp_nonce_t)((ecp_nonce_t)(a) - (ecp_nonce_t)(b)) > ECP_NONCE_HALF)
#define ECP_NONCE_LTE(a,b)          ((ecp_nonce_t)((ecp_nonce_t)(b) - (ecp_nonce_t)(a)) < ECP_NONCE_HALF)

struct ECP2Buffer;
struct ECPSocket;
struct ECPConnection;

#ifdef ECP_WITH_DIRSRV
struct ECPDirList;
#endif

#include "crypto/crypto.h"
#include "transport.h"
#include "timer.h"

typedef void (*ecp_err_handler_t) (struct ECPConnection *conn, unsigned char mtype, int err);
typedef ssize_t (*ecp_dir_handler_t) (struct ECPConnection *conn, unsigned char *msg, size_t msg_size, struct ECP2Buffer *b);
typedef struct ECPConnection * (*ecp_conn_alloc_t) (struct ECPSocket *sock, unsigned char type);
typedef void (*ecp_conn_free_t) (struct ECPConnection *conn);

typedef ssize_t (*ecp_msg_handler_t) (struct ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, size_t msg_size, struct ECP2Buffer *b);
typedef int (*ecp_open_handler_t) (struct ECPConnection *conn, struct ECP2Buffer *b);
typedef void (*ecp_close_handler_t) (struct ECPConnection *conn);
typedef ssize_t (*ecp_open_send_t) (struct ECPConnection *conn, unsigned char *cookie);

typedef struct ECPBuffer {
    unsigned char *buffer;
    size_t size;
} ECPBuffer;

typedef struct ECP2Buffer {
    ECPBuffer *packet;
    ECPBuffer *payload;
} ECP2Buffer;

typedef struct ECPDHKey {
    ecp_ecdh_public_t public;
    ecp_ecdh_private_t private;
    unsigned char valid;
} ECPDHKey;

typedef struct ECPDHPub {
    ecp_ecdh_public_t public;
    unsigned char valid;
} ECPDHPub;

typedef struct ECPNode {
    ecp_tr_addr_t addr;
    ECPDHPub key_perma;
} ECPNode;

typedef struct ECPDHShkey {
    ecp_aead_key_t key;
    unsigned char valid;
} ECPDHShkey;

typedef struct ECPPktMeta {
    unsigned char *cookie;
    ecp_ecdh_public_t *public;
    ecp_aead_key_t *shkey;
    ecp_nonce_t *nonce;
    unsigned char ntype;
    unsigned char s_idx;
    unsigned char c_idx;
} ECPPktMeta;

typedef struct ECPConnHandler {
    ecp_msg_handler_t handle_msg;
    ecp_open_handler_t handle_open;
    ecp_close_handler_t handle_close;
    ecp_open_send_t send_open;
} ECPConnHandler;

typedef struct ECPContext {
    ecp_err_handler_t handle_err;
    ecp_dir_handler_t handle_dir;
    ecp_conn_alloc_t conn_alloc;
    ecp_conn_free_t conn_free;
    ECPConnHandler *handler[ECP_MAX_CTYPE];
#ifdef ECP_WITH_DIRSRV
    struct ECPDirSrv *dir_srv;
#endif
} ECPContext;

typedef struct ECPConnTable {
#ifdef ECP_WITH_HTABLE
    void *keys;
    void *addrs;
#else
    struct ECPConnection *arr[ECP_MAX_SOCK_CONN];
    unsigned short size;
#endif
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_t mutex;
#endif
} ECPConnTable;

typedef struct ECPSocket {
    ECPContext *ctx;
    unsigned char running;
    ecp_tr_sock_t sock;
    ecp_nonce_t nonce_out;
    ECPDHKey key_perma;
    ECPDHKey key[ECP_MAX_SOCK_KEY];
    unsigned char key_curr;
    ecp_bc_ctx_t minkey;
    ECPConnTable conn_table;
    ECPTimer timer;
#ifdef ECP_WITH_PTHREAD
    pthread_t rcvr_thd;
    pthread_mutex_t mutex;
#endif
} ECPSocket;

typedef struct ECPConnection {
    unsigned char type;
    unsigned char flags;
    unsigned char flags_im;
    unsigned short refcount;
    ecp_nonce_t nonce_out;
    ecp_nonce_t nonce_in;
    ecp_ack_t nonce_map;
    ECPSocket *sock;
    ECPNode remote;
    ECPDHKey key[ECP_MAX_CONN_KEY];
    ECPDHPub rkey[ECP_MAX_NODE_KEY];
    unsigned char key_curr;
    unsigned char key_next;
    unsigned char rkey_curr;
    ECPDHShkey shkey[ECP_MAX_NODE_KEY][ECP_MAX_NODE_KEY];
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_t mutex;
#endif
#ifdef ECP_WITH_VCONN
    struct ECPConnection *parent;
    struct ECPConnection *next;
    unsigned short pcount;
#endif
} ECPConnection;

int ecp_dhkey_gen(ECPDHKey *key);

int ecp_init(ECPContext *ctx);
int ecp_ctx_init(ECPContext *ctx, ecp_err_handler_t handle_err, ecp_dir_handler_t handle_dir, ecp_conn_alloc_t conn_alloc, ecp_conn_free_t conn_free);
int ecp_ctx_set_handler(ECPContext *ctx, ECPConnHandler *handler, unsigned char ctype);

int ecp_node_init(ECPNode *node, ecp_ecdh_public_t *public, void *addr);
void ecp_node_set_pub(ECPNode *node, ecp_ecdh_public_t *public);
int ecp_node_set_addr(ECPNode *node, void *addr);

int ecp_sock_init(ECPSocket *sock, ECPContext *ctx, ECPDHKey *key);
int ecp_sock_create(ECPSocket *sock, ECPContext *ctx, ECPDHKey *key);
void ecp_sock_destroy(ECPSocket *sock);
int ecp_sock_open(ECPSocket *sock, void *myaddr);
void ecp_sock_close(ECPSocket *sock);
int ecp_sock_minkey_new(ECPSocket *sock);
int ecp_sock_dhkey_new(ECPSocket *sock);
int ecp_sock_dhkey_get(ECPSocket *sock, unsigned char idx, ECPDHKey *key);
int ecp_sock_dhkey_get_pub(ECPSocket *sock, unsigned char *idx, ecp_ecdh_public_t *public);
void ecp_sock_get_nonce(ECPSocket *sock, ecp_nonce_t *nonce);

int ecp_cookie_gen(ECPSocket *sock, unsigned char *cookie, unsigned char *public_buf);
int ecp_cookie_verify(ECPSocket *sock, unsigned char *cookie, unsigned char *public_buf);

int ecp_conn_alloc(ECPSocket *sock, unsigned char ctype, ECPConnection **_conn);
void ecp_conn_free(ECPConnection *conn);

void ecp_conn_init(ECPConnection *conn, ECPSocket *sock, unsigned char ctype);
void ecp_conn_reinit(ECPConnection *conn);
int ecp_conn_create(ECPConnection *conn, ECPSocket *sock, unsigned char ctype);
int ecp_conn_create_inb(ECPConnection *conn, ECPSocket *sock, unsigned char ctype);
void ecp_conn_destroy(ECPConnection *conn);

void ecp_conn_set_flags(ECPConnection *conn, unsigned char flags);
void ecp_conn_clr_flags(ECPConnection *conn, unsigned char flags);
void ecp_conn_set_remote_key(ECPConnection *conn, ECPDHPub *key);
void ecp_conn_set_remote_addr(ECPConnection *conn, ecp_tr_addr_t *addr);

int ecp_conn_init_inb(ECPConnection *conn, ECPConnection *parent, unsigned char s_idx, unsigned char c_idx, ecp_ecdh_public_t *public, ECPDHPub *remote_key, ecp_aead_key_t *shkey);
int ecp_conn_init_outb(ECPConnection *conn, ECPNode *node);

int ecp_conn_insert(ECPConnection *conn);
void ecp_conn_remove(ECPConnection *conn, unsigned short *refcount);
void ecp_conn_remove_addr(ECPConnection *conn);

int ecp_conn_open(ECPConnection *conn, ECPNode *node);
int ecp_conn_reset(ECPConnection *conn);
void _ecp_conn_close(ECPConnection *conn);
int ecp_conn_close(ECPConnection *conn);
void ecp_conn_refcount_inc(ECPConnection *conn);
void ecp_conn_refcount_dec(ECPConnection *conn);

int ecp_conn_dhkey_new(ECPConnection *conn);
int ecp_conn_dhkey_get(ECPConnection *conn, unsigned char idx, ECPDHKey *key);
int ecp_conn_dhkey_get_remote(ECPConnection *conn, unsigned char idx, ECPDHPub *key);
int ecp_conn_dhkey_get_pub(ECPConnection *conn, unsigned char *idx, ecp_ecdh_public_t *public, int will_send);
int ecp_conn_dhkey_set_pub(ECPConnection *conn, unsigned char idx, ecp_ecdh_public_t *public);
void ecp_conn_dhkey_set_curr(ECPConnection *conn);

void ecp_conn_handler_init(ECPConnHandler *handler, ecp_msg_handler_t handle_msg, ecp_open_handler_t handle_open, ecp_close_handler_t handle_close, ecp_open_send_t send_open);
ecp_msg_handler_t ecp_get_msg_handler(ECPConnection *conn);
ecp_open_handler_t ecp_get_open_handler(ECPConnection *conn);
ecp_close_handler_t ecp_get_close_handler(ECPConnection *conn);
ecp_dir_handler_t ecp_get_dir_handler(ECPConnection *conn);
void ecp_err_handle(ECPConnection *conn, unsigned char mtype, int err);

ssize_t ecp_send_init_req(ECPConnection *conn);
ssize_t ecp_handle_init_req(ECPSocket *sock, ECPConnection *parent, ecp_tr_addr_t *addr, unsigned char c_idx, unsigned char *public_buf, ecp_aead_key_t *shkey);
ssize_t ecp_send_init_rep(ECPSocket *sock, ECPConnection *parent, ecp_tr_addr_t *addr, unsigned char c_idx, unsigned char *public_buf, ecp_aead_key_t *shkey);
ssize_t ecp_handle_init_rep(ECPConnection *conn, unsigned char *msg, size_t msg_size);
ssize_t ecp_write_open_req(ECPConnection *conn, ECPBuffer *payload);
ssize_t ecp_send_open_req(ECPConnection *conn, unsigned char *cookie);
ssize_t ecp_handle_open_req(ECPSocket *sock, ECPConnection *parent, unsigned char s_idx, unsigned char c_idx, unsigned char *public_buf, unsigned char *msg, size_t msg_size, ecp_aead_key_t *shkey, ECPConnection **_conn);
ssize_t ecp_send_open_rep(ECPConnection *conn);
ssize_t ecp_handle_open(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *bufs);

ssize_t ecp_send_keyx_req(ECPConnection *conn);
ssize_t ecp_send_keyx_rep(ECPConnection *conn);
ssize_t ecp_handle_keyx(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *bufs);

ssize_t ecp_msg_handle(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *bufs);
ssize_t ecp_pld_handle_one(ECPConnection *conn, ecp_seq_t seq, unsigned char *payload, size_t pld_size, ECP2Buffer *bufs);
ssize_t ecp_pld_handle(ECPConnection *conn, ecp_seq_t seq, unsigned char *payload, size_t _pld_size, ECP2Buffer *bufs);

ssize_t ecp_unpack(ECPSocket *sock, ECPConnection *parent, ecp_tr_addr_t *addr, ECP2Buffer *bufs, size_t _pkt_size, ECPConnection **_conn, unsigned char **_payload, ecp_seq_t *_seq);
ssize_t ecp_pkt_handle(ECPSocket *sock, ECPConnection *parent, ecp_tr_addr_t *addr, ECP2Buffer *bufs, size_t pkt_size);

ssize_t ecp_pkt_send(ECPSocket *sock, ECPBuffer *packet, size_t pkt_size, unsigned char flags, ECPTimerItem *ti, ecp_tr_addr_t *addr);

void ecp_nonce2buf(unsigned char *b, ecp_nonce_t *n);
void ecp_buf2nonce(ecp_nonce_t *n, unsigned char *b);
int ecp_pkt_get_seq(unsigned char *pkt, size_t pkt_size, ecp_seq_t *s);
ssize_t ecp_pack_irep(ECPConnection *parent, ECPBuffer *packet, ECPPktMeta *pkt_meta, ECPBuffer *payload, size_t pld_size, ecp_tr_addr_t *addr);
ssize_t ecp_pack_conn(ECPConnection *conn, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, unsigned char *cookie, ecp_nonce_t *nonce, ECPBuffer *payload, size_t pld_size, ecp_tr_addr_t *addr);

int ecp_pld_get_type(unsigned char *pld, size_t pld_size, unsigned char *mtype);
int ecp_pld_set_type(unsigned char *pld, size_t pld_size, unsigned char mtype);
int ecp_pld_get_frag(unsigned char *pld, size_t pld_size, unsigned char *frag_cnt, unsigned char *frag_tot, uint16_t *frag_size);
int ecp_pld_set_frag(unsigned char *pld, size_t pld_size, unsigned char frag_cnt, unsigned char frag_tot, uint16_t frag_size);
int ecp_pld_get_pts(unsigned char *pld, size_t pld_size, ecp_pts_t *pts);
int ecp_pld_set_pts(unsigned char *pld, size_t pld_size, ecp_pts_t pts);
unsigned char *ecp_pld_get_msg(unsigned char *pld, size_t pld_size);

ssize_t _ecp_pld_send(ECPConnection *conn, ECPBuffer *packet, unsigned char s_idx, unsigned char c_idx, unsigned char *cookie, ecp_nonce_t *n, ECPBuffer *payload, size_t pld_size, unsigned char flags, ECPTimerItem *ti);
ssize_t ecp_pld_send(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pld_size, unsigned char flags);
ssize_t ecp_pld_send_wtimer(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pld_size, unsigned char flags, ECPTimerItem *ti);
ssize_t ecp_pld_send_wcookie(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pld_size, unsigned char flags, unsigned char *cookie);
ssize_t ecp_pld_send_wnonce(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pld_size, unsigned char flags, ecp_nonce_t *nonce);
ssize_t ecp_pld_send_irep(ECPSocket *sock, ECPConnection *parent, ecp_tr_addr_t *addr, ECPBuffer *packet, ECPPktMeta *pkt_meta, ECPBuffer *payload, size_t pld_size, unsigned char flags);

ssize_t ecp_msg_send(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size);
int ecp_receiver(ECPSocket *sock);
int ecp_start_receiver(ECPSocket *sock);
int ecp_stop_receiver(ECPSocket *sock);
