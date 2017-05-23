#define ECP_CTYPE_PROXYF    1
#define ECP_CTYPE_PROXYB    2

#define ECP_MTYPE_RELAY     0x04

typedef struct ECPConnProxy {
    ECPConnection b;
    ECPConnection *next;
} ECPConnProxy;

typedef struct ECPConnProxyF {
    ECPConnection b;
    unsigned char key_next[ECP_MAX_NODE_KEY][ECP_ECDH_SIZE_KEY];
    unsigned char key_next_curr;
    unsigned char key_out[ECP_ECDH_SIZE_KEY];
} ECPConnProxyF;

int ecp_proxy_init(ECPContext *ctx);

int ecp_conn_proxy_init(ECPConnection *conn, ECPNode *conn_node, ECPConnProxy proxy[], ECPNode proxy_node[], int size);
int ecp_conn_proxy_open(ECPConnection *conn, ECPNode *conn_node, ECPConnProxy proxy[], ECPNode proxy_node[], int size);
