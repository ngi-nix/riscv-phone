void *ecp_ht_create_keys(void);
void *ecp_ht_create_addrs(void);

void ecp_ht_destroy(void *h);
int ecp_ht_insert(void *h, void *k, ECPConnection *v);
ECPConnection *ecp_ht_remove(void *h, void *k);
ECPConnection *ecp_ht_search(void *h, void *k);
