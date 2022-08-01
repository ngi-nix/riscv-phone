{
  inputs.flake-utils.url = "github:numtide/flake-utils";

  #outputs = { self, nixpkgs, flake-utils, ... }:
  #    flake-utils.lib.eachDefaultSystem (system:
  #      let
  #        pkgs = nixpkgs.legacyPackages.${system};
  #        riscvToolchain = pkgs.stdenv.mkDerivation {
  #          name = "riscv-toolchain";
  #          dontPatchELF = true;
  #          fetchSubmodules = true;
  #          src = pkgs.fetchFromGitHub {
  #            owner = "riscv-collab";
  #            repo = "riscv-gnu-toolchain";
  #            rev = "409b951ba6621f2f115aebddfb15ce2dd78ec24f";
  #            sha256 = "xY02I6fAsAvZvJA6Sn94FOIPCK/ulk5aN0XmOPqa27Q=";
  #            leaveDotGit = true;
  #          };
  #          esp32 = final.stdenv.mkDerivation {
  #            IDF_PATH = (builtins.getFlake "github:mirrexagon/nixpkgs-esp-dev/13ba39c98aa1a87384e35eef2e646f9b8521229a").packages.${final.hostPlatform.system}.esp-idf;
  #            name = "esp32";
  #            src = ./fw/esp32;
  #          };
  #          configureFlags = ["--with-arch=rv64g"];
  #          buildInputs = with pkgs; [ autoconf automake curl python39 libmpc mpfr gmp gawk bison flex texinfo gperf libtool patchutils bc zlib expat git flock ];
  #        };          
  #      in
  #        {
  #          devShell = pkgs.mkShell {
  #            buildInputs = with pkgs; [
  #              riscvToolchain
  #              #mkspiffs-presets.esp-idf
  #              openocd
  #            ];
  #            shellHook = ''
  #              export RISCV_PATH=${riscvToolchain}
  #              export RISCV_OPENOCD_PATH=${pkgs.openocd}
  #            '';
  #          };
  #        }
  #    );


  outputs = { self, nixpkgs, flake-utils, ... }:
   flake-utils.lib.eachDefaultSystem (system:
     let
       riscv-toolchain =
         import nixpkgs {
           localSystem = "${system}";
           crossSystem = {
             #config = "riscv64-none-elf";
             config = "riscv64-unknown-linux-gnu";
             abi = "ilp32";
           };
         };
     in
       with import <nixpkgs> {};
       {
         devShell = pkgs.mkShell {
           buildInputs = with pkgs; [
             riscv-toolchain.buildPackages.gcc
             openocd
             mkspiffs-presets.esp-idf              
           ];           
           shellHook = ''
             RISCV_HOME=${riscv-toolchain.buildPackages.gcc}
             RISCV_OPENOCD_PATH=${pkgs.openocd}
           '';
         };
       }
   );
}
