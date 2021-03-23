#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "eve.h"

static int _intr_mask = EVE_INT_TAG | EVE_INT_TOUCH;
static int _multitouch;
static uint8_t _tag0;

static EVETouch _touch[EVE_MAX_TOUCH];
static EVETouchTimer _touch_timer;

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

void eve_handle_touch(void) {
    int i;
    char touch_ex = 0;
    char int_ccomplete = 0;
    uint8_t flags;

    eve_spi_start();

    flags = eve_read8(REG_INT_FLAGS) & _intr_mask;
    if (!_multitouch && (flags & EVE_INT_TOUCH)) _multitouch = 1;
    for (i=0; i<EVE_MAX_TOUCH; i++) {
        uint8_t touch_tag;
        uint32_t touch_xy;
        uint64_t now = 0;
        uint16_t touch_evt = 0;
        EVETouch *touch = &_touch[i];

        touch_xy = i < 4 ? eve_read32(_reg_touch[i]) : (((uint32_t)eve_read16(REG_CTOUCH_TOUCH4_X) << 16) | eve_read16(REG_CTOUCH_TOUCH4_Y));

        if (touch_xy != EVE_NOTOUCH) {
            int16_t touch_x = touch_xy >> 16;
            int16_t touch_y = touch_xy & 0xffff;
            now = eve_time_get_tick();
            if (touch->eevt & EVE_TOUCH_EETYPE_NOTOUCH) {
                uint16_t _evt = 0;
                uint16_t _eevt = 0;
                uint16_t _ttevt = eve_touch_timer_get_evt(touch);

                if (_ttevt) {
                    touch->eevt &= ~EVE_TOUCH_EETYPE_NOTOUCH;

                    if (_ttevt & EVE_TOUCH_ETYPE_TAP2) {
                        int dx = touch_x - touch->x0;
                        int dy = touch_y - touch->y0;

                        dx = dx < 0 ? -dx : dx;
                        dy = dy < 0 ? -dy : dy;
                        if ((dx > EVE_TOUCH_THRESHOLD_X) || (dy > EVE_TOUCH_THRESHOLD_Y)) {
                            touch_evt |= EVE_TOUCH_ETYPE_TAP1;
                        } else {
                            _evt |= EVE_TOUCH_ETYPE_TAP2;
                            _eevt |= EVE_TOUCH_EETYPE_TAP2;
                        }
                    }
                    if (_ttevt & EVE_TOUCH_ETYPE_TRACK) {
                        EVEVTrack *vtrack = eve_vtrack_get();

                        _eevt |= EVE_TOUCH_EETYPE_TRACK_ABORT;
                        touch_evt |= (EVE_TOUCH_ETYPE_TRACK_STOP | EVE_TOUCH_ETYPE_TRACK_ABORT);
                        if (vtrack->stop) vtrack->stop(touch, vtrack->param);
                    }
                    if (_ttevt & EVE_TOUCH_ETYPE_TIMER) {
                        _eevt |= EVE_TOUCH_EETYPE_TIMER_ABORT;
                        touch_evt |= EVE_TOUCH_ETYPE_TIMER_ABORT;
                    }

                    eve_touch_timer_clear(touch);
                    if (_touch_handler && touch_evt) {
                        _touch_handler(_touch_timer.touch, touch_evt, _touch_timer.tag0, _touch_handler_param);
                    }
                }
                touch_evt = EVE_TOUCH_ETYPE_POINT | _evt;
                touch->eevt = _eevt;
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
                touch->vx = touch->vx ? (vx + touch->vx * EVE_TOUCH_TRAVG) / (EVE_TOUCH_TRAVG + 1) : vx;
                touch->vy = touch->vy ? (vy + touch->vy * EVE_TOUCH_TRAVG) / (EVE_TOUCH_TRAVG + 1) : vy;
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
            if (!(touch->eevt & EVE_TOUCH_EETYPE_NOTOUCH)) {
                uint16_t _ttevt = eve_touch_timer_get_evt(touch);

                touch_evt = EVE_TOUCH_ETYPE_POINT_UP;
                touch->eevt |= EVE_TOUCH_EETYPE_NOTOUCH;
                if (_ttevt & EVE_TOUCH_ETYPE_LPRESS) {
                    eve_touch_timer_set_evt(touch, _ttevt & ~EVE_TOUCH_ETYPE_LPRESS);
                }
                if (touch->tracker.tag && touch->tracker.track) {
                    uint8_t opt = _tag_opt[touch->tracker.tag];
                    char track_ext = ((opt & EVE_TOUCH_OPT_TRACK_EXT_X) && (touch->eevt & EVE_TOUCH_EETYPE_TRACK_X)) ||
                                     ((opt & EVE_TOUCH_OPT_TRACK_EXT_Y) && (touch->eevt & EVE_TOUCH_EETYPE_TRACK_Y));
                    if (!eve_touch_timer_get_evt(NULL) && track_ext) {
                        EVEVTrack *vtrack = eve_vtrack_get();

                        eve_touch_timer_set(touch, EVE_TOUCH_ETYPE_TRACK, _tag0, EVE_TOUCH_TIMEOUT_TRACK);
                        if (vtrack->start) vtrack->start(touch, vtrack->param);
                    } else {
                        touch_evt |= EVE_TOUCH_ETYPE_TRACK_STOP;
                    }
                }
            }
        }
        if (touch_tag != touch->tag) {
            if (touch_tag) {
                if (!_tag0) _tag0 = touch_tag;
                if (!touch->tag0) {
                    touch->tag0 = touch_tag;
                    if (_tag_opt[touch_tag] & (EVE_TOUCH_OPT_TRACK | EVE_TOUCH_OPT_TRACK_REG)) {
                        touch->tracker.tag = touch_tag;
                    }
                    if (touch->tracker.tag && !(_tag_opt[touch->tracker.tag] & EVE_TOUCH_OPT_TRACK_XY)) {
                        touch_evt |= EVE_TOUCH_ETYPE_TRACK_START;
                        touch->tracker.track = 1;
                        touch->t = now;
                    }
                    if (!eve_touch_timer_get_evt(NULL) && (_tag_opt[touch_tag] & (EVE_TOUCH_OPT_LPRESS | EVE_TOUCH_OPT_TAP2))) {
                        uint16_t _evt = 0;

                        if (_tag_opt[touch_tag] & EVE_TOUCH_OPT_LPRESS) _evt |= EVE_TOUCH_ETYPE_LPRESS;
                        if (_tag_opt[touch_tag] & EVE_TOUCH_OPT_TAP2) _evt |= EVE_TOUCH_ETYPE_TAP2;
                        eve_touch_timer_set(touch, _evt, _tag0, EVE_TOUCH_TIMEOUT_TAP);
                    }
                }
            }
            touch->tag_up = touch->tag;
            if (touch->tag_up) touch_evt |= EVE_TOUCH_ETYPE_TAG_UP;
            touch->tag = touch_tag;
            if (touch->tag) touch_evt |= EVE_TOUCH_ETYPE_TAG;
        }
        if (touch_xy != EVE_NOTOUCH) {
            uint16_t _ttevt = eve_touch_timer_get_evt(touch);
            int _track = touch->tracker.tag && !touch->tracker.track;
            int _timer = _ttevt & (EVE_TOUCH_ETYPE_LPRESS | EVE_TOUCH_ETYPE_TAP2);
            if (_track || _timer) {
                int dx = touch->x - touch->x0;
                int dy = touch->y - touch->y0;
                dx = dx < 0 ? -dx : dx;
                dy = dy < 0 ? -dy : dy;
                if (_track) {
                    if ((dx > EVE_TOUCH_THRESHOLD_X) && !(_tag_opt[touch->tracker.tag] & EVE_TOUCH_OPT_TRACK_X)) {
                        touch->tracker.tag = 0;
                    }
                    if ((dy > EVE_TOUCH_THRESHOLD_Y) && !(_tag_opt[touch->tracker.tag] & EVE_TOUCH_OPT_TRACK_Y)) {
                        touch->tracker.tag = 0;
                    }
                    if (touch->tracker.tag && ((dx > EVE_TOUCH_THRESHOLD_X) || (dy > EVE_TOUCH_THRESHOLD_Y))) {
                        if (dx > EVE_TOUCH_THRESHOLD_X) {
                            touch->eevt |= touch->x > touch->x0 ? EVE_TOUCH_EETYPE_TRACK_RIGHT : EVE_TOUCH_EETYPE_TRACK_LEFT;
                        }
                        if (dy > EVE_TOUCH_THRESHOLD_Y) {
                            touch->eevt |= touch->y > touch->y0 ? EVE_TOUCH_EETYPE_TRACK_DOWN : EVE_TOUCH_EETYPE_TRACK_UP;
                        }
                        touch_evt |= EVE_TOUCH_ETYPE_TRACK_START;
                        touch->tracker.track = 1;
                        touch->t = now;
                    }
                }
                if (_timer && ((dx > EVE_TOUCH_THRESHOLD_X) || (dy > EVE_TOUCH_THRESHOLD_Y))) {
                    eve_touch_timer_set_evt(touch, _ttevt & ~(EVE_TOUCH_ETYPE_LPRESS | EVE_TOUCH_ETYPE_TAP2));
                    _timer = 0;
                }
            }
            if (touch->tracker.tag && touch->tracker.track) {
                if (_tag_opt[touch->tracker.tag] & EVE_TOUCH_OPT_TRACK) touch_evt |= EVE_TOUCH_ETYPE_TRACK;
                if (_tag_opt[touch->tracker.tag] & EVE_TOUCH_OPT_TRACK_REG) touch_evt |= EVE_TOUCH_ETYPE_TRACK_REG;
            }
            if (touch_evt & EVE_TOUCH_ETYPE_TRACK_REG) {
                uint32_t touch_track = eve_read32(_reg_track[i]);
                if (touch->tracker.tag == (touch_track & 0xff)) {
                    touch->tracker.val = touch_track >> 16;
                } else {
                    touch_evt &= ~EVE_TOUCH_ETYPE_TRACK_REG;
                }
            }
            if (touch->tracker.tag || _timer) int_ccomplete = 1;
        }
        if (_touch_handler && touch_evt) {
            _touch_handler(touch, touch_evt, _tag0, _touch_handler_param);
        }
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

    eve_spi_stop();
}

void eve_handle_time(void) {
    EVETouch *touch = _touch_timer.touch;

    if (_touch_timer.evt) {
        int more = 0;
        uint16_t touch_evt = 0;

        eve_spi_start();

        if (_touch_timer.evt & EVE_TOUCH_ETYPE_LPRESS) {
            touch_evt |= EVE_TOUCH_ETYPE_LPRESS;
            if (touch) touch->eevt |= EVE_TOUCH_EETYPE_LPRESS;
        }
        if (_touch_timer.evt & EVE_TOUCH_ETYPE_TAP2) {
            touch_evt |= EVE_TOUCH_ETYPE_TAP1;
            if (touch) touch->eevt |= EVE_TOUCH_EETYPE_TAP1;
        }
        if (_touch_timer.evt & EVE_TOUCH_ETYPE_TRACK) {
            EVEVTrack *vtrack = eve_vtrack_get();

            if (vtrack->tick) {
                touch_evt |= EVE_TOUCH_ETYPE_TRACK;
                more = vtrack->tick(touch, vtrack->param);
            }
            if (!more) {
                touch_evt |= EVE_TOUCH_ETYPE_TRACK_STOP;
                if (vtrack->stop) vtrack->stop(touch, vtrack->param);
            }
        }
        if (_touch_timer.evt & EVE_TOUCH_ETYPE_TIMER) {
            touch_evt |= EVE_TOUCH_ETYPE_TIMER;
            more = 1;
        }

        if (more) {
            eve_timer_set(_touch_timer.to);
        } else {
            _touch_timer.evt = 0;
        }

        if (_touch_handler && touch_evt) {
            _touch_handler(touch, touch_evt, _touch_timer.tag0, _touch_handler_param);
        }

        eve_spi_stop();
    }
}

void eve_touch_init(void) {
    int i;

    eve_vtrack_init();

    for (i=0; i<EVE_MAX_TOUCH; i++) {
        EVETouch *touch = &_touch[i];
        touch->eevt |= EVE_TOUCH_EETYPE_NOTOUCH;
    }

    eve_write8(REG_INT_MASK, _intr_mask);
    eve_write8(REG_INT_EN, 0x01);
    while(eve_read8(REG_INT_FLAGS));
}

void eve_touch_set_handler(eve_touch_handler_t handler, void *param) {
    _touch_handler = handler;
    _touch_handler_param = param;
}

EVETouch *eve_touch_get(int i) {
    return &_touch[i];
}

int8_t eve_touch_get_idx(EVETouch *touch) {
    if (touch == NULL) return -1;
    return touch - _touch;
}

uint16_t eve_touch_evt(EVETouch *touch, uint16_t evt, uint8_t tag0, uint8_t tag_min, uint8_t tag_n) {
    int tag_max;
    uint8_t _tag;
    uint16_t _evt;

    if (tag_min == EVE_TAG_NOTAG) return 0;

    tag_max = tag_min + tag_n;
    if ((tag0 < tag_min) || (tag0 >= tag_max)) return 0;

    _evt = evt & (EVE_TOUCH_ETYPE_TIMER_MASK | EVE_TOUCH_ETYPE_USR_MASK);
    if (touch == NULL) return _evt;

    _evt |= evt & EVE_TOUCH_ETYPE_POINT_MASK;
    if (evt & EVE_TOUCH_ETYPE_TAG) {
        _tag = touch->tag;
        if ((_tag >= tag_min) && (_tag < tag_max)) _evt |= EVE_TOUCH_ETYPE_TAG;
    }
    if (evt & EVE_TOUCH_ETYPE_TAG_UP) {
        _tag = touch->tag_up;
        if ((_tag >= tag_min) && (_tag < tag_max)) _evt |= EVE_TOUCH_ETYPE_TAG_UP;
    }
    if (evt & EVE_TOUCH_ETYPE_TRACK_MASK) {
        _tag = touch->tracker.tag;
        if ((_tag >= tag_min) && (_tag < tag_max)) _evt |= evt & EVE_TOUCH_ETYPE_TRACK_MASK;
    }
    if (evt & (EVE_TOUCH_ETYPE_LPRESS | EVE_TOUCH_ETYPE_TAP1 | EVE_TOUCH_ETYPE_TAP2)) {
        _tag = touch->tag0;
        if ((_tag >= tag_min) && (_tag < tag_max)) _evt |= evt & (EVE_TOUCH_ETYPE_LPRESS | EVE_TOUCH_ETYPE_TAP1 | EVE_TOUCH_ETYPE_TAP2);
    }

    return _evt;
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

void eve_touch_timer_set(EVETouch *touch, uint16_t evt, uint8_t tag0, uint32_t to) {
    _touch_timer.touch = touch;
    _touch_timer.evt = evt;
    _touch_timer.tag0 = tag0;
    _touch_timer.to = to;
    eve_timer_set(to);
}

void eve_touch_timer_clear(EVETouch *touch) {
    eve_touch_timer_set_evt(touch, 0);
}

uint16_t eve_touch_timer_get_evt(EVETouch *touch) {
    uint16_t ret = 0;

    if ((touch == NULL) || (_touch_timer.touch == touch)) {
        ret = _touch_timer.evt;
    } else if (_touch_timer.touch == NULL) {
        ret = _touch_timer.evt & EVE_TOUCH_ETYPE_TIMER;
    }
    return ret;
}

void eve_touch_timer_set_evt(EVETouch *touch, uint16_t evt) {
    if (touch == _touch_timer.touch) {
        _touch_timer.evt = evt;
    } else if (_touch_timer.touch == NULL) {
        _touch_timer.evt = evt & EVE_TOUCH_ETYPE_TIMER;
    }
    if (!_touch_timer.evt) eve_timer_clear();
}

void eve_touch_timer_start(uint8_t tag0, uint32_t to) {
    eve_touch_timer_set(NULL, EVE_TOUCH_ETYPE_TIMER, tag0, to);
}

void eve_touch_timer_stop(void) {
    eve_touch_timer_clear(NULL);
}

EVETouchTimer *eve_touch_timer_get(void) {
    return &_touch_timer;
}
