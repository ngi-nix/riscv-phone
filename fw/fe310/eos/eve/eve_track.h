#include <stdint.h>

#define EVE_TRACK_TYPE_INERT            1
#define EVE_TRACK_TYPE_OSC              2
#define EVE_TRACK_FRICTION              500

typedef struct EVETrackOsc {
    int x;
    int y;
    double f;
    double d;
    double a;
    uint32_t t_max;
} EVETrackOsc;

void eve_track_init(void);
void eve_track_set(uint8_t type, void *param);

void eve_track_inert_init(EVETouchTimer *timer, EVETouch *touch);
int eve_track_inert_tick(EVETouchTimer *timer, EVETouch *touch);
void eve_track_osc_init(EVETrackOsc *p, int x, int y, uint32_t T, double d, uint32_t t_max);
int eve_track_osc_tick(EVETouchTimer *timer, EVETouch *touch);
