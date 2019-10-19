int ecp_tm_init(ECPContext *ctx);
ecp_cts_t ecp_tm_abstime_ms(ecp_cts_t msec);
void ecp_tm_sleep_ms(ecp_cts_t msec);
void ecp_tm_timer_set(ecp_cts_t next);
