#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <esp_system.h>
#include <esp_event.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_err.h>
#include <driver/spi_slave.h>

#include "fe310.h"
#include "i2c.h"
#include "modem.h"
#include "pcm.h"
#include "transport.h"
#include "bq25895.h"

// Main application
void app_main() {
    eos_i2c_init();
    eos_modem_init();
    eos_pcm_init();
    eos_wifi_init();
    eos_fe310_init();
    eos_bq25895_set_ilim();
}


