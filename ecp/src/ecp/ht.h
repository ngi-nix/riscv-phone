ecp_ht_table_t *ecp_ht_create_keys(void);
ecp_ht_table_t *ecp_ht_create_addrs(void);

void ecp_ht_destroy(ecp_ht_table_t *h);
int ecp_ht_insert(ecp_ht_table_t *h, void *k, ECPConnection *v);
ECPConnection *ecp_ht_remove(ecp_ht_table_t *h, void *k);
ECPConnection *ecp_ht_search(ecp_ht_table_t *h, void *k);

void ecp_ht_itr_create(ecp_ht_itr_t *i, ecp_ht_table_t *h);
int ecp_ht_itr_advance(ecp_ht_itr_t *i);
int ecp_ht_itr_remove(ecp_ht_itr_t *i);
int ecp_ht_itr_search(ecp_ht_itr_t *i, void *k);
void *ecp_ht_itr_key(ecp_ht_itr_t *i);
ECPConnection *ecp_ht_itr_value(ecp_ht_itr_t *i);
