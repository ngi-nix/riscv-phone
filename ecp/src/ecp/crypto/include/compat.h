#include <sys/types.h>
#include <stdint.h>

#ifndef HAVE_GETENTROPY
int getentropy(void *buf, size_t buflen);
#endif

#ifndef HAVE_EXPLICIT_BZERO
void explicit_bzero(void *, size_t);
#endif

#ifndef HAVE_TIMINGSAFE_BCMP
int timingsafe_bcmp(const void *b1, const void *b2, size_t n);
#endif

#ifndef HAVE_TIMINGSAFE_MEMCMP
int timingsafe_memcmp(const void *b1, const void *b2, size_t len);
#endif

#ifndef HAVE_ARC4RANDOM_BUF
uint32_t arc4random(void);
void arc4random_buf(void *_buf, size_t n);
uint32_t arc4random_uniform(uint32_t upper_bound);
#endif
