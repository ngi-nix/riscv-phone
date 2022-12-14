# RISC-V Phone

This flake packages the firmware for the Freedom E310 and the Espressif ESP32 SoCs.

It is currently based against $HEAD of upstream for the [source files](http://majstor.org/gitweb/?p=rvPhone.git;a=tree).

If anything breaks, you can always pin the `riscvphone-src` input to a working revision:

```
url = "git://majstor.org/rvPhone.git?rev=38e19a65fda7a37688320b4b732eb1113bbcbad7";
```
You may find more information about this project [here](http://majstor.org/rvphone/) and [here](http://majstor.org/rvphone/build.html).

For further details, please refer to the documentation in the flake itself.


## Building

To build the firmware for each SoC (and possibly also flash it), type `nix build .#fe310` or `nix build .#esp32`.


## Developing

You can enter a developer shell by typing `nix develop .#fe310` or `nix develop .#esp32`. 

Navigate into `fw/fe310` or `fw/esp32` and type `make` to compile. Typing `make clean` will delete the compiled files.

When typing `make` in `fw/esp32`, a configuration menu will open if no `sdkconfig` configuration file exists, even if empty. You can manually choose the configuration values in this menu at any time by typing `make menuconfig` or pass them automatically via the [sdkconfig.defaults](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html#using-sdkconfig-defaults) file.
