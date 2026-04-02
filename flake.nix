{
  description = "HelixDrift nRF mocap node workspace";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
      pythonEnv = pkgs.python3.withPackages (ps: with ps; [
        pytest
        numpy
        matplotlib
        pydantic
        click
        cbor2
        intelhex
        cryptography
      ]);
    in {
      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          git
          pythonEnv
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
          netcat-openbsd
          picocom
          minicom
        ];
      };
    };
}
