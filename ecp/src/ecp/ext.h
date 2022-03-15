int ecp_ext_err_handle(ECPConnection *conn, unsigned char mtype, int err);
int ecp_ext_conn_open(ECPConnection *conn);
void ecp_ext_conn_destroy(ECPConnection *conn);
ssize_t ecp_ext_msg_handle(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *bufs);
ssize_t ecp_ext_pld_store(ECPConnection *conn, ecp_seq_t seq, unsigned char *payload, size_t pld_size, ECP2Buffer *bufs);
ssize_t ecp_ext_pld_handle(ECPConnection *conn, ecp_seq_t seq, unsigned char *payload, size_t pld_size, ECP2Buffer *bufs);
ssize_t ecp_ext_pld_send(ECPConnection *conn, ECPBuffer *payload, size_t pld_size, ECPBuffer *packet, size_t pkt_size, unsigned char flags, ECPTimerItem *ti, ecp_tr_addr_t *addr);
ssize_t ecp_ext_msg_send(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, ECPBuffer *packet, ECPBuffer *payload);
