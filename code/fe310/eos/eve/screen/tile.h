#include <stdint.h>

struct EVECanvas;

typedef int (*eve_canvas_touch_t) (struct EVECanvas *, uint8_t, int);
typedef uint8_t (*eve_canvas_draw_t) (struct EVECanvas *, uint8_t);

typedef struct EVECanvas {
    eve_canvas_touch_t touch;
    eve_canvas_draw_t draw;
} EVECanvas;

typedef struct EVETile {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    EVECanvas *canvas;
    EVEScreen *screen;
} EVETile;

void eve_tile_init(EVETile *tile, EVEScreen *screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, EVECanvas *canvas);
void eve_tile_get_pos(EVETile *tile, uint16_t *x, uint16_t *y, uint16_t *w, uint16_t *h);
void eve_canvas_init(EVECanvas *canvas, eve_canvas_touch_t touch, eve_canvas_draw_t draw);
