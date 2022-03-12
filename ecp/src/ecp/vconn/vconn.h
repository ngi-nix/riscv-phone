#define ECP_CTYPE_VCONN     (0x01 | ECP_CTYPE_FLAG_SYS)
#define ECP_CTYPE_VLINK     (0x02 | ECP_CTYPE_FLAG_SYS)

#define ECP_MTYPE_NEXT      0x00
#define ECP_MTYPE_EXEC      0x02
#define ECP_MTYPE_RELAY     0x01

/* inbound only */
typedef struct ECPVConn {
    ECPConnection b;
    ECPDHPub key_next[ECP_MAX_NODE_KEY];
    unsigned char key_next_curr;
} ECPVConn;

ssize_t ecp_vconn_send_open_req(ECPConnection *conn, unsigned char *cookie);
ssize_t ecp_vconn_pack_parent(ECPConnection *conn, ECPBuffer *packet, ECPBuffer *payload, size_t pkt_size, ecp_tr_addr_t *addr);
int ecp_vconn_handle_open(ECPConnection *conn, ECP2Buffer *b);
void ecp_vconn_handle_close(ECPConnection *conn);
ssize_t ecp_vconn_handle_msg(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *b);

int ecp_vconn_create_inb(ECPVConn *conn, ECPSocket *sock);
void ecp_vconn_destroy_inb(ECPVConn *conn);

int ecp_vconn_create(ECPConnection vconn[], ecp_ecdh_public_t keys[], size_t vconn_size, ECPConnection *conn);
int ecp_vconn_open(ECPConnection *conn, ECPNode *node);
