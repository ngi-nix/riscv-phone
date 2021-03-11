#include <stdlib.h>
#include <math.h>

#include "eve.h"

static EVEVTrack _vtrack;
static EVEVTrackInert _vtrack_inert;

void eve_vtrack_init(void) {
    eve_vtrack_inert_init(&_vtrack_inert, EVE_VTRACK_FRICTION);
    eve_vtrack_set(eve_vtrack_inert_start, eve_vtrack_inert_tick, eve_vtrack_inert_stop, &_vtrack_inert);
}

EVEVTrack *eve_vtrack_get(void) {
    return &_vtrack;
}

void eve_vtrack_set(eve_vtrack_start_t start, eve_vtrack_tick_t tick, eve_vtrack_stop_t stop, void *param) {
    _vtrack.start = start;
    _vtrack.tick = tick;
    _vtrack.stop = stop;
    _vtrack.param = param;
}

void eve_vtrack_reset(void) {
    eve_vtrack_set(eve_vtrack_inert_start, eve_vtrack_inert_tick, eve_vtrack_inert_stop, &_vtrack_inert);
}

void eve_vtrack_start(uint8_t tag0, int i) {
    EVETouch *touch = eve_touch_get(i);

    eve_touch_timer_set(tag0, i, EVE_TOUCH_ETYPE_TRACK, EVE_TOUCH_TIMEOUT_TRACK);
    if (_vtrack.start) _vtrack.start(touch, _vtrack.param);
}

void eve_vtrack_stop(void) {
    eve_touch_timer_clear();
    eve_vtrack_reset();
}


EVEVTrackInert *eve_vtrack_inert_get_param(void) {
    return &_vtrack_inert;
}

void eve_vtrack_inert_init(EVEVTrackInert *param, int fc) {
    param->fc = fc;
}

void eve_vtrack_inert_start(EVETouch *touch, void *p) {
    EVEVTrackInert *param = (EVEVTrackInert *)p;
    double d = sqrt(touch->vx * touch->vx + touch->vy * touch->vy);

    param->x0 = touch->x;
    param->y0 = touch->y;
    param->f = (double)(EVE_RTC_FREQ) * d / param->fc;
}

int eve_vtrack_inert_tick(EVETouch *touch, void *p) {
    EVEVTrackInert *param = (EVEVTrackInert *)p;
    int dt = eve_time_get_tick() - touch->t;
    int f = param->f;
    int more = 1;

    if (dt >= f / 2) {
        dt = f / 2;
        more = 0;
    }
    touch->x = param->x0 + (touch->vx * dt - touch->vx * dt / f * dt) / (int)(EVE_RTC_FREQ);
    touch->y = param->y0 + (touch->vy * dt - touch->vy * dt / f * dt) / (int)(EVE_RTC_FREQ);

    return more;
}

void eve_vtrack_inert_stop(EVETouch *touch, void *p) {
    touch->evt |= EVE_TOUCH_ETYPE_TRACK_STOP;
}


static EVEVTrackOsc _vtrack_osc;

EVEVTrackOsc *eve_vtrack_osc_get_param(void) {
    return &_vtrack_osc;
}

void eve_vtrack_osc_init(EVEVTrackOsc *param, int x, int y, uint32_t T, double d, uint32_t t_max) {
    double f0 = 2 * M_PI / (T * EVE_RTC_FREQ / 1000);

    if (d < 0) d = 0;
    if (d > 1) d = 1;
    param->x = x;
    param->y = y;
    param->f = d ? f0 * sqrt(1 - d * d) : f0;
    param->d = d;
    param->a = -d * f0;
    param->t_max = t_max;
}

void eve_vtrack_osc_start(EVETouch *touch, void *p) {
    EVEVTrackOsc *param = (EVEVTrackOsc *)p;

    param->x0 = touch->x;
    param->y0 = touch->y;
}

int eve_vtrack_osc_tick(EVETouch *touch, void *p) {
    EVEVTrackOsc *param = (EVEVTrackOsc *)p;
    int dt = eve_time_get_tick() - touch->t;
    int ax = param->x0 - param->x;
    int ay = param->y0 - param->y;
    int more = 1;

    if (param->t_max && (dt >= param->t_max)) {
        dt = param->t_max;
        more = 0;
    }
    if (param->d) {
        double e = exp(param->a * dt);
        ax = ax * e;
        ay = ay * e;
        if ((ax == 0) && (ay == 0)) more = 0;
    }
    touch->x = param->x + ax * cos(param->f * dt);
    touch->y = param->y + ay * cos(param->f * dt);

    return more;
}

void eve_vtrack_osc_stop(EVETouch *touch, void *p) {
    touch->evt |= EVE_TOUCH_ETYPE_TRACK_STOP;
    eve_vtrack_reset();
}
