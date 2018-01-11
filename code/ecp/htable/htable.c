#include <core.h>

#include <string.h>

#include "hashtable.h"

static void *h_create(ECPContext *ctx) {
    return create_hashtable(1000, (unsigned int (*)(void *))ctx->cr.dh_pub_hash_fn, (int (*)(void *, void *))ctx->cr.dh_pub_hash_eq, NULL, NULL, NULL);
}

static void h_destroy(void *h) {
    hashtable_destroy(h);
}

static int h_insert(void *h, unsigned char *k, ECPConnection *v) {
    int rv = hashtable_insert(h, k, v);
    if (!rv) return ECP_ERR;
    return ECP_OK;
}

static ECPConnection *h_remove(void *h, unsigned char *k) {
    return hashtable_remove(h, k);
}

static ECPConnection *h_search(void *h, unsigned char *k) {
    return hashtable_search(h, k);
}

#ifdef ECP_WITH_HTABLE
int ecp_htable_init(ECPHTableIface *h) {
    h->init = 1;
    h->create = h_create;
    h->destroy = h_destroy;
    h->insert = h_insert;
    h->remove = h_remove;
    h->search = h_search;
    return ECP_OK;
}
#endif