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
#include "transport.h"

// Main application
void app_main() {
    eos_fe310_init();
    eos_net_init();
}


