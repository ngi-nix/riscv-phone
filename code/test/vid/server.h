#include "core.h"

ssize_t send_frame(unsigned char *buffer, size_t size, ecp_pts_t pts);
int conn_is_open(void);
int init_server(char *address, char *key);