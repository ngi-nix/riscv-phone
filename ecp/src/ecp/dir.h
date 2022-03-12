#define ECP_MAX_DIR_ITEM        30
#define ECP_SIZE_DIR_ITEM       40

#define ECP_MTYPE_DIR_UPD       0x00
#define ECP_MTYPE_DIR_REQ       0x01
#define ECP_MTYPE_DIR_REP       0x02

#define ECP_CTYPE_DIR           (0x00 | ECP_CTYPE_FLAG_SYS)

typedef struct ECPDirItem {
    ECPNode node;
    uint16_t capabilities;
} ECPDirItem;

typedef struct ECPDirList {
    ECPDirItem item[ECP_MAX_DIR_ITEM];
    uint16_t count;
} ECPDirList;

ssize_t ecp_dir_parse(ECPDirList *list, unsigned char *buf, size_t buf_size);
ssize_t ecp_dir_serialize(ECPDirList *list, unsigned char *buf, size_t buf_size);

void ecp_dir_item_parse(ECPDirItem *item, unsigned char *buf);
void ecp_dir_item_serialize(ECPDirItem *item, unsigned char *buf);

ssize_t ecp_dir_handle(ECPConnection *conn, unsigned char *msg, size_t msg_size, ECP2Buffer *b);

int ecp_dir_handle_open(ECPConnection *conn, ECP2Buffer *b);
ssize_t ecp_dir_handle_msg(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *b);

ssize_t ecp_dir_send_req(ECPConnection *conn);
int ecp_dir_get(ECPConnection *conn, ECPSocket *sock, ECPNode *node);