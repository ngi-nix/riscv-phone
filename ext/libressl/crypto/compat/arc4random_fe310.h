#define _ARC4_LOCK() ;
#define _ARC4_UNLOCK() ;

static inline void
_getentropy_fail(void)
{
	_exit(1);
}

static inline void
_rs_forkdetect(void)
{
}

int arc4random_alloc(void **rsp, size_t rsp_size, void **rsxp, size_t rsxp_size);

static inline int
_rs_allocate(struct _rs **rsp, struct _rsx **rsxp)
{
	return arc4random_alloc((void **)rsp, sizeof(**rsp), (void **)rsxp, sizeof(**rsxp));
}

void arc4random_close(void **rsp, void **rsxp)
{
	*rsp = rs;
	*rsxp = rsx;
	rs = NULL;
	rsx = NULL;
}