#include <stdint.h>

struct EVEView;

typedef int (*eve_view_touch_t) (struct EVEView *, uint8_t, int);
typedef uint8_t (*eve_view_draw_t) (struct EVEView *, uint8_t);

typedef struct EVEView {
    eve_view_touch_t touch;
    eve_view_draw_t draw;
} EVEView;

typedef struct EVEWindow {
    EVERect g;
    EVEView *view;
    EVEScreen *screen;
    struct EVEWindow *next;
} EVEWindow;

void eve_window_init(EVEWindow *window, EVERect *g, EVEView *view, EVEScreen *screen);
int eve_window_visible(EVEWindow *window);
void eve_window_visible_g(EVEWindow *window, EVERect *g);
