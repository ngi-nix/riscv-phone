#include <stdint.h>

#define EVE_TRACK_TYPE_INERT            1
#define EVE_TRACK_TYPE_OSC              2
#define EVE_TRACK_FRICTION              500

typedef void (*eve_track_init_t) (EVETouchTimer *, EVETouch *);
typedef int  (*eve_track_tick_t) (EVETouchTimer *, EVETouch *);
typedef void (*eve_track_stop_t) (EVETouchTimer *, EVETouch *);

typedef struct EVETracker {
    eve_track_init_t init;
    eve_track_tick_t tick;
    eve_track_stop_t stop;
} EVETracker;

typedef struct EVETrackOsc {
    int x;
    int y;
    double f;
    double d;
    double a;
    uint32_t t_max;
} EVETrackOsc;

void eve_init_track(void);
EVETracker *eve_track_get_tracker(void);
void eve_track_set_handler(eve_track_init_t init, eve_track_tick_t tick, eve_track_stop_t stop, void *param);
void eve_track_set(uint8_t type, void *param);
void eve_track_stop(EVETouchTimer *timer, EVETouch *touch);

void eve_track_inert_init(EVETouchTimer *timer, EVETouch *touch);
int eve_track_inert_tick(EVETouchTimer *timer, EVETouch *touch);
int eve_track_osc_tick(EVETouchTimer *timer, EVETouch *touch);
void eve_track_osc_init(EVETrackOsc *p, int x, int y, uint32_t T, double d, uint32_t t_max);
