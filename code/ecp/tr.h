int ecp_tr_addr_eq(ECPNetAddr *addr1, ECPNetAddr *addr2);
int ecp_tr_addr_set(ECPNetAddr *addr, void *addr_s);
int ecp_tr_open(int *sock, void *addr_s);
void ecp_tr_close(int *sock);
ssize_t ecp_tr_send(int *sock, ECPBuffer *packet, size_t msg_size, ECPNetAddr *addr, unsigned char flags);
ssize_t ecp_tr_recv(int *sock, ECPBuffer *packet, ECPNetAddr *addr, int timeout);
void ecp_tr_buf_free(ECP2Buffer *b, unsigned char flags);
void ecp_tr_buf_flag_set(ECP2Buffer *b, unsigned char flags);
void ecp_tr_buf_flag_clear(ECP2Buffer *b, unsigned char flags);
