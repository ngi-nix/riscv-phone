typedef struct ECPFragIter {
    ecp_seq_t seq;
    unsigned char frag_cnt;
    unsigned char *buffer;
    size_t buf_size;
    size_t pld_size;
} ECPFragIter;

int ecp_frag_iter_init(ECPRBConn *conn, ECPFragIter *iter, unsigned char *buffer, size_t buf_size);
ECPFragIter *ecp_frag_iter_get(ECPRBConn *conn);
void ecp_frag_iter_reset(ECPFragIter *iter);
ssize_t ecp_msg_frag(ECPConnection *conn, unsigned char mtype, unsigned char *msg, size_t msg_size, ECPBuffer *packet, ECPBuffer *payload);
ssize_t ecp_pld_defrag(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *payload, size_t pld_size);