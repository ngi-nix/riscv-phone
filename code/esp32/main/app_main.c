#include <tcpip_adapter.h>
#include <driver/gpio.h>

#include "i2c.h"
#include "cell.h"
#include "_net.h"
#include "wifi.h"
#include "sock.h"
#include "power.h"
#include "bq25895.h"

#define ESP_INTR_FLAG_DEFAULT   0

// Main application
void app_main() {
    tcpip_adapter_init();

    eos_pcm_init();
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    eos_modem_init();

    eos_net_init();
    eos_cell_init();
    eos_wifi_init();
    eos_sock_init();

    eos_power_init();

    eos_i2c_init();
    eos_bq25895_set_ilim();
}


