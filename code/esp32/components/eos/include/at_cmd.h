#include <sys/types.h>
#include <stdint.h>
#include <regex.h>

#define AT_SIZE_NMATCH      4
#define AT_SIZE_PATTERN     64

#define AT_SIZE_URC_LIST    16

#define AT_URC_OK           0
#define AT_URC_MORE         1

typedef int (*at_urc_cb_t) (char *, regmatch_t[]);

void at_init(void);
int at_urc_process(char *urc);
int at_urc_insert(char *pattern, at_urc_cb_t cb, int flags);
int at_urc_delete(char *pattern);
int at_expect(char *str_ok, char *str_err, uint32_t timeout);