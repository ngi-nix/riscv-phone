#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen.h"
#include "window.h"
#include "page.h"
#include "font.h"

#include "widget/label.h"
#include "widget/widget.h"

void eve_page_init(EVEPage *page, eve_view_touch_t touch, eve_view_draw_t draw, eve_page_open_t open, eve_page_close_t close, eve_page_evt_handler_t handle_evt, eve_page_g_updater_t update_g, EVEWindow *window) {
    memset(page, 0, sizeof(EVEPage));
    page->v.touch = touch;
    page->v.draw = draw;
    page->open = open;
    page->close = close;
    page->handle_evt = handle_evt;
    page->update_g = update_g;
    page->widget_f = NULL;
    page->window = window;
}

int16_t eve_page_x(EVEPage *page, int16_t x) {
    return x + page->win_x - page->window->g.x;
}

int16_t eve_page_y(EVEPage *page, int16_t y) {
    return y + page->win_y - page->window->g.y;
}

int16_t eve_page_scrx(EVEPage *page, int16_t x) {
    return x - page->win_x + page->window->g.x;
}

int16_t eve_page_scry(EVEPage *page, int16_t y) {
    return y - page->win_y + page->window->g.y;
}

void eve_page_set_focus(EVEPage *page, EVEWidget *widget, EVERect *f) {
    EVERect g;

    eve_window_visible_g(page->window, &g);
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
    page->widget_f = widget;
}

EVEWidget *eve_page_get_focus(EVEPage *page) {
    return page->widget_f;
}

int eve_page_rect_visible(EVEPage *page, EVERect *g) {
    uint16_t w = page->window->g.w;
    uint16_t h = page->window->g.h;

    if (((g->x + g->w) >= page->win_x) && ((g->y + g->h) >= page->win_y) && (g->x <= (page->win_x + w)) && (g->y <= (page->win_y + h))) return 1;
    return 0;
}
