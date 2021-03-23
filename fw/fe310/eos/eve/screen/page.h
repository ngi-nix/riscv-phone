#include <stdint.h>

struct EVEWidget;
struct EVEPage;

typedef void (*eve_page_destructor_t) (struct EVEPage *);

typedef struct EVEPage {
    EVEView v;
    int win_x;
    int win_y;
    eve_page_destructor_t destructor;
    EVEViewStack *stack;
    struct EVEWidget *widget_f;
} EVEPage;

void eve_page_init(EVEPage *page, EVEWindow *window, EVEViewStack *stack, eve_view_touch_t touch, eve_view_draw_t draw, eve_page_destructor_t destructor);
void eve_page_open(EVEPage *parent, eve_view_constructor_t constructor);
void eve_page_close(EVEPage *page);

int16_t eve_page_x(EVEPage *page, int16_t x);
int16_t eve_page_y(EVEPage *page, int16_t y);
int16_t eve_page_scr_x(EVEPage *page, int16_t x);
int16_t eve_page_scr_y(EVEPage *page, int16_t y);

void eve_page_set_focus(EVEPage *page, struct EVEWidget *widget, EVERect *focus);
struct EVEWidget *eve_page_get_focus(EVEPage *page);
int eve_page_rect_visible(EVEPage *page, EVERect *g);
