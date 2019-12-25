#include <stdint.h>

typedef struct EVEText {
    int x;
    int y;
    int w;
    int h;
    uint32_t buf_addr;
    int buf_line_h;
    int buf_idx;
    int line_idx;
    uint8_t bitmap_handle;
} EVEText;

void eos_eve_text_init(EVEText *box, int x, int y, int w, int h, uint8_t bitmap_handle, uint32_t mem_addr, int buf_line_h, uint32_t *mem_next);
void eos_eve_text_draw(EVEText *box);
void eos_eve_text_update(EVEText *box);
void eos_eve_text_putc(EVEText *box, int c);
void eos_eve_text_newline(EVEText *box);
