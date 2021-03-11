#include <stdint.h>

#define EVE_VTRACK_FRICTION             500

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
void eve_vtrack_start(uint8_t tag0, int i);
void eve_vtrack_stop(void);

typedef struct EVEVTrackInert {
    int fc;
    int f;
    int x0;
    int y0;
} EVEVTrackInert;

EVEVTrackInert *eve_vtrack_inert_get_param(void);
void eve_vtrack_inert_init(EVEVTrackInert *param, int fc);
void eve_vtrack_inert_start(EVETouch *touch, void *p);
int eve_vtrack_inert_tick(EVETouch *touch, void *p);
void eve_vtrack_inert_stop(EVETouch *touch, void *p);

typedef struct EVEVTrackOsc {
    int x;
    int y;
    double f;
    double d;
    double a;
    uint32_t t_max;
    int x0;
    int y0;
} EVEVTrackOsc;

EVEVTrackOsc *eve_vtrack_osc_get_param(void);
void eve_vtrack_osc_init(EVEVTrackOsc *param, int x, int y, uint32_t T, double d, uint32_t t_max);
void eve_vtrack_osc_start(EVETouch *touch, void *p);
int eve_vtrack_osc_tick(EVETouch *touch, void *p);
void eve_vtrack_osc_stop(EVETouch *touch, void *p);
