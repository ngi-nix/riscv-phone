#include <stdlib.h>
#include <stdio.h>

#include "eos.h"

#include "eve.h"
#include "eve_platform.h"

void *eve_malloc(size_t size) {
    void *p = malloc(size);
    printf("MALLOC:%p %d\n", p, size);
    return p;
}

void eve_free(void *p) {
    printf("FREE:%p\n", p);
    free(p);
}

void eve_timer_set(uint32_t ms) {
    eos_timer_set(ms, EOS_TIMER_ETYPE_UI);
}

void eve_time_sleep(uint32_t ms) {
    eos_time_sleep(ms);
}

uint64_t eve_time_get_tick(void) {
    return eos_time_get_tick();
}

void eve_timer_clear(void) {
    eos_timer_clear(EOS_TIMER_ETYPE_UI);
}

void eve_spi_start(void) {
    eos_spi_select(EOS_SPI_DEV_EVE);
}

void eve_spi_stop(void) {
    eos_spi_deselect();
}
