#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen.h"
#include "window.h"

#define MIN(X, Y)           (((X) < (Y)) ? (X) : (Y))

void eve_window_init(EVEWindow *window, EVERect *g, EVEScreen *screen, char *name) {
    memset(window, 0, sizeof(EVEWindow));

    if (g) window->g = *g;
    window->screen = screen;
    window->name = name;
    window->color_fg = 0xffffff;
}

void eve_window_set_color_bg(EVEWindow *window, uint8_t r, uint8_t g, uint8_t b) {
    window->color_bg = (r << 16) | (g << 8) | b;
}

void eve_window_set_color_fg(EVEWindow *window, uint8_t r, uint8_t g, uint8_t b) {
    window->color_fg = (r << 16) | (g << 8) | b;
}

int eve_window_visible(EVEWindow *window) {
    if (window->g.x >= window->screen->w) return 0;
    if (window->g.y >= window->screen->h) return 0;
    if ((window->g.x + window->g.w) <= 0) return 0;
    if ((window->g.y + window->g.h) <= 0) return 0;
    return 1;
}

void eve_window_visible_g(EVEWindow *window, EVERect *g) {
    EVEWindow *w = window->next;

    *g = window->g;
    while (w) {
        if (eve_window_visible(w)) {
            if (w->g.x > g->x) g->w = MIN(g->w, w->g.x - g->x);
            if (w->g.y > g->y) g->h = MIN(g->h, w->g.y - g->y);
            if (w->g.x + w->g.w < g->x + g->w) {
                uint16_t x0 = g->w - MIN(g->w, (g->x + g->w) - (w->g.x + w->g.w));
                g->x += x0;
                g->w -= x0;
            }
            if (w->g.y + w->g.h < g->y + g->h) {
                uint16_t y0 = g->h - MIN(g->h, (g->y + g->h) - (w->g.y + w->g.h));
                g->y += y0;
                g->h -= y0;
            }
        }
        w = w->next;
    }
}

void eve_window_append(EVEWindow *window) {
    EVEScreen *screen = window->screen;

    window->prev = screen->win_tail;
    if (screen->win_tail) {
        screen->win_tail->next = window;
    } else {
        screen->win_head = window;
    }
    screen->win_tail = window;
}

void eve_window_insert_above(EVEWindow *window, EVEWindow *win_prev) {
    EVEScreen *screen = window->screen;

    window->prev = win_prev;
    window->next = win_prev->next;

    if (window->next) {
        window->next->prev = window;
    } else {
        screen->win_tail = window;
    }
    win_prev->next = window;
}

void eve_window_insert_below(EVEWindow *window, EVEWindow *win_next) {
    EVEScreen *screen = window->screen;

    window->prev = win_next->prev;
    window->next = win_next;

    win_next->prev = window;
    if (window->prev) {
        window->prev->next = window;
    } else {
        screen->win_head = window;
    }
}

void eve_window_remove(EVEWindow *window) {
    EVEScreen *screen = window->screen;

    if (window->prev) {
        window->prev->next = window->next;
    } else {
        screen->win_head = window->next;
    }
    if (window->next) {
        window->next->prev = window->prev;
    } else {
        screen->win_tail = window->prev;
    }
}

EVEWindow *eve_window_get(EVEScreen *screen, char *name) {
    EVEWindow *w = screen->win_head;

    while (w) {
        if (strcmp(name, w->name) == 0) return w;
        w = w->next;
    }

    return NULL;
}
