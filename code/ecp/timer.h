#define ECP_MAX_TIMER       8

#define ECP_ERR_MAX_TIMER   -110

struct ECPTimerItem;

typedef ssize_t ecp_timer_retry_t (struct ECPConnection *, struct ECPTimerItem *);

typedef struct ECPTimerItem {
    struct ECPConnection *conn;
    unsigned char mtype;
    short cnt;
    ecp_cts_t abstime;
    ecp_cts_t timeout;
    ecp_timer_retry_t *retry;
} ECPTimerItem;

typedef struct ECPTimer {
    ECPTimerItem item[ECP_MAX_TIMER];
    short head;
#ifdef ECP_WITH_PTHREAD
    pthread_mutex_t mutex;
#endif
} ECPTimer;

int ecp_timer_create(ECPTimer *timer);
void ecp_timer_destroy(ECPTimer *timer);
int ecp_timer_item_init(ECPTimerItem *ti, struct ECPConnection *conn, unsigned char mtype, short cnt, ecp_cts_t timeout);
int ecp_timer_push(ECPTimerItem *ti);
void ecp_timer_pop(struct ECPConnection *conn, unsigned char mtype);
void ecp_timer_remove(struct ECPConnection *conn);
ecp_cts_t ecp_timer_exe(struct ECPSocket *sock);
ssize_t ecp_timer_send(struct ECPConnection *conn, ecp_timer_retry_t *send_f, unsigned char mtype, short cnt, ecp_cts_t timeout);