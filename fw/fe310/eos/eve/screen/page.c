#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"
#include "unicode.h"

#include "screen.h"
#include "window.h"
#include "view.h"
#include "page.h"

#include "widget/font.h"
#include "widget/label.h"
#include "widget/widget.h"

#define CH_EOF              0x1a

void eve_page_init(EVEPage *page, EVEWindow *window, EVEViewStack *stack, eve_view_touch_t touch, eve_view_draw_t draw, eve_page_destructor_t destructor) {
    memset(page, 0, sizeof(EVEPage));
    eve_view_init(&page->v, window, touch, draw, NULL);
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
    EVEScreen *screen = window->screen;
    EVEViewStack *stack = page->stack;
    eve_page_destructor_t destructor = page->destructor;

    if (stack->level > 1) {
        if (destructor) destructor(page);
        eve_screen_hide_kbd(screen);
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
        EVEScreen *screen = page->v.window->screen;
        EVEWidget *widget_f = page->widget_f;

        if (widget_f && widget_f->putc) {
            eve_screen_hide_kbd(screen);
            widget_f->putc(page, CH_EOF);
        }
        if (widget && widget->putc) {
            EVEKbd *kbd = eve_screen_get_kbd(screen);

            if (kbd) eve_kbd_set_handler(kbd, widget->putc, page);
            eve_screen_show_kbd(screen);
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
