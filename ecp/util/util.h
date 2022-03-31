int ecp_util_load_key(ecp_ecdh_public_t *public, ecp_ecdh_private_t *private, char *filename);
int ecp_util_save_key(ecp_ecdh_public_t *public, ecp_ecdh_private_t *private, char *filename);

int ecp_util_load_pub(ecp_ecdh_public_t *public, char *filename);
int ecp_util_save_pub(ecp_ecdh_public_t *public, char *filename);

int ecp_util_load_node(ECPNode *node, char *filename);
int ecp_util_save_node(ECPNode *node, char *filename);
