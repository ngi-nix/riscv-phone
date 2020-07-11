#include <stdint.h>

struct EVEWindow;

typedef struct EVEScreen {
    uint16_t w;
    uint16_t h;
    uint32_t mem_next;
    struct EVEWindow *win_head;
    struct EVEWindow *win_tail;
    EVEKbd *kbd;
} EVEScreen;

int eve_screen_init(EVEScreen *screen, uint16_t w, uint16_t h);
void eve_screen_set_kbd(EVEScreen *screen, EVEKbd *kbd);
EVEKbd *eve_screen_get_kbd(EVEScreen *screen);
void eve_screen_show_kbd(EVEScreen *screen);
void eve_screen_hide_kbd(EVEScreen *screen);

void eve_screen_draw(EVEScreen *screen);
void eve_screen_handle_touch(void *s, uint8_t tag0, int touch_idx);
