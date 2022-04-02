#ifdef ECP_WITH_RBUF

int ecp_ext_err_handle(ECPConnection *conn, unsigned char mtype, int err);
int ecp_ext_conn_open(ECPConnection *conn);
void ecp_ext_conn_destroy(ECPConnection *conn);
ssize_t ecp_ext_msg_handle(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *bufs);
ssize_t ecp_ext_pld_store(ECPConnection *conn, ecp_seq_t seq, unsigned char *payload, size_t pld_size, ECP2Buffer *bufs);
ssize_t ecp_ext_pld_handle(ECPConnection *conn, ecp_seq_t seq, unsigned char *payload, size_t pld_size, ECP2Buffer *bufs);
ssize_t ecp_ext_pld_send(ECPConnection *conn, ECPBuffer *payload, size_t pld_size, ECPBuffer *packet, size_t pkt_size, unsigned char flags, ECPTimerItem *ti, ecp_tr_addr_t *addr);
ssize_t ecp_ext_msg_send(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, ECPBuffer *packet, ECPBuffer *payload);

#else

#define ecp_ext_err_handle(c,t,e)                   (ECP_PASS)
#define ecp_ext_conn_open(c)                        (ECP_OK)
#define ecp_ext_conn_destroy(c)                     ;
#define ecp_ext_msg_handle(c,s,t,m,sz,b)            (0)
#define ecp_ext_pld_store(c,s,p,sz,b)               (0)
#define ecp_ext_pld_handle(c,s,p,sz,b)              (0)
#define ecp_ext_pld_send(c,p1,sz1,p2,sz2,f,t,a)     (0)
#define ecp_ext_msg_send(c,t,m,sz,p1,p2)            (0)

#endif
