void *ecp_ht_create(ECPContext *ctx);
void ecp_ht_destroy(void *h);
int ecp_ht_insert(void *h, unsigned char *k, ECPConnection *v);
ECPConnection *ecp_ht_remove(void *h, unsigned char *k);
ECPConnection *ecp_ht_search(void *h, unsigned char *k);
