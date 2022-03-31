#include <stdlib.h>

int getentropy(unsigned char *b, size_t sz);
int arc4random_alloc(void **rsp, size_t rsp_size, void **rsxp, size_t rsxp_size);
void arc4random_close(void **rsp, void **rsxp);