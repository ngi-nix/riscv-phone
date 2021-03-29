#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"
#include "eve_font.h"

#include "window.h"
#include "page.h"

#include "widget/label.h"
#include "widget/widget.h"

#define PAGE_TMODE_NONE     0
#define PAGE_TMODE_TRACK    1
#define PAGE_TMODE_SCROLL   2

void eve_page_init(EVEPage *page, EVEWindow *window, EVEViewStack *stack, EVEWidget *widget, uint16_t widget_size, uint8_t opt, eve_view_draw_t draw, eve_view_touch_t touch, eve_view_uievt_t uievt, eve_page_destructor_t destructor) {
    memset(page, 0, sizeof(EVEPage));
    eve_view_init(&page->v, window, draw, touch, uievt, NULL);
    page->stack = stack;
    page->opt = opt;
    page->destructor = destructor;
    eve_page_update(page, widget, widget_size);
}

void eve_page_update(EVEPage *page, EVEWidget *widget, uint16_t widget_size) {
    page->widget = widget;
    page->widget_size = widget_size;
}

void eve_page_open(EVEPage *parent, eve_view_constructor_t constructor) {
    EVEWindow *window = parent->v.window;
    EVEViewStack *stack = parent->stack;
    eve_page_destructor_t destructor = parent->destructor;

    if (destructor) destructor(parent);
    eve_view_create(window, stack, constructor);
}

void eve_page_close(EVEPage *page) {
    EVEWindow *window = page->v.window;
    EVEViewStack *stack = page->stack;
    eve_page_destructor_t destructor = page->destructor;

    if (stack->level <= 1) return;

    if (page->lho_t0) {
        page->lho_t0 = 0;
        eve_touch_timer_stop();
    }
    if (eve_window_scroll(window->root, NULL) == window) {
        eve_window_scroll_stop(window);
    }

    if (destructor) destructor(page);
    eve_window_kbd_detach(window);
    eve_view_destroy(window, stack);
}

/* Screen to page coordinates */
int16_t eve_page_x(EVEPage *page, int16_t x) {
    return x + page->g.x - page->v.window->g.x;
}

int16_t eve_page_y(EVEPage *page, int16_t y) {
    return y + page->g.y - page->v.window->g.y;
}

/* Page to window coordinates */
int16_t eve_page_win_x(EVEPage *page, int16_t x) {
    return x - page->g.x;
}

int16_t eve_page_win_y(EVEPage *page, int16_t y) {
    return y - page->g.y;
}

/* Page to screen coordinates */
int16_t eve_page_scr_x(EVEPage *page, int16_t x) {
    return eve_page_win_x(page, x) + page->v.window->g.x;
}

int16_t eve_page_scr_y(EVEPage *page, int16_t y) {
    return eve_page_win_y(page, y) + page->v.window->g.y;
}

int eve_page_rect_visible(EVEPage *page, EVERect *g) {
    uint16_t w = page->v.window->g.w;
    uint16_t h = page->v.window->g.h;

    if (((g->x + g->w) >= page->g.x) && ((g->y + g->h) >= page->g.y) && (g->x <= (page->g.x + w)) && (g->y <= (page->g.y + h))) return 1;
    return 0;
}

void eve_page_focus(EVEPage *page, EVERect *rect) {
    if (rect) {
        EVERect g;

        eve_window_visible_g(page->v.window, &g);
        g.x -= page->v.window->g.x;
        g.y -= page->v.window->g.y;

        if (rect->x < page->g.x + g.x) {
            page->g.x = rect->x - g.x;
        }
        if (rect->y < page->g.y + g.y) {
            page->g.y = rect->y - g.y;
        }
        if ((rect->x + rect->w) > (page->g.x + g.x + g.w)) {
            page->g.x = (rect->x + rect->w) - (g.x + g.w);
        }
        if ((rect->y + rect->h) > (page->g.y + g.y + g.h)) {
            page->g.y = (rect->y + rect->h) - (g.y + g.h);
        }
    }
}

void eve_page_focus_widget(EVEPage *page, EVEWidget *widget, EVERect *rect) {
    if (page->widget_f != widget) {
        EVEWindow *window = page->v.window;
        EVEWidget *widget_f = page->widget_f;

        if (widget_f && widget_f->putc) {
            widget_f->putc(widget_f, EVE_PAGE_KBDCH_CLOSE);
            if (!(widget && widget->putc)) eve_window_kbd_detach(window);
        }
        if (widget && widget->putc) {
            EVEKbd *kbd = eve_window_kbd(window);

            if (kbd) eve_kbd_set_handler(kbd, widget->putc, widget);
            if (!(widget_f && widget_f->putc)) eve_window_kbd_attach(window);
        }
        page->widget_f = widget;
    }
    if (rect) eve_page_focus(page, rect);
}

EVEWidget *eve_page_focus_widget_get(EVEPage *page) {
    return page->widget_f;
}

EVEWidget *eve_page_widget(EVEPage *page, uint16_t idx) {
    EVEWidget *w = page->widget;
    int i;

    if (idx >= page->widget_size) return NULL;

    for (i=0; i<idx; i++) {
        w = eve_widget_next(w);
    }
    return w;
}

static int page_touch(EVEPage *page, EVETouch *touch, uint16_t evt, uint8_t tag0) {
    EVEView *view = &page->v;
    EVEWindow *window = view->window;
    int scroll, scroll_x, scroll_y;
    int ret = 0;

    scroll_x = page->opt & (EVE_PAGE_OPT_SCROLL_X | EVE_PAGE_OPT_SCROLL_XY);
    if (scroll_x && touch) scroll_x = touch->eevt & ((page->opt & EVE_PAGE_OPT_SCROLL_XY) ? EVE_TOUCH_EETYPE_TRACK_XY : EVE_TOUCH_EETYPE_TRACK_X);

    scroll_y = page->opt & (EVE_PAGE_OPT_SCROLL_Y | EVE_PAGE_OPT_SCROLL_XY);
    if (scroll_y && touch) scroll_y = touch->eevt & ((page->opt & EVE_PAGE_OPT_SCROLL_XY) ? EVE_TOUCH_EETYPE_TRACK_XY : EVE_TOUCH_EETYPE_TRACK_Y);

    scroll = scroll_x || scroll_y;

    if ((evt & EVE_TOUCH_ETYPE_POINT_UP) && !(touch->eevt & (EVE_TOUCH_EETYPE_TRACK_XY | EVE_TOUCH_EETYPE_ABORT))) {
        int _ret = 0;

        if (page->widget_f) eve_page_focus_widget(page, NULL, NULL);
        _ret = eve_view_uievt_pusht(view, EVE_UIEVT_PAGE_TOUCH, touch, evt, tag0);
        if (_ret) return _ret;
        ret = 1;
    }

    /* Scroll start */
    if ((evt & EVE_TOUCH_ETYPE_TRACK_START) && !(touch->eevt & EVE_TOUCH_EETYPE_ABORT)) {
        int _ret = 0;

        if (scroll) {
            page->track_mode = PAGE_TMODE_SCROLL;
            eve_window_scroll_start(window, touch->tracker.tag);
            _ret = eve_view_uievt_pusht(view, EVE_UIEVT_PAGE_SCROLL_START, touch, evt, tag0);
        } else {
            page->track_mode = PAGE_TMODE_TRACK;
            _ret = eve_view_uievt_pusht(view, EVE_UIEVT_PAGE_TRACK_START, touch, evt, tag0);
        }
        if (_ret) return _ret;
        ret = 1;
    }

    /* Scroll stop */
    if (((evt & EVE_TOUCH_ETYPE_TRACK_STOP) && !(evt & EVE_TOUCH_ETYPE_TRACK_ABORT)) ||
        ((evt & EVE_TOUCH_ETYPE_POINT_UP) && (touch->eevt & EVE_TOUCH_EETYPE_ABORT) && !(touch->eevt & EVE_TOUCH_EETYPE_TRACK_XY))) {
        if ((page->track_mode == PAGE_TMODE_SCROLL) && (page->opt & EVE_PAGE_OPT_SCROLL_BACK)) {
            int wmax_x, wmax_y;
            int lho_x, lho_y;
            EVERect vg;

            eve_window_visible_g(page->v.window, &vg);
            wmax_x = page->g.w > vg.w ? page->g.w - vg.w : 0;
            wmax_y = page->g.h > vg.h ? page->g.h - vg.h : 0;
            lho_x = page->g.x < 0 ? 0 : wmax_x;
            lho_y = page->g.y < 0 ? 0 : wmax_y;
            if ((page->g.x < 0) || (page->g.x > wmax_x) ||
                (page->g.y < 0) || (page->g.y > wmax_y)) {
                EVEPhyLHO *lho = &page->lho;
                uint8_t _tag;

                eve_window_scroll(window->root, &_tag);

                eve_phy_lho_init(lho, lho_x, lho_y, 1000, 0.5, 0);
                eve_phy_lho_start(lho, page->g.x, page->g.y);
                page->lho_t0 = eve_time_get_tick();
                eve_touch_timer_start(_tag, 20);
            }
        }

        if (!page->lho_t0) {
            int _ret = 0;

            if (page->track_mode == PAGE_TMODE_SCROLL) {
                page->track_mode = PAGE_TMODE_NONE;
                eve_window_scroll_stop(window);
                _ret = eve_view_uievt_pusht(view, EVE_UIEVT_PAGE_SCROLL_STOP, touch, evt, tag0);
            } else if (!(touch->eevt & EVE_TOUCH_EETYPE_ABORT)) {
                page->track_mode = PAGE_TMODE_NONE;
                _ret = eve_view_uievt_pusht(view, EVE_UIEVT_PAGE_TRACK_STOP, touch, evt, tag0);
            }
            if (_ret) return _ret;
            ret = 1;
        }
    }

    if (page->track_mode == PAGE_TMODE_SCROLL) {
        if (evt & EVE_TOUCH_ETYPE_TRACK_START) {
            if (scroll_x) {
                page->x0 = page->g.x;
            }
            if (scroll_y) {
                page->y0 = page->g.y;
            }
        }
        if (evt & EVE_TOUCH_ETYPE_TRACK) {
            if (scroll_x) {
                page->g.x = page->x0 + touch->x0 - touch->x;
            }
            if (scroll_y) {
                page->g.y = page->y0 + touch->y0 - touch->y;
            }
            ret = 1;
        }
        if ((evt & EVE_TOUCH_ETYPE_TIMER) && (page->opt & EVE_PAGE_OPT_SCROLL_BACK)) {
            EVEPhyLHO *lho = &page->lho;
            int x, y, more;

            more = eve_phy_lho_tick(lho, eve_time_get_tick() - page->lho_t0, scroll_x ? &x : NULL, scroll_y ? &y : NULL);
            if (scroll_x) page->g.x = x;
            if (scroll_y) page->g.y = y;
            if (!more) {
                int _ret = 0;

                page->lho_t0 = 0;
                eve_touch_timer_stop();
                page->track_mode = PAGE_TMODE_NONE;
                eve_window_scroll_stop(window);
                _ret = eve_view_uievt_pusht(view, EVE_UIEVT_PAGE_SCROLL_STOP, touch, evt, tag0);
                if (_ret) return _ret;
            }
            ret = 1;
        }
        if (evt & EVE_TOUCH_EETYPE_TIMER_ABORT) {
            page->lho_t0 = 0;
        }
    }

    return ret;
}

uint8_t eve_page_draw(EVEView *view, uint8_t tag0) {
    EVEPage *page = (EVEPage *)view;
    EVEWidget *widget = page->widget;
    int i;
    uint8_t tagN = tag0;
    uint8_t tag_opt;

    tag_opt = EVE_TOUCH_OPT_TRACK | EVE_TOUCH_OPT_TRACK_XY;
    if (page->opt & EVE_PAGE_OPT_TRACK_EXT_X) tag_opt |= EVE_TOUCH_OPT_TRACK_EXT_X;
    if (page->opt & EVE_PAGE_OPT_TRACK_EXT_Y) tag_opt |= EVE_TOUCH_OPT_TRACK_EXT_Y;

    tag0 = eve_view_clear(view, tag0, tag_opt);
    tagN = tag0;
    eve_cmd_dl(SAVE_CONTEXT());
    eve_cmd_dl(VERTEX_FORMAT(0));
    eve_cmd_dl(VERTEX_TRANSLATE_X(eve_page_scr_x(page, 0) * 16));
    eve_cmd_dl(VERTEX_TRANSLATE_Y(eve_page_scr_y(page, 0) * 16));
    for (i=0; i<page->widget_size; i++) {
        if (widget->label && eve_page_rect_visible(page, &widget->label->g)) {
            eve_cmd_dl(TAG_MASK(0));
            eve_label_draw(widget->label);
            eve_cmd_dl(TAG_MASK(1));
        }
        if (eve_page_rect_visible(page, &widget->g)) {
            tagN = widget->draw(widget, tagN);
        }
        widget = eve_widget_next(widget);
    }

    eve_cmd_dl(RESTORE_CONTEXT());

    for (i=tag0; i<tagN; i++) {
        if (i != EVE_NOTAG) eve_touch_set_opt(i, eve_touch_get_opt(i) | tag_opt);
    }

    return tagN;
}

int eve_page_touch(EVEView *view, EVETouch *touch, uint16_t evt, uint8_t tag0) {
    EVEPage *page = (EVEPage *)view;
    EVEWidget *widget = page->widget;
    int8_t touch_idx = eve_touch_get_idx(touch);
    uint16_t _evt;
    int i, ret;

    if (touch_idx > 0) return 0;

    _evt = eve_touch_evt(touch, evt, tag0, page->v.tag, 1);
    if (_evt) {
        ret = page_touch(page, touch, _evt, tag0);
        if (ret) return 1;
    }
    for (i=0; i<page->widget_size; i++) {
        if (eve_page_rect_visible(page, &widget->g)) {
            _evt = eve_touch_evt(touch, evt, tag0, widget->tag0, widget->tagN - widget->tag0);
            if (_evt) {
                if (page->track_mode == PAGE_TMODE_NONE) {
                    ret = widget->touch(widget, touch, _evt);
                    if (ret) return 1;
                }
                ret = page_touch(page, touch, _evt, tag0);
                if (ret) return 1;
            }
        }
        widget = eve_widget_next(widget);
    }

    return 0;
}
