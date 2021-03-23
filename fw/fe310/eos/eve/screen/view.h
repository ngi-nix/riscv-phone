#include <stdint.h>

#define EVE_VIEW_SIZE_STACK     16

struct EVEView;
struct EVEViewStack;

typedef int (*eve_view_touch_t) (struct EVEView *, EVETouch *, uint16_t, uint8_t);
typedef uint8_t (*eve_view_draw_t) (struct EVEView *, uint8_t);
typedef void (*eve_view_constructor_t) (EVEWindow *window, struct EVEViewStack *);

typedef struct EVEView {
    eve_view_touch_t touch;
    eve_view_draw_t draw;
    EVEWindow *window;
    void *param;
} EVEView;

typedef struct EVEViewStack {
    eve_view_constructor_t constructor[EVE_VIEW_SIZE_STACK];
    uint8_t level;
} EVEViewStack;

void eve_view_init(EVEView *view, EVEWindow *window, eve_view_touch_t touch, eve_view_draw_t draw, void *param);
void eve_view_stack_init(EVEViewStack *stack);
void eve_view_create(EVEWindow *window, EVEViewStack *stack, eve_view_constructor_t constructor);
void eve_view_destroy(EVEWindow *window, EVEViewStack *stack);