#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen.h"
#include "tile.h"

int eve_screen_init(EVEScreen *screen, uint16_t w, uint16_t h) {
    memset(screen, 0, sizeof(EVEScreen));
    screen->w = w;
    screen->h = h;
}

void eve_screen_set_kbd(EVEScreen *screen, EVEKbd *kbd) {
    screen->kbd = kbd;
}

EVEKbd *eve_screen_get_kbd(EVEScreen *screen) {
    return screen->kbd;
}

void eve_screen_show_kbd(EVEScreen *screen) {
    screen->kbd_active = 1;
}

void eve_screen_hide_kbd(EVEScreen *screen) {
    screen->kbd_active = 0;
}

void eve_screen_add_tile(EVEScreen *screen, EVETile *tile) {
    if (screen->tile_size < EVE_MAX_TILES) {
        screen->tile[screen->tile_size] = tile;
        screen->tile_size++;
    }
}

void eve_screen_handle_touch(EVEScreen *screen, uint8_t tag0, int touch_idx) {
    eve_touch_clear_opt();
}


