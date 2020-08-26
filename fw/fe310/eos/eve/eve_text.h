#include <stdint.h>

typedef struct EVEText {
    EVERect g;
    uint16_t w;
    uint16_t h;
    uint32_t mem_addr;
    uint16_t line_size;
    uint16_t line0;
    int line_top;
    int line_top0;
    uint16_t ch_idx;
    uint16_t transform_a;
    uint16_t transform_e;
    uint8_t tag;
    uint8_t ch_h;
    uint8_t ch_w;
    uint8_t dl_size;
    char dirty;
} EVEText;

void eve_text_init(EVEText *box, EVERect *g, uint16_t w, uint16_t h, uint16_t line_size, uint32_t mem_addr, uint32_t *mem_next);
void eve_text_update(EVEText *box);
void eve_text_scroll0(EVEText *box);

int eve_text_touch(EVEText *box, uint8_t tag0, int touch_idx);
uint8_t eve_text_draw(EVEText *box, uint8_t tag);

void eve_text_putc(EVEText *box, int c);
void eve_text_backspace(EVEText *box);
void eve_text_newline(EVEText *box);
void eve_text_puts(EVEText *box, char *s);
