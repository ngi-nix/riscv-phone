# RISC-V Phone

The goal of the this project is to develop a privacy enhanced, simple and fully featured mobile phone.

**Technical specs**:

  - CPU: SiFive Freedom E310 (FE310-G002) microcontroller
  - WiFi/BT: Espressif ESP32
  - Graphics: BT815 Display/Touch Controller
  - Baseband: Mini PCIe module (SIMCom SIM7600X or Quectel EC-25)
  - Audio: MAX98357A Class D amplifier / ICS-43434 MEMS Microphone
  - Accelerometer: LSM9DS1 - gyro, accel, magnetometer
  - Haptic driver: DRV2605L
  - NXP i.MX8M System-on-Module daughterboard

##1. Building

Follow theese instructions to build both FE310 and ESP32 firmware.

###1.1 Install prerequisites

You will need the following software available on your machine:

  - git
  - GNU Make
  - RISC-V GNU Toolchain
  - RISC-V OpenOCD
  - ESP-IDF from Espressif

Binary RISC-V GNU Toolchain and OpenOCD are available from SiFive at [https://github.com/sifive/freedom-tools/releases](https://github.com/sifive/freedom-tools/releases)

Download and unpack the archives for your platform and export `RISCV_PATH` and `RISCV_OPENOCD_PATH` variables:

```
export RISCV_PATH=/path/to/riscv64-gcc
export RISCV_OPENOCD_PATH=/path/to/riscv-openocd
```

To install ESP-IDF follow instructions from [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html#installation-step-by-step). Install version 4.2 or later.

###1.2 Obtain source

This repository can be cloned by running the following command:

```
git clone git://majstor.org/rvPhone.git
```

Repository contains following directories:

  - `hw/rvPhone`: KiCad v5 project that contains schematics and board layout
  - `fw/fe310`: Firmware for Freedom FE310-G002 SoC
  - `fw/esp32`: Firmware for Espressif ESP32 SoC

###1.3 Build
To build firmware for FE310 run follwing commands:

```
cd <path to repository>/fw/fe310
make
cd test
make upload
```

To build firmware for ESP32 run following commands:

```
cd <path to repository>/fw/esp32
make menuconfig
make
make flash
```

In `make menuconfig`, please make sure that `Enable PPP support` is selected under `Component config -> LWIP` menu and that `Notify Phase Callback`, `PAP Support` and `CHAP Support` are selected under PPP menu.

##2. License

Hardware licensed under the [CERN OHL v1.2](https://ohwr.org/project/licenses/wikis/cern-ohl-v1.2)

Software licensed under the [GPLv2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

##3. Acknowledgments

This project is supported by the [NGI Zero PET](https://nlnet.nl/PET) project, a fund established by [NLnet](https://nlnet.nl) foundation with financial support from the European Commission's [Next Generation Internet](https://ngi.eu/) programme, under the aegis of DG Communications Networks, Content and Technology. NGI Zero PET project has received funding from the European Union's [Horizon 2020](https://ec.europa.eu/programmes/horizon2020/) research and innovation programme under grant agreement No 825310.