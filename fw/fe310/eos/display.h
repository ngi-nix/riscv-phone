#include <stdint.h>

int eos_disp_select(void);
void eos_disp_deselect(void);
void eos_disp_cs_set(void);
void eos_disp_cs_clear(void);
void eos_disp_write(uint8_t dc, uint8_t data);
void eos_disp_read(uint8_t *data);