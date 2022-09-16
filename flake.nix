{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs";
  inputs.riscvphone-src = {
    url = "git://majstor.org/rvPhone.git";
    flake = false;
  };
  
  outputs = { self, nixpkgs, riscvphone-src }:
    let
      system = "x86_64-linux";

      pkgs = import nixpkgs { inherit system; inherit riscvphone-src; };
        
      riscv-toolchain = import nixpkgs {
        localSystem = system;
        crossSystem = {
          config = "riscv32-none-elf";
          libc = "newlib-nano";
          abi = "ilp32";
        };          
      };
      
      nanolibsPath = pkgs.writeShellApplication {
        name = "nanolibs-path";
        runtimeInputs = with nixpkgs; [
          riscv-toolchain.newlib-nano
        ];
        text = builtins.readFile ./nanolibs-script.sh;
      };
      
      fe310-drv = riscv-toolchain.stdenv.mkDerivation {
        name = "riscv-fe310-firmware";
        src = ./.;
        buildInputs = with pkgs; [
          riscv-toolchain.buildPackages.gcc
          nanolibsPath
          openocd
        ];
        BUILD_DIR = (placeholder "out") + "/build";
        configurePhase = ''
          mkdir -p $out/build/test
        '';
        buildPhase = ''
          export NANOLIBS_PATH=${riscv-toolchain.newlib-nano}/riscv32-none-elf/lib/*.a
          export RISCV_HOME=${riscv-toolchain.buildPackages.gcc}
          export RISCV_OPENOCD_HOME=${pkgs.openocd}
          ${nanolibsPath}/bin/nanolibs-path
          make -C fw/fe310
          make -C fw/fe310/test
          #make upload -C fw/fe310/test
        '';
        installPhase = ''
          cp -r fw/fe310/libeos.a $out/build
          find . -path fw/fe310/test -prune -o -name "*.o" -exec cp {} $out/build \;
          find fw/fe310/test -type f -name "*.o" -exec cp {} $out/build/test \;
        '';
      };

    in
      {
        packages = {
          x86_64-linux = {
            nanolibsPath = nanolibsPath;
            # usage: nix build .#fe310
            fe310 = fe310-drv;
          };
        };
      
        devShells = {
          # usage: nix develop .#fe310
          # compile: cd fw/fe310 --> make --> cd fw/fe310/test --> make or make upload
          # clean up: cd fw/fe310 make clean, fw/fe310/test make clean
          x86_64-linux.fe310 = pkgs.mkShell {
            src = "${riscvphone-src}";
            buildInputs = with pkgs; [
              riscv-toolchain.buildPackages.gcc
              openocd
            ];
            shellHook = ''
              export NANOLIBS_PATH=${riscv-toolchain.newlib-nano}/riscv32-none-elf/lib/*.a
              export RISCV_HOME=${riscv-toolchain.buildPackages.gcc}
              export RISCV_OPENOCD_HOME=${pkgs.openocd}
              nix run .#nanolibsPath
              cd $src
            '';
          };
        };
        
      };
}
