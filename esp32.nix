{ bison
, cmake
, esp-idf
, esptool
, flex
, gcc-xtensa-esp32-elf-bin
, git
, gnumake
, gperf
, ncurses5
, ninja
, nixpkgs-esp-dev
, openocd-esp32-bin
, pkgconfig
, wget
, riscvphone-src
, stdenv
, sdkconfig-defaults ? ./sdkconfig.defaults
}:

stdenv.mkDerivation
{
  name = "esp32";
  src = riscvphone-src;
  dontUseCmakeConfigure = true;
  dontUseNinjaBuild = true;
  dontUseNinjaInstall = true;
  dontUseNinjaCheck = true;

  PROJECT_VER = riscvphone-src.rev;
  LC_ALL = "C";
  LC_CTYPE = "C.UTF-8";

  configurePhase = ''
    cd fw/esp32

    # generate an empty sdkconfig file to override the menu prompt.
    touch sdkconfig

    # copy custom configuration defaults to pass to sdkconfig.
    cat ${sdkconfig-defaults} > sdkconfig.defaults
  '';

  installPhase = ''
    #mkdir -p $out && cp -r . $out
    make flash
  '';

  nativeBuildInputs = [
    nixpkgs-esp-dev.packages.x86_64-linux.esp-idf
    nixpkgs-esp-dev.packages.x86_64-linux.gcc-xtensa-esp32-elf-bin
    nixpkgs-esp-dev.packages.x86_64-linux.openocd-esp32-bin
    esp-idf
    gcc-xtensa-esp32-elf-bin
    openocd-esp32-bin
    esptool
  ];

  buildInputs = [
    nixpkgs-esp-dev.packages.x86_64-linux.esp-idf
    git
    nixpkgs-esp-dev.packages.x86_64-linux.gcc-xtensa-esp32-elf-bin
    nixpkgs-esp-dev.packages.x86_64-linux.openocd-esp32-bin
    esptool
    git
    wget
    gnumake
    flex
    bison
    gperf
    pkgconfig
    cmake
    ninja
    ncurses5
  ];
}
