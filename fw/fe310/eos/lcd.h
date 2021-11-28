#include <stdint.h>

int eos_lcd_init(uint8_t wakeup_cause);
int eos_lcd_select(void);
void eos_lcd_deselect(void);
void eos_lcd_cs_set(void);
void eos_lcd_cs_clear(void);
void eos_lcd_write(uint8_t dc, uint8_t data);
void eos_lcd_read(uint8_t *data);

int eos_lcd_sleep(void);
int eos_lcd_wake(void);