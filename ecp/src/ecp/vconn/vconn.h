#define ECP_CTYPE_VCONN     1
#define ECP_CTYPE_VLINK     2

#define ECP_MTYPE_RELAY     0x08
#define ECP_MTYPE_EXEC      0x09

typedef struct ECPVConnOut {
    ECPConnection b;
    ECPConnection *next;
} ECPVConnOut;

typedef struct ECPVConnIn {
    ECPConnection b;
    unsigned char key_next[ECP_MAX_NODE_KEY][ECP_ECDH_SIZE_KEY];
    unsigned char key_next_curr;
    unsigned char key_out[ECP_ECDH_SIZE_KEY];
} ECPVConnIn;

ssize_t ecp_vconn_handle_exec(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b);
int ecp_vconn_ctx_init(ECPContext *ctx);
int ecp_vconn_create_parent(ECPConnection *conn, ECPNode *conn_node, ECPVConnOut vconn[], ECPNode vconn_node[], int size);
int ecp_vconn_open(ECPConnection *conn, ECPNode *conn_node, ECPVConnOut vconn[], ECPNode vconn_node[], int size);
