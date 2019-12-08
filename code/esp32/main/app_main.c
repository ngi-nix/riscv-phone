#include "i2c.h"
#include "cell.h"
#include "_net.h"
#include "wifi.h"
#include "sock.h"
#include "bq25895.h"

// Main application
void app_main() {
    eos_i2c_init();
    eos_net_init();
    eos_pcm_init();
    eos_modem_init();
    eos_wifi_init();
    eos_sock_init();
    eos_bq25895_set_ilim();
}


