#include <stdint.h>

#define EVE_MAX_TILES   8

struct EVETile;

typedef struct EVEWindow {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
} EVEWindow;

typedef struct EVEScreen {
    uint16_t w;
    uint16_t h;
    EVEKbd *kbd;
    struct EVETile *tile[EVE_MAX_TILES];
    uint8_t tile_size;
    char kbd_active;
} EVEScreen;

int eve_screen_init(EVEScreen *screen, uint16_t w, uint16_t h);
void eve_screen_set_kbd(EVEScreen *screen, EVEKbd *kbd) ;
EVEKbd *eve_screen_get_kbd(EVEScreen *screen);
void eve_screen_show_kbd(EVEScreen *screen);
void eve_screen_hide_kbd(EVEScreen *screen);

void eve_screen_add_tile(EVEScreen *screen, struct EVETile *tile);
void eve_screen_handle_touch(EVEScreen *screen, uint8_t tag0, int touch_idx);
