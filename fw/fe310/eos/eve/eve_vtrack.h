#include <stdint.h>

#define EVE_VTRACK_ACC_A        1000

typedef void (*eve_vtrack_start_t) (EVETouch *, void *);
typedef int  (*eve_vtrack_tick_t)  (EVETouch *, void *);
typedef void (*eve_vtrack_stop_t)  (EVETouch *, void *);

typedef struct EVEVTrack {
    eve_vtrack_start_t start;
    eve_vtrack_tick_t  tick;
    eve_vtrack_stop_t  stop;
    void *param;
} EVEVTrack;

void eve_vtrack_init(void);
EVEVTrack *eve_vtrack_get(void);
void eve_vtrack_set(eve_vtrack_start_t start, eve_vtrack_tick_t tick, eve_vtrack_stop_t stop, void *param);
void eve_vtrack_reset(void);
void eve_vtrack_start(EVETouch *touch, uint8_t tag0, uint32_t to);
void eve_vtrack_stop(EVETouch *touch);

void eve_vtrack_acc_start(EVETouch *touch, void *p);
int eve_vtrack_acc_tick(EVETouch *touch, void *p);

void eve_vtrack_lho_start(EVETouch *touch, void *p);
int eve_vtrack_lho_tick(EVETouch *touch, void *p);
void eve_vtrack_lho_stop(EVETouch *touch, void *p);
