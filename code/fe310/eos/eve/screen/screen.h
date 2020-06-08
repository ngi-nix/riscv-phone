#include <stdint.h>

struct EVEWindow;

typedef struct EVERect {
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
} EVERect;

typedef struct EVEScreen {
    uint16_t w;
    uint16_t h;
    struct EVEWindow *win_head;
    struct EVEWindow *win_tail;
    EVEKbd *kbd;
    char kbd_active;
} EVEScreen;

int eve_screen_init(EVEScreen *screen, uint16_t w, uint16_t h);
void eve_screen_set_kbd(EVEScreen *screen, EVEKbd *kbd) ;
EVEKbd *eve_screen_get_kbd(EVEScreen *screen);
void eve_screen_show_kbd(EVEScreen *screen);
void eve_screen_hide_kbd(EVEScreen *screen);

int eve_screen_win_insert(EVEScreen *screen, struct EVEWindow *window, int idx);
int eve_screen_win_remove(EVEScreen *screen, struct EVEWindow *window);
void eve_screen_win_append(EVEScreen *screen, struct EVEWindow *window);

void eve_screen_handle_touch(EVEScreen *screen, uint8_t tag0, int touch_idx);
