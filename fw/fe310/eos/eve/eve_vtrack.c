#include <stdlib.h>
#include <math.h>

#include "eve.h"

static EVEVTrack _vtrack;
static EVEPhyAcc _vtrack_acc;

void eve_vtrack_init(void) {
    eve_phy_acc_init(&_vtrack_acc, -EVE_VTRACK_ACC_A);
    eve_vtrack_reset();
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
    eve_vtrack_set(eve_vtrack_acc_start, eve_vtrack_acc_tick, NULL, &_vtrack_acc);
}

void eve_vtrack_start(EVETouch *touch, uint8_t tag0, uint32_t to) {
    eve_touch_timer_set(touch, EVE_TOUCH_ETYPE_TRACK, tag0, to);
    if (_vtrack.start) _vtrack.start(touch, _vtrack.param);
}

void eve_vtrack_stop(EVETouch *touch) {
    eve_touch_timer_clear(touch);
    eve_vtrack_reset();
}

void eve_vtrack_acc_start(EVETouch *touch, void *p) {
    EVEPhyAcc *param = (EVEPhyAcc *)p;
    eve_phy_acc_start(param, touch->x, touch->y, touch->vx, touch->vy);
}

int eve_vtrack_acc_tick(EVETouch *touch, void *p) {
    EVEPhyAcc *param = (EVEPhyAcc *)p;

    return eve_phy_acc_tick(param, eve_time_get_tick() - touch->t, &touch->x, &touch->y);
}

void eve_vtrack_lho_start(EVETouch *touch, void *p) {
    EVEPhyLHO *param = (EVEPhyLHO *)p;

    eve_phy_lho_start(param, touch->x, touch->y);
}

int eve_vtrack_lho_tick(EVETouch *touch, void *p) {
    EVEPhyLHO *param = (EVEPhyLHO *)p;

    return eve_phy_lho_tick(param, eve_time_get_tick() - touch->t, &touch->x, &touch->y);
}

void eve_vtrack_lho_stop(EVETouch *touch, void *p) {
    eve_vtrack_reset();
}

