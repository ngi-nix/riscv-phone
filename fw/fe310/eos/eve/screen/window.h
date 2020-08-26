#include <stdint.h>

struct EVEView;
struct EVEWindow;

typedef struct EVEWindow {
    EVERect g;
    EVEScreen *screen;
    char *name;
    struct EVEView *view;
    struct EVEWindow *next;
    struct EVEWindow *prev;
    uint32_t color_bg;
    uint32_t color_fg;
    uint8_t tag;
} EVEWindow;

void eve_window_init(EVEWindow *window, EVERect *g, EVEScreen *screen, char *name);
void eve_window_set_color_bg(EVEWindow *window, uint8_t r, uint8_t g, uint8_t b);
void eve_window_set_color_fg(EVEWindow *window, uint8_t r, uint8_t g, uint8_t b);

int eve_window_visible(EVEWindow *window);
void eve_window_visible_g(EVEWindow *window, EVERect *g);

void eve_window_append(EVEWindow *window);
void eve_window_insert_above(EVEWindow *window, EVEWindow *win_prev);
void eve_window_insert_below(EVEWindow *window, EVEWindow *win_next);
void eve_window_remove(EVEWindow *window);
EVEWindow *eve_window_get(EVEScreen *screen, char *name);
