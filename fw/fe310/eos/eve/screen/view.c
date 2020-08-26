#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen.h"
#include "window.h"
#include "view.h"

void eve_view_init(EVEView *view, EVEWindow *window, eve_view_touch_t touch, eve_view_draw_t draw, void *param) {
    view->touch = touch;
    view->draw = draw;
    view->param = param;
    view->window = window;
    window->view = view;
}

void eve_view_stack_init(EVEViewStack *stack) {
    memset(stack, 0, sizeof(EVEViewStack));
}

void eve_view_create(EVEWindow *window, EVEViewStack *stack, eve_view_constructor_t constructor) {
    if (stack->level < EVE_VIEW_SIZE_STACK - 1) {
        stack->constructor[stack->level] = constructor;
        stack->level++;
        constructor(window, stack);
    }
}

void eve_view_destroy(EVEWindow *window, EVEViewStack *stack) {
    if (stack->level > 1) {
        eve_view_constructor_t constructor;

        stack->level--;
        constructor = stack->constructor[stack->level - 1];
        constructor(window, stack);
    }
}
