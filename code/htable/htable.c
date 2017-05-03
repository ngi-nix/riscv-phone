#include <core.h>

#include <string.h>

#include "hashtable.h"
#include "hashtable_private.h"

static unsigned int hash_fn(void *k) {
    return *((unsigned int *)k);
}

static int eq_fn(void *k1, void *k2) {
    return !memcmp(k1, k2, ECP_ECDH_SIZE_KEY);
}

static void *h_create(ECPContext *ctx) {
    int rv;
    struct hashtable *h = malloc(sizeof(struct hashtable));
    if (h == NULL) return NULL;
        
    rv = create_hashtable(h, 1000, (unsigned int (*)(void *))ctx->cr.dh_pub_hash_fn, (int (*)(void *, void *))ctx->cr.dh_pub_hash_eq, NULL, NULL, NULL);
    if (!rv) {
        free(h);
        return NULL;
    }
    
    return h;
}

static void h_destroy(void *h) {
    hashtable_destroy(h);
    free(h);
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

int ecp_htable_init(ECPHTableIface *h) {
    h->init = 1;
    h->create = h_create;
    h->destroy = h_destroy;
    h->insert = h_insert;
    h->remove = h_remove;
    h->search = h_search;
    return 0;
}
