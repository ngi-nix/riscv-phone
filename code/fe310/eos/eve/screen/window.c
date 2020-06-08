#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen.h"
#include "window.h"

#define MIN(X, Y)           (((X) < (Y)) ? (X) : (Y))

void eve_window_init(EVEWindow *window, EVERect *g, EVEView *view, EVEScreen *screen) {
    memset(window, 0, sizeof(EVEWindow));

    if (g) window->g = *g;
    window->view = view;
    window->screen = screen;
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
