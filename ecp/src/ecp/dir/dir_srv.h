typedef struct ECPDirTable {
    ECPDirList *list;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_t mutex;
#endif
} ECPDirTable;

typedef struct ECPDirSrv {
     ECPDirTable online;
     ECPDirTable shadow;
} ECPDirSrv;

int ecp_dir_create(ECPContext *ctx, ECPDirSrv *dir_srv, ECPDirList *dir_online, ECPDirList *dir_shadow);
void ecp_dir_destroy(ECPContext *ctx);
ssize_t ecp_dir_send_list(ECPConnection *conn, unsigned char mtype, ECPDirTable *list);

ssize_t ecp_dir_send_upd(ECPConnection *conn);
ssize_t ecp_dir_handle_upd(ECPConnection *conn, unsigned char *msg, size_t msg_size);

ssize_t ecp_dir_handle_req(ECPConnection *conn, unsigned char *msg, size_t msg_size);

ssize_t ecp_dir_send_rep(ECPConnection *conn);
ssize_t ecp_dir_handle_rep(ECPConnection *conn, unsigned char *msg, size_t msg_size);
