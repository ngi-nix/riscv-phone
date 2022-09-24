# Todo

- ~~Generate sdkconfig programatically (see [this](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html#using-sdkconfig-defaults)) using defaults to override values `PPP support`, `Notify Phase Callback`, `PAP Support` and `CHAP Support` (as stated [here](http://majstor.org/rvphone/build.html), at the bottom).~~ &#x2714;
- ~~`make` now generates a full `sdkconfig` configuration file with the defaults passed in `sdkconfig.defaults`, but still enters the ncurses menu. Figure out a way to override the menu altogether.~~ &#x2714;
- ~~Add instructions pertaining the developer shell and derivation for the ESP32 SoC to the README file.~~ &#x2714;
- Merge mob/main branch into main

## Links

<https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html#step-5-start-a-project>

<http://majstor.org/rvphone/build.html>
