#include <core.h>
#include <ht.h>
#include <cr.h>

#include "hashtable.h"

void *ecp_ht_create(ECPContext *ctx) {
    printf("NEFORE CREATE\n");
    void *r = hashtable_create(1000, (unsigned int (*)(void *))ecp_cr_dh_pub_hash_fn, (int (*)(void *, void *))ecp_cr_dh_pub_hash_eq);
    printf("AFTER CREATE\n");
    return r;
}

void ecp_ht_destroy(void *h) {
    hashtable_destroy(h, 0);
}

int ecp_ht_insert(void *h, unsigned char *k, ECPConnection *v) {
    int rv = hashtable_insert(h, k, v);
    if (!rv) return ECP_ERR;
    return ECP_OK;
}

ECPConnection *ecp_ht_remove(void *h, unsigned char *k) {
    return hashtable_remove(h, k);
}

ECPConnection *ecp_ht_search(void *h, unsigned char *k) {
    return hashtable_search(h, k);
}

