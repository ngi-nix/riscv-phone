#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen.h"
#include "tile.h"

#define MIN(X, Y)           (((X) < (Y)) ? (X) : (Y))

void eve_tile_init(EVETile *tile, EVEScreen *screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, EVECanvas *canvas) {
    tile->screen = screen;
    tile->x = x;
    tile->y = y;
    tile->w = w;
    tile->h = h;
    tile->canvas = canvas;
}

void eve_tile_get_pos(EVETile *tile, uint16_t *x, uint16_t *y, uint16_t *w, uint16_t *h) {
    if (x) *x = tile->x;
    if (y) *y = tile->y;
    if (w) *w = tile->w;
    if (h) *h = MIN(tile->x + tile->h, tile->screen->h - (tile->screen->kbd && tile->screen->kbd_active ? tile->screen->kbd->h : 0));
}

void eve_canvas_init(EVECanvas *canvas, eve_canvas_touch_t touch, eve_canvas_draw_t draw) {
    canvas->touch = touch;
    canvas->draw = draw;
}

