#include <stdint.h>

typedef struct EVEStrCursor {
    char on;
    uint16_t x;
    uint16_t ch;
} EVEStrCursor;

typedef struct EVEStrWidget {
    EVEWidget w;
    EVEFont *font;
    utf8_t *str;
    uint16_t str_size;
    uint16_t str_len;
    struct {
        int16_t x;
        int16_t x0;
        uint16_t w;
    } str_g;
    EVEStrCursor cursor1;
    EVEStrCursor cursor2;
    struct {
        EVEStrCursor *cursor;
        short dx;
        char mode;
    } track;
} EVEStrWidget;

typedef struct EVEStrSpec {
    EVEFont *font;
    uint16_t str_size;
} EVEStrSpec;

int eve_strw_create(EVEStrWidget *widget, EVERect *g, EVEPage *page, EVEStrSpec *spec);
void eve_strw_init(EVEStrWidget *widget, EVERect *g, EVEPage *page, EVEFont *font, utf8_t *str, uint16_t str_size);
void eve_strw_destroy(EVEStrWidget *widget);
int eve_strw_update(EVEStrWidget *widget);

uint8_t eve_strw_draw(EVEWidget *_widget, uint8_t tag0);
int eve_strw_touch(EVEWidget *_widget, EVETouch *touch, uint16_t evt);
void eve_strw_putc(void *_page, int c);
void eve_strw_cursor_set(EVEStrWidget *widget, EVEStrCursor *cursor, int16_t x);
void eve_strw_cursor_clear(EVEStrWidget *widget, EVEStrCursor *cursor);