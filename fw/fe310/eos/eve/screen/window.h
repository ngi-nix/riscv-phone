#include <stdint.h>

struct EVEView;
struct EVEWindow;

typedef int (*eve_view_touch_t) (struct EVEView *, uint8_t, int);
typedef uint8_t (*eve_view_draw_t) (struct EVEView *, uint8_t);

typedef struct EVEView {
    eve_view_touch_t touch;
    eve_view_draw_t draw;
    struct EVEWindow *window;
} EVEView;

typedef struct EVEWindow {
    EVERect g;
    EVEView *view;
    EVEScreen *screen;
    struct EVEWindow *next;
    struct EVEWindow *prev;
    uint32_t color_bg;
    uint32_t color_fg;
    uint8_t tag;
} EVEWindow;

void eve_window_init(EVEWindow *window, EVERect *g, EVEView *view, EVEScreen *screen);
void eve_window_set_color_bg(EVEWindow *window, uint8_t r, uint8_t g, uint8_t b);
void eve_window_set_color_fg(EVEWindow *window, uint8_t r, uint8_t g, uint8_t b);

int eve_window_visible(EVEWindow *window);
void eve_window_visible_g(EVEWindow *window, EVERect *g);

void eve_window_append(EVEWindow *window);
void eve_window_insert_above(EVEWindow *window, EVEWindow *win_prev);
void eve_window_insert_below(EVEWindow *window, EVEWindow *win_next);
void eve_window_remove(EVEWindow *window);
