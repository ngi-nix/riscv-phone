#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"
#include "eve_font.h"

#include "window.h"
#include "page.h"

#include "widget/label.h"
#include "widget/widget.h"

void eve_page_init(EVEPage *page, EVEWindow *window, EVEViewStack *stack, eve_view_draw_t draw, eve_view_touch_t touch, eve_view_uievt_t uievt, eve_page_destructor_t destructor) {
    memset(page, 0, sizeof(EVEPage));
    eve_view_init(&page->v, window, draw, touch, uievt, NULL);
    page->destructor = destructor;
    page->stack = stack;
    page->widget_f = NULL;
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

    if (stack->level > 1) {
        if (destructor) destructor(page);
        eve_window_kbd_detach(window);
        eve_view_destroy(window, stack);
    }
}

int16_t eve_page_x(EVEPage *page, int16_t x) {
    return x + page->win_x - page->v.window->g.x;
}

int16_t eve_page_y(EVEPage *page, int16_t y) {
    return y + page->win_y - page->v.window->g.y;
}

int16_t eve_page_scr_x(EVEPage *page, int16_t x) {
    return x - page->win_x + page->v.window->g.x;
}

int16_t eve_page_scr_y(EVEPage *page, int16_t y) {
    return y - page->win_y + page->v.window->g.y;
}

void eve_page_set_focus(EVEPage *page, EVEWidget *widget, EVERect *f) {
    if (page->widget_f != widget) {
        EVEWindow *window = page->v.window;
        EVEWidget *widget_f = page->widget_f;

        if (widget_f && widget_f->putc) {
            eve_window_kbd_detach(window);
            widget_f->putc(widget_f, EVE_PAGE_KBDCH_CLOSE);
        }
        if (widget && widget->putc) {
            EVEKbd *kbd = eve_window_kbd(window);

            if (kbd) {
                eve_kbd_set_handler(kbd, widget->putc, widget);
                eve_window_kbd_attach(window);
            }
        }
        page->widget_f = widget;
    }

    if (f) {
        EVERect g;

        eve_window_visible_g(page->v.window, &g);
        g.x -= page->v.window->g.x;
        g.y -= page->v.window->g.y;

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
}

EVEWidget *eve_page_get_focus(EVEPage *page) {
    return page->widget_f;
}

int eve_page_rect_visible(EVEPage *page, EVERect *g) {
    uint16_t w = page->v.window->g.w;
    uint16_t h = page->v.window->g.h;

    if (((g->x + g->w) >= page->win_x) && ((g->y + g->h) >= page->win_y) && (g->x <= (page->win_x + w)) && (g->y <= (page->win_y + h))) return 1;
    return 0;
}

void eve_page_uievt_push(EVEPage *page, uint16_t evt, void *param) {
    EVEView *view = &page->v;
    eve_view_uievt_push(view, evt, param ? param : page);
}
