#include <stdlib.h>
#include <string.h>

#include "eve.h"

#define EVE_THRESHOLD_X         5
#define EVE_THRESHOLD_Y         5
#define EVE_TIMEOUT_TAP         1000
#define EVE_TIMEOUT_TRACK       20
#define EVE_TRAVG               3

#define EVE_NOTOUCH             0x80000000

#define EVE_MAX_TOUCH           5

static int _intr_mask = EVE_INT_TAG | EVE_INT_TOUCH;
static int _multitouch;
static uint8_t _tag0;

static EVETouch _touch[EVE_MAX_TOUCH];
static EVETouchTimer _touch_timer;
static EVEExtTracker _ext_tracker;

static eve_touch_handler_t _touch_handler;
static void *_touch_handler_param;
static uint8_t _tag_opt[256];

static const uint32_t _reg_touch[] = {
    REG_CTOUCH_TOUCH0_XY,
    REG_CTOUCH_TOUCH1_XY,
    REG_CTOUCH_TOUCH2_XY,
    REG_CTOUCH_TOUCH3_XY
};

static const uint32_t _reg_tag[] = {
    REG_TOUCH_TAG,
    REG_TOUCH_TAG1,
    REG_TOUCH_TAG2,
    REG_TOUCH_TAG3,
    REG_TOUCH_TAG4
};

static const uint32_t _reg_track[] = {
    REG_TRACKER,
    REG_TRACKER_1,
    REG_TRACKER_2,
    REG_TRACKER_3,
    REG_TRACKER_4
};

static inline void _touch_evt_clear(EVETouch *touch) {
    touch->evt &= ~EVE_TOUCH_EVT_MASK;
}

static void _touch_timer_set(uint8_t tag, uint8_t idx, uint16_t evt, int x0, int y0, uint32_t to) {
    _touch_timer.tag = tag;
    _touch_timer.idx = idx;
    _touch_timer.evt = evt;
    _touch_timer.x0 = x0;
    _touch_timer.y0 = y0;
    eve_timer_set(to);
}

static void _touch_timer_clear(void) {
    eve_timer_clear();
    _touch_timer.tag = 0;
    _touch_timer.evt = 0;
}

void eve_handle_touch(void) {
    int i;
    char touch_ex = 0;
    char int_ccomplete = 0;
    uint8_t tag0 = _tag0;
    uint8_t touch_last = 0;
    uint8_t flags = eve_read8(REG_INT_FLAGS) & _intr_mask;

    if (!_multitouch && (flags & EVE_INT_TOUCH)) _multitouch = 1;
    for (i=0; i<EVE_MAX_TOUCH; i++) {
        uint8_t touch_tag;
        uint32_t touch_xy;
        uint64_t now = 0;
        EVETouch *touch = &_touch[i];

        _touch_evt_clear(touch);
        touch_xy = i < 4 ? eve_read32(_reg_touch[i]) : (((uint32_t)eve_read16(REG_CTOUCH_TOUCH4_X) << 16) | eve_read16(REG_CTOUCH_TOUCH4_Y));

        if (touch_xy != 0x80008000) {
            int16_t touch_x = touch_xy >> 16;
            int16_t touch_y = touch_xy & 0xffff;
            now = eve_time_get_tick();
            if (touch->x == EVE_NOTOUCH) {
                if (!_tag0 && _touch_timer.tag) {
                    if (_touch_timer.evt & EVE_TOUCH_ETYPE_TAP1) {
                        int dx = touch_x - touch->x0;
                        int dy = touch_y - touch->y0;
                        dx = dx < 0 ? -dx : dx;
                        dy = dy < 0 ? -dy : dy;
                        if ((dx > EVE_THRESHOLD_X) || (dy > EVE_THRESHOLD_Y)) {
                            touch->evt |= EVE_TOUCH_ETYPE_TAP1;
                        } else {
                            touch->evt |= EVE_TOUCH_ETYPE_TAP2;
                        }
                    }
                    if (_touch_timer.evt & EVE_TOUCH_ETYPE_TRACK) {
                        touch->evt |= EVE_TOUCH_ETYPE_TRACK_STOP;
                        if (_ext_tracker.stop) _ext_tracker.stop(&_touch_timer, touch);
                    }
                    if (_touch_handler && (touch->evt & EVE_TOUCH_EVT_MASK)) {
                        _touch_handler(_touch_handler_param, _touch_timer.tag, i);
                    }
                    _touch_timer_clear();
                }
                touch->evt = EVE_TOUCH_ETYPE_POINT;
                touch->tag0 = 0;
                touch->tag = 0;
                touch->tag_up = 0;
                touch->tracker.tag = 0;
                touch->tracker.track = 0;
                touch->tracker.val = 0;
                touch->t = 0;
                touch->vx = 0;
                touch->vy = 0;
                touch->x0 = touch_x;
                touch->y0 = touch_y;
            } else if (touch->t) {
                int dt = now - touch->t;
                int vx = ((int)touch_x - touch->x) * (int)(EVE_RTC_FREQ) / dt;
                int vy = ((int)touch_y - touch->y) * (int)(EVE_RTC_FREQ) / dt;
                touch->vx = touch->vx ? (vx + touch->vx * EVE_TRAVG) / (EVE_TRAVG + 1) : vx;
                touch->vy = touch->vy ? (vy + touch->vy * EVE_TRAVG) / (EVE_TRAVG + 1) : vy;
                touch->t = now;
            }
            touch->x = touch_x;
            touch->y = touch_y;
            if (_multitouch || (flags & EVE_INT_TAG)) {
                touch_tag = eve_read8(_reg_tag[i]);
            } else {
                touch_tag = touch->tag;
            }
            touch_ex = 1;
        } else {
            touch_tag = 0;
            if (touch->x != EVE_NOTOUCH) {
                touch->evt |= EVE_TOUCH_ETYPE_POINT_UP;
                if (_touch_timer.tag && (i == 0)) {
                    _touch_timer.evt &= ~EVE_TOUCH_ETYPE_LPRESS;
                    if (!_touch_timer.evt) _touch_timer_clear();
                }
                if (touch->tracker.tag && touch->tracker.track) {
                    if (!_touch_timer.tag && (_tag_opt[touch->tracker.tag] & EVE_TOUCH_OPT_TRACK_EXT)) {
                        _touch_timer_set(touch->tracker.tag, i, EVE_TOUCH_ETYPE_TRACK, touch->x, touch->y, EVE_TIMEOUT_TRACK);
                        if (_ext_tracker.init) _ext_tracker.init(&_touch_timer, touch);
                    } else {
                        touch->evt |= EVE_TOUCH_ETYPE_TRACK_STOP;
                    }
                }
                touch->x = EVE_NOTOUCH;
                touch->y = EVE_NOTOUCH;
            }
        }
        if (touch_tag != touch->tag) {
            if (touch_tag) {
                if (!touch->tag0) {
                    touch->tag0 = touch_tag;
                    if (_tag_opt[touch_tag] & EVE_TOUCH_OPT_TRACK_MASK) {
                        touch->tracker.tag = touch_tag;
                    }
                    if (touch->tracker.tag && !(_tag_opt[touch->tracker.tag] & EVE_TOUCH_OPT_TRACK_XY)) {
                        touch->tracker.track = 1;
                        touch->evt |= EVE_TOUCH_ETYPE_TRACK_START;
                        touch->t = now;
                    }
                    if (!_tag0 && (_tag_opt[touch_tag] & EVE_TOUCH_OPT_TIMER_MASK)) {
                        uint16_t _evt = 0;

                        if (_tag_opt[touch_tag] & EVE_TOUCH_OPT_LPRESS) _evt |= EVE_TOUCH_ETYPE_LPRESS;
                        if (_tag_opt[touch_tag] & EVE_TOUCH_OPT_DTAP) _evt |= EVE_TOUCH_ETYPE_TAP1;
                        _touch_timer_set(touch_tag, 0, _evt, 0, 0, EVE_TIMEOUT_TAP);
                    }
                }
                if (!_tag0) _tag0 = tag0 = touch_tag;
            }
            touch->tag_up = touch->tag;
            if (touch->tag_up) touch->evt |= EVE_TOUCH_ETYPE_TAG_UP;
            touch->tag = touch_tag;
            if (touch->tag) touch->evt |= EVE_TOUCH_ETYPE_TAG;
        }
        if (touch_xy != 0x80008000) {
            char _track = touch->tracker.tag && !touch->tracker.track;
            char _timer = _touch_timer.tag && (_touch_timer.evt & EVE_TOUCH_ETYPE_TIMER_MASK) && (i == 0);
            if (_track || _timer) {
                int dx = touch->x - touch->x0;
                int dy = touch->y - touch->y0;
                dx = dx < 0 ? -dx : dx;
                dy = dy < 0 ? -dy : dy;
                if (_track) {
                    if ((dx > EVE_THRESHOLD_X) && !(_tag_opt[touch->tracker.tag] & EVE_TOUCH_OPT_TRACK_X)) {
                        touch->tracker.tag = 0;
                    }
                    if ((dy > EVE_THRESHOLD_Y) && !(_tag_opt[touch->tracker.tag] & EVE_TOUCH_OPT_TRACK_Y)) {
                        touch->tracker.tag = 0;
                    }
                    if (touch->tracker.tag && ((dx > EVE_THRESHOLD_X) || (dy > EVE_THRESHOLD_Y))) {
                        if (dx > EVE_THRESHOLD_X) {
                            touch->evt |= touch->x > touch->x0 ? EVE_TOUCH_ETYPE_TRACK_RIGHT : EVE_TOUCH_ETYPE_TRACK_LEFT;
                        }
                        if (dy > EVE_THRESHOLD_Y) {
                            touch->evt |= touch->y > touch->y0 ? EVE_TOUCH_ETYPE_TRACK_DOWN : EVE_TOUCH_ETYPE_TRACK_UP;
                        }
                        touch->tracker.track = 1;
                        touch->evt |= EVE_TOUCH_ETYPE_TRACK_START;
                        touch->t = now;
                    }
                }
                if (_timer && ((dx > EVE_THRESHOLD_X) || (dy > EVE_THRESHOLD_Y))) {
                    _touch_timer.evt &= ~EVE_TOUCH_ETYPE_TIMER_MASK;
                    if (!_touch_timer.evt) _touch_timer_clear();
                }
            }
            if (touch->tracker.tag && touch->tracker.track) {
                touch->evt |= _tag_opt[touch->tracker.tag] & EVE_TOUCH_OPT_TRACK_MASK;
            }
            if (touch->evt & EVE_TOUCH_ETYPE_TRACK_REG) {
                uint32_t touch_track = eve_read32(_reg_track[i]);
                if (touch->tracker.tag == (touch_track & 0xff)) {
                    touch->tracker.val = touch_track >> 16;
                } else {
                    touch->evt &= ~EVE_TOUCH_ETYPE_TRACK_REG;
                }
            }
            if (touch->tracker.tag || _touch_timer.tag) int_ccomplete = 1;
        }
        if (touch->evt & EVE_TOUCH_EVT_MASK) touch_last = i + 1;
        if (!_multitouch) break;
    }

    if (!touch_ex) {
        _tag0 = 0;
        _multitouch = 0;
    }

    if (_multitouch) int_ccomplete = 1;

    if (int_ccomplete && !(_intr_mask & EVE_INT_CONVCOMPLETE)) {
        _intr_mask |= EVE_INT_CONVCOMPLETE;
        eve_write8(REG_INT_MASK, _intr_mask);
    }
    if (!int_ccomplete && (_intr_mask & EVE_INT_CONVCOMPLETE)) {
        _intr_mask &= ~EVE_INT_CONVCOMPLETE;
        eve_write8(REG_INT_MASK, _intr_mask);
    }

    for (i=0; i<touch_last; i++) {
        EVETouch *touch = &_touch[i];
        if (_touch_handler && (touch->evt & EVE_TOUCH_EVT_MASK)) {
            _touch_handler(_touch_handler_param, tag0, i);
        }
    }
}

void eve_handle_time(void) {
    if (_touch_handler && _touch_timer.tag) {
        EVETouch *touch = &_touch[_touch_timer.idx];

        if ((_touch_timer.evt & EVE_TOUCH_ETYPE_TAP1) && (touch->x != EVE_NOTOUCH)) _touch_timer.evt &= ~EVE_TOUCH_ETYPE_TAP1;

        if (_touch_timer.evt) {
            int more = 0;
            int _x = touch->x;
            int _y = touch->y;

            _touch_evt_clear(touch);
            touch->evt |= _touch_timer.evt;
            if (touch->evt & EVE_TOUCH_ETYPE_TRACK) {
                if (_ext_tracker.tick) more = _ext_tracker.tick(&_touch_timer, touch);
                if (more) {
                    eve_timer_set(EVE_TIMEOUT_TRACK);
                } else {
                    touch->evt |= EVE_TOUCH_ETYPE_TRACK_STOP;
                    if (_ext_tracker.stop) _ext_tracker.stop(&_touch_timer, touch);
                }
            }

            _touch_handler(_touch_handler_param, _touch_timer.tag, _touch_timer.idx);

            if (!more) _touch_timer_clear();
            touch->x = _x;
            touch->y = _y;
        }
    }
}

void eve_touch_init(void) {
    int i;

    for (i=0; i<EVE_MAX_TOUCH; i++) {
        EVETouch *touch = &_touch[i];
        touch->x = EVE_NOTOUCH;
        touch->y = EVE_NOTOUCH;
    }
    eve_write8(REG_INT_MASK, _intr_mask);
    eve_write8(REG_INT_EN, 0x01);
    while(eve_read8(REG_INT_FLAGS));
}

void eve_touch_set_handler(eve_touch_handler_t handler, void *param) {
    _touch_handler = handler;
    _touch_handler_param = param;
}

EVETouch *eve_touch_evt(uint8_t tag0, int touch_idx, uint8_t tag_min, uint8_t tag_n, uint16_t *evt) {
    int tag_max;
    uint8_t _tag;
    uint16_t _evt;
    EVETouch *ret = NULL;

    *evt = 0;

    if ((touch_idx < 0) || (touch_idx > 4)) return ret;
    if (tag_min == EVE_TAG_NOTAG) return ret;

    tag_max = tag_min + tag_n;
    if ((tag0 < tag_min) || (tag0 >= tag_max)) return ret;

    ret = &_touch[touch_idx];
    _evt = ret->evt;

    *evt |= _evt & EVE_TOUCH_ETYPE_POINT_MASK;
    if (_evt & EVE_TOUCH_ETYPE_TAG) {
        _tag = ret->tag;
        if ((_tag >= tag_min) && (_tag < tag_max)) *evt |= EVE_TOUCH_ETYPE_TAG;
    }
    if (_evt & EVE_TOUCH_ETYPE_TAG_UP) {
        _tag = ret->tag_up;
        if ((_tag >= tag_min) && (_tag < tag_max)) *evt |= EVE_TOUCH_ETYPE_TAG_UP;
    }
    if (_evt & EVE_TOUCH_ETYPE_TRACK_REG) {
        _tag = ret->tracker.tag;
        if ((_tag >= tag_min) && (_tag < tag_max)) *evt |= EVE_TOUCH_ETYPE_TRACK_REG;
    }
    if (_evt & EVE_TOUCH_ETYPE_TRACK_MASK) {
        _tag = ret->tracker.tag;
        if ((_tag >= tag_min) && (_tag < tag_max)) *evt |= _evt & (EVE_TOUCH_ETYPE_TRACK_MASK | EVE_TOUCH_ETYPE_TRACK_XY);
    }
    if (_evt & (EVE_TOUCH_ETYPE_LPRESS | EVE_TOUCH_ETYPE_TAP1 | EVE_TOUCH_ETYPE_TAP2)) {
        _tag = _touch_timer.tag;
        if ((_tag >= tag_min) && (_tag < tag_max)) *evt |= _evt & (EVE_TOUCH_ETYPE_LPRESS | EVE_TOUCH_ETYPE_TAP1 | EVE_TOUCH_ETYPE_TAP2);
    }

    return ret;
}

void eve_touch_set_opt(uint8_t tag, uint8_t opt) {
    _tag_opt[tag] = opt;
}

uint8_t eve_touch_get_opt(uint8_t tag) {
    return _tag_opt[tag];
}

void eve_touch_clear_opt(void) {
    memset(_tag_opt, 0, sizeof(_tag_opt));
}

EVETouchTimer *eve_touch_get_timer(void) {
    return &_touch_timer;
}

void eve_etrack_set(eve_etrack_init_t init, eve_etrack_tick_t tick, eve_etrack_stop_t stop, void *param) {
    _ext_tracker.init = init;
    _ext_tracker.tick = tick;
    _ext_tracker.stop = stop;
    _touch_timer.p = param;
}

void eve_etrack_start(int i) {
    EVETouch *touch = &_touch[i];

    _touch_timer_set(touch->tracker.tag, i, EVE_TOUCH_ETYPE_TRACK, touch->x, touch->y, EVE_TIMEOUT_TRACK);
    if (_ext_tracker.init) _ext_tracker.init(&_touch_timer, touch);
}

void eve_etrack_stop(void) {
    if (_ext_tracker.stop) {
        EVETouch *touch = &_touch[_touch_timer.idx];

        _touch_evt_clear(touch);
        touch->evt |= EVE_TOUCH_ETYPE_TRACK_STOP;
        _ext_tracker.stop(&_touch_timer, touch);
    }
    _touch_timer_clear();
}
