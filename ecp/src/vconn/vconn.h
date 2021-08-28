#define ECP_CTYPE_VCONN     1
#define ECP_CTYPE_VLINK     2

#define ECP_MTYPE_RELAY     0x08
#define ECP_MTYPE_EXEC      0x09

typedef struct ECPVConnection {
    ECPConnection b;
    ECPConnection *next;
} ECPVConnection;

typedef struct ECPVConnIn {
    ECPConnection b;
    unsigned char key_next[ECP_MAX_NODE_KEY][ECP_ECDH_SIZE_KEY];
    unsigned char key_next_curr;
    unsigned char key_out[ECP_ECDH_SIZE_KEY];
} ECPVConnIn;

int ecp_vconn_ctx_init(ECPContext *ctx);
int ecp_vconn_init(ECPConnection *conn, ECPNode *conn_node, ECPVConnection vconn[], ECPNode vconn_node[], int size);
int ecp_vconn_open(ECPConnection *conn, ECPNode *conn_node, ECPVConnection vconn[], ECPNode vconn_node[], int size);
