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
    uint8_t tag;
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

int eve_strw_create(EVEStrWidget *widget, EVERect *g, EVEStrSpec *spec);
void eve_strw_destroy(EVEStrWidget *widget);
void eve_strw_init(EVEStrWidget *widget, EVERect *g, EVEFont *font, utf8_t *str, uint16_t str_size);
void eve_strw_update(EVEStrWidget *widget, EVEFont *font, utf8_t *str, uint16_t str_size);

int eve_strw_touch(EVEWidget *_widget, EVEPage *page, uint8_t tag0, int touch_idx);
uint8_t eve_strw_draw(EVEWidget *_widget, EVEPage *page, uint8_t tag0);
void eve_strw_putc(void *_page, int c);
void eve_strw_cursor_set(EVEStrWidget *widget, EVEStrCursor *cursor, int16_t x);
void eve_strw_cursor_clear(EVEStrWidget *widget, EVEStrCursor *cursor);