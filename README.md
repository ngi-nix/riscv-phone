# RISC-V Phone

This Nix flake packages the firmware for the Freedom E310 and the Espressif ESP32 SoCs.

You will find more information on this project [here](http://majstor.org/rvphone/) and [here](http://majstor.org/rvphone/build.html).

This flake is currently based against $HEAD of upstream for the [source files](http://majstor.org/gitweb/?p=rvPhone.git;a=tree).

If anything breaks, you can always pin the `riscvphone-src` input to a working revision:

```
url = "git://majstor.org/rvPhone.git?rev=38e19a65fda7a37688320b4b732eb1113bbcbad7";
```

For further details, please refer to the documentation in the flake itself.


## Building

### Freedom E310 SoC

To build the firmware for Freedom E310 SoC type `nix build .#fe310`.

You will find the compiled files in the `result/build` directory.

The `result` directory can be safely deleted at any time.


## Developing

### Freedom E310 SoC

You can enter a developer shell by typing `nix develop .#fe310`.

To compile using the makefile at `fw/fe310`, cd into `src/fw/fe310` and type `make`. 

Typing `make clean` while in the same directory will delete the compiled files.
