#include <unistd.h>

#include <openssl/opensslconf.h>

static inline void
OpenSSLDie(const char *file, int line, const char *assertion)
{
        _exit(1);
}
/* die if we have to */
void OpenSSLDie(const char *file, int line, const char *assertion);
#define OPENSSL_assert(e)       (void)((e) ? 0 : (OpenSSLDie(__FILE__, __LINE__, #e),1))
