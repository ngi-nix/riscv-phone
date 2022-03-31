int ecp_tr_init(ECPContext *ctx);
unsigned int ecp_tr_addr_hash(ecp_tr_addr_t *addr);
int ecp_tr_addr_eq(ecp_tr_addr_t *addr1, ecp_tr_addr_t *addr2);
int ecp_tr_addr_set(ecp_tr_addr_t *addr, void *addr_s);
int ecp_tr_open(ECPSocket *sock, void *addr_s);
void ecp_tr_close(ECPSocket *sock);
ssize_t ecp_tr_send(ECPSocket *sock, ECPBuffer *packet, size_t pkt_size, ecp_tr_addr_t *addr, unsigned char flags);
ssize_t ecp_tr_recv(ECPSocket *sock, ECPBuffer *packet, ecp_tr_addr_t *addr, int timeout);
void ecp_tr_release(ECPBuffer *packet, unsigned char more);
