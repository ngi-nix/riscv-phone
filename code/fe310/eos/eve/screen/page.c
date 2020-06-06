#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen.h"
#include "window.h"
#include "page.h"

#include "widget/widget.h"

void eve_page_init(EVEPage *page, eve_view_touch_t touch, eve_view_draw_t draw, eve_page_open_t open, eve_page_close_t close, EVEWindow *window) {
    memset(page, 0, sizeof(EVEPage));
    page->v.touch = touch;
    page->v.draw = draw;
    page->open = open;
    page->close = close;
    page->handle_evt = eve_page_handle_evt;
    page->window = window;
}

void eve_page_focus(EVEPage *page, EVERect *f) {
    EVERect g;

    eve_window_get_visible(page->window, &g);
    g.x -= page->window->g.x;
    g.y -= page->window->g.y;

    if (f->x < page->win_x + g.x) {
        page->win_x = f->x - g.x;
    }
    if (f->y < page->win_y + g.y) {
        page->win_y = f->y - g.y;
    }
    if ((f->x + f->w) > (page->win_x + g.x + g.w)) {
        page->win_x = (f->x + f->w) - (g.x + g.w);
    }
    if ((f->y + f->h) > (page->win_y + g.y + g.h)) {
        page->win_y = (f->y + f->h) - (g.y + g.h);
    }
}

int eve_page_widget_visible(EVEPage *page, EVEWidget *widget) {
    uint16_t w = page->window->g.w;
    uint16_t h = page->window->g.h;

    if (((widget->g.x + widget->g.w) >= page->win_x) && ((widget->g.y + widget->g.h) >= page->win_y) && (widget->g.x <= (page->win_x + w)) && (widget->g.y <= (page->win_y + h))) return 1;
    return 0;
}

void eve_page_handle_evt(EVEPage *page, EVEWidget *widget, EVETouch *touch, uint16_t evt, uint8_t tag0, int touch_idx) {
    /*
    if (evt & EVE_TOUCH_ETYPE_TRACK_Y) {
        // do scroll
    } else {
        // go back / forward
    }
    */
}

