{
  description = "HelixDrift nRF mocap node workspace";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in {
      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          git
          python3
          curl
          cmake
          ninja
          gcc
          gcc-arm-embedded
          openocd
          python3Packages.pyocd
          python3Packages.west
          python3Packages.pyusb
          usbutils
          dfu-util
          picocom
          minicom
        ];
      };
    };
}
