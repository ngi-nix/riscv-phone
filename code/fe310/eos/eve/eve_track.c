#include <stdlib.h>
#include <math.h>

#include "eve.h"
#include "eve_platform.h"

static EVETracker _tracker;

void eve_init_track(void) {
    eve_track_set_handler(eve_track_inert_init, eve_track_inert_tick, eve_track_stop, NULL);
}

EVETracker *eve_track_get_tracker(void) {
    return &_tracker;
}

void eve_track_set_handler(eve_track_init_t init, eve_track_tick_t tick, eve_track_stop_t stop, void *param) {
    if (stop == NULL) stop = eve_track_stop;

    _tracker.init = init;
    _tracker.tick = tick;
    _tracker.stop = stop;
    eve_touch_get_timer()->p = param;
}

void eve_track_set(uint8_t type, void *param) {
    switch (type) {
        case EVE_TRACK_TYPE_INERT:
            eve_track_set_handler(eve_track_inert_init, eve_track_inert_tick, eve_track_stop, NULL);
            break;
        case EVE_TRACK_TYPE_OSC:
            eve_track_set_handler(NULL, eve_track_osc_tick, eve_track_stop, param);
            break;
        default:
            break;
    }
}

void eve_track_stop(EVETouchTimer *timer, EVETouch *touch) {
    touch->evt |= EVE_TOUCH_ETYPE_TRACK_STOP;
}


void eve_track_inert_init(EVETouchTimer *timer, EVETouch *touch) {
    double d = sqrt(touch->vx * touch->vx + touch->vy * touch->vy);
    int fc = (double)(EVE_RTC_FREQ) * d / EVE_TRACK_FRICTION;

    timer->p = (void *)fc;
}

int eve_track_inert_tick(EVETouchTimer *timer, EVETouch *touch) {
    int dt = eve_time_get_tick() - touch->t;
    int fc = (int)timer->p;
    int more = 1;

    if (dt >= fc / 2) {
        dt = fc / 2;
        more = 0;
    }
    touch->x = timer->x0 + (touch->vx * dt - touch->vx * dt / fc * dt) / (int)(EVE_RTC_FREQ);
    touch->y = timer->y0 + (touch->vy * dt - touch->vy * dt / fc * dt) / (int)(EVE_RTC_FREQ);
    return more;
}

int eve_track_osc_tick(EVETouchTimer *timer, EVETouch *touch) {
    EVETrackOsc *p = (EVETrackOsc *)timer->p;
    int dt = eve_time_get_tick() - touch->t;
    int ax = timer->x0 - p->x;
    int ay = timer->y0 - p->y;
    int more = 1;

    if (p->t_max && (dt >= p->t_max)) {
        dt = p->t_max;
        more = 0;
    }
    if (p->d) {
        double e = exp(p->a * dt);
        ax = ax * e;
        ay = ay * e;
        if ((ax == 0) && (ay == 0)) more = 0;
    }
    touch->x = p->x + ax * cos(p->f * dt);
    touch->y = p->y + ay * cos(p->f * dt);
    return more;
}

void eve_track_osc_init(EVETrackOsc *p, int x, int y, uint32_t T, double d, uint32_t t_max) {
    double f0 = 2 * M_PI / (T * EVE_RTC_FREQ / 1000);

    if (d < 0) d = 0;
    if (d > 1) d = 1;
    p->x = x;
    p->y = y;
    p->f = d ? f0 * sqrt(1 - d * d) : f0;
    p->d = d;
    p->a = -d * f0;
    p->t_max = t_max;
}
