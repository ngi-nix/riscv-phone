#include "i2c.h"
#include "modem.h"
#include "pcm.h"
#include "_net.h"
#include "sock.h"
#include "wifi.h"

// Main application
void app_main() {
    eos_i2c_init();
    eos_modem_init();
    eos_pcm_init();
    eos_net_init();
    eos_wifi_init();
    eos_sock_init();
    // eos_bq25895_set_ilim();
}


