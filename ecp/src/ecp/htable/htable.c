#include <stdlib.h>

#include <core.h>
#include <cr.h>
#include <tr.h>
#include <ht.h>

ecp_ht_table_t *ecp_ht_create_keys(void) {
    return hashtable_create(1000, (unsigned int (*)(void *))ecp_ecdh_pub_hash, (int (*)(void *, void *))ecp_ecdh_pub_eq);
}

ecp_ht_table_t *ecp_ht_create_addrs(void) {
    return hashtable_create(1000, (unsigned int (*)(void *))ecp_tr_addr_hash, (int (*)(void *, void *))ecp_tr_addr_eq);
}

void ecp_ht_destroy(ecp_ht_table_t *h) {
    hashtable_destroy(h, 0);
}

int ecp_ht_insert(ecp_ht_table_t *h, void *k, ECPConnection *v) {
    int rv;

    rv = hashtable_insert(h, k, v);
    if (rv == 0) return ECP_ERR;
    return ECP_OK;
}

ECPConnection *ecp_ht_remove(ecp_ht_table_t *h, void *k) {
    return hashtable_remove(h, k);
}

ECPConnection *ecp_ht_search(ecp_ht_table_t *h, void *k) {
    return hashtable_search(h, k);
}

void ecp_ht_itr_create(ecp_ht_itr_t *i, ecp_ht_table_t *h) {
    hashtable_iterator(i, h);
}

int ecp_ht_itr_advance(ecp_ht_itr_t *i) {
    int rv;

    rv = hashtable_iterator_advance(i);
    if (rv == 0) return ECP_ITR_END;
    return rv;
}

int ecp_ht_itr_remove(ecp_ht_itr_t *i) {
    int rv;

    rv = hashtable_iterator_remove(i);
    if (rv == 0) return ECP_ITR_END;
    return ECP_OK;
}

int ecp_ht_itr_search(ecp_ht_itr_t *i, void *k) {
    int rv;

    rv = hashtable_iterator_search(i, k);
    if (rv == 0) return ECP_ERR;
    return ECP_OK;
}

void *ecp_ht_itr_key(ecp_ht_itr_t *i) {
    return hashtable_iterator_key(i);
}

ECPConnection *ecp_ht_itr_value(ecp_ht_itr_t *i) {
    return hashtable_iterator_value(i);
}
