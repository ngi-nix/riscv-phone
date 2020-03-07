#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen.h"
#include "tile.h"
#include "page.h"

#include "widget/widget.h"

void eve_page_init(EVEPage *page, EVETile *tile, eve_canvas_touch_t touch, eve_canvas_draw_t draw, eve_page_open_t open, eve_page_close_t close) {
    memset(page, 0, sizeof(EVEPage));
    page->tile = tile;
    page->handle_evt = eve_page_handle_evt;
}

void eve_page_focus(EVEPage *page, EVEPageFocus *focus) {
    uint16_t tile_w, tile_h;

    if (focus->w == NULL) return;
    eve_tile_get_pos(page->tile, NULL, NULL, &tile_w, &tile_h);

    if (focus->f.x < page->win_x) {
        page->win_x -= page->win_x - focus->f.x;
    }
    if (focus->f.y < page->win_y) {
        page->win_y -= page->win_y - focus->f.y;
    }
    if ((focus->f.x + focus->f.w) > (page->win_x + tile_w)) {
        page->win_x += (focus->f.x + focus->f.w) - (page->win_x + tile_w);
    }
    if ((focus->f.y + focus->f.h) > (page->win_y + tile_h)) {
        page->win_y += (focus->f.y + focus->f.h) - (page->win_y + tile_h);
    }
}

void eve_page_widget_focus(EVEPageFocus *focus, EVEWidget *widget) {
    focus->w = widget;
    focus->f.x = widget->x;
    focus->f.y = widget->y;
    focus->f.w = widget->w;
    focus->f.h = widget->h;
}

int eve_page_widget_visible(EVEPage *page, EVEWidget *widget) {
    uint16_t tile_w, tile_h;
    eve_tile_get_pos(page->tile, NULL, NULL, &tile_w, &tile_h);

    if (((widget->x + widget->w) >= page->win_x) && ((widget->y + widget->h) >= page->win_y) && (widget->x <= (page->win_x + tile_w)) && (widget->y <= (page->win_y + tile_h))) return 1;
    return 0;
}

void eve_page_handle_evt(EVEPage *page, struct EVEWidget *widget, EVETouch *touch, uint16_t evt, uint8_t tag0, int touch_idx) {
    /*
    if (evt & EVE_TOUCH_ETYPE_TRACK_Y) {
        // do scroll
    } else {
        // go back / forward
    }
    */
}

