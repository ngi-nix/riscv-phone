#include <stdlib.h>

#include <core.h>
#include <cr.h>
#include <tr.h>
#include <ht.h>

#include "hashtable.h"

void *ecp_ht_create_keys(void) {
    return hashtable_create(1000, (unsigned int (*)(void *))ecp_ecdh_pub_hash, (int (*)(void *, void *))ecp_ecdh_pub_eq);
}

void *ecp_ht_create_addrs(void) {
    return hashtable_create(1000, (unsigned int (*)(void *))ecp_tr_addr_hash, (int (*)(void *, void *))ecp_tr_addr_eq);
}

void ecp_ht_destroy(void *h) {
    hashtable_destroy(h, 0);
}

int ecp_ht_insert(void *h, void *k, ECPConnection *v) {
    int rv = hashtable_insert(h, k, v);
    if (!rv) return ECP_ERR;
    return ECP_OK;
}

ECPConnection *ecp_ht_remove(void *h, void *k) {
    return hashtable_remove(h, k);
}

ECPConnection *ecp_ht_search(void *h, void *k) {
    return hashtable_search(h, k);
}
