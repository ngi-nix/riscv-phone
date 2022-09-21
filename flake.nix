{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/";
    nixpkgs-esp32.url = "github:NixOS/nixpkgs/c11d08f02390aab49e7c22e6d0ea9b176394d961";
    # Requires specific nixpkgs version due to: https://github.com/mirrexagon/nixpkgs-esp-dev/issues/10
    nixpkgs-esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
    riscvphone-src = {
      url = "git://majstor.org/rvPhone.git";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, nixpkgs-esp-dev, riscvphone-src, nixpkgs-esp32 }:
    let
      system = "x86_64-linux";

      pkgs = import nixpkgs-esp32 {
        inherit system;
        overlays = [
          (import "${nixpkgs-esp-dev}/overlay.nix")
          self.overlays.default
        ];
      };

      riscv-toolchain = import nixpkgs {
        localSystem = system;
        crossSystem = {
          config = "riscv64-none-elf";
          libc = "newlib-nano";
          abi = "ilp64";
        };
      };

    in
    {

      overlays.default = final: prev: rec {
        esp32 = prev.callPackage ./esp32.nix { inherit riscvphone-src nixpkgs-esp-dev; };
        fe310-drv = prev.callPackage ./fe310.nix { inherit riscvphone-src riscv-toolchain nanolibsPath; };
        nanolibsPath = prev.callPackage ./nanolibs.nix { inherit (riscv-toolchain) newlib-nano; };
      };

      packages.x86_64-linux = {
        inherit (pkgs) esp32 nanolibsPath fe310-drv;
      };

      devShells.x86_64-linux = {
        # usage: nix develop .#esp32
        esp32 = nixpkgs-esp-dev.devShells.x86_64-linux.esp32-idf;
      };
    };
}
