#ifdef ECP_WITH_DIRSRV

int ecp_dir_init(struct ECPContext *ctx, struct ECPDirList *dir_online, struct ECPDirList *dir_shadow);
ssize_t ecp_dir_handle_update(struct ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, struct ECP2Buffer *b);
int ecp_dir_handle_req(struct ECPSocket *sock, struct ECPNetAddr *addr, struct ECPConnection *parent, unsigned char *msg, size_t msg_size, struct ECPPktMeta *pkt_meta, struct ECP2Buffer *bufs, struct ECPConnection **_conn);

#endif  /* ECP_WITH_DIRSRV */