#include <stdint.h>

int eos_eve_init(uint8_t wakeup_cause, uint8_t gpio_dir, int touch_calibrate, uint32_t *touch_matrix);
int eos_eve_run(uint8_t wakeup_cause);
void eos_eve_start(void);
void eos_eve_stop(void);
