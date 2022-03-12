int ecp_dir_init(ECPContext *ctx, ECPDirList *dir_online, ECPDirList *dir_shadow);
ssize_t ecp_dir_send_list(ECPConnection *conn, unsigned char mtype, ECPDirList *list);

ssize_t ecp_dir_send_upd(ECPConnection *conn);
ssize_t ecp_dir_handle_upd(ECPConnection *conn, unsigned char *msg, size_t msg_size);

ssize_t ecp_dir_handle_req(ECPConnection *conn, unsigned char *msg, size_t msg_size);

ssize_t ecp_dir_send_rep(ECPConnection *conn);
ssize_t ecp_dir_handle_rep(ECPConnection *conn, unsigned char *msg, size_t msg_size);
