{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/c11d08f02390aab49e7c22e6d0ea9b176394d961";
  inputs.nixpkgs-esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";

  outputs = { self, nixpkgs, nixpkgs-esp-dev }:
    let
      system = "x86_64-linux";

      pkgs = import nixpkgs { inherit system; overlays = [ (import "${nixpkgs-esp-dev}/overlay.nix") ]; };

      esp32-toolchain = with pkgs; pkgs.stdenv.mkDerivation {
        name = "riscv-esp32-firmware";
        src = ./.;
        nativeBuildInputs = with pkgs; [
          gcc-xtensa-esp32-elf-bin
          openocd-esp32-bin
          esp-idf
        ];
        buildInputs = with pkgs; [
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
        preBuild = ''
          export IDF_PATH=$(pwd)/esp-idf
        '';

      };

    in
    {


}
