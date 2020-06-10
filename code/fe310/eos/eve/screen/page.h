#include <stdint.h>

struct EVEPage;
struct EVEWidget;

typedef void (*eve_page_open_t) (struct EVEPage *, struct EVEPage *);
typedef void (*eve_page_close_t) (struct EVEPage *);
typedef void (*eve_page_evt_handler_t) (struct EVEPage *, struct EVEWidget *, EVETouch *, uint16_t, uint8_t, int);
typedef void (*eve_page_g_updater_t) (struct EVEPage *, struct EVEWidget *);

typedef struct EVEPage {
    EVEView v;
    int16_t win_x;
    int16_t win_y;
    eve_page_open_t open;
    eve_page_close_t close;
    eve_page_evt_handler_t handle_evt;
    eve_page_g_updater_t update_g;
    struct EVEWidget *widget_f;
    EVEWindow *window;
} EVEPage;

void eve_page_init(EVEPage *page, eve_view_touch_t touch, eve_view_draw_t draw, eve_page_open_t open, eve_page_close_t close, eve_page_evt_handler_t handle_evt, eve_page_g_updater_t update_g, EVEWindow *window);
void eve_page_set_focus(EVEPage *page, struct EVEWidget *widget, EVERect *focus);
struct EVEWidget *eve_page_get_focus(EVEPage *page);
int eve_page_rect_visible(EVEPage *page, EVERect *g);
