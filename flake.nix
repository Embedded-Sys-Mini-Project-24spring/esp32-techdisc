{
  description = "A very basic flake";

  inputs = { nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable"; 
             flake-utils.url = "github:numtide/flake-utils";
             idf.url = "github:mirrexagon/nixpkgs-esp-dev";
};

  outputs = { nixpkgs, flake-utils, idf, ... }:
    let lib = {
      inherit (flake-utils.lib) defaultSystems eachSystem;
    };
    supportedSystems = [ "x86_64-linux" ];
  in lib.eachSystem supportedSystems (system: let
    pkgs = import nixpkgs { 
      inherit system; config.allowUnfree = true;
    overlays = [ idf.overlays.default];
  };
    in {
      devShell =
        # idf.devShells.${system}.esp32-idf;
        pkgs.mkShell { buildInputs = with pkgs; [ 
           platformio-core
          (esp-idf-esp32s2
          .override 
          {
            # rev="4f3cd0deb9c79c8282da4938a29d265705a57564"; sha256 = "sha256-5nq/znpBDsJuae94eqUaDpuIVlCr0SzNoIJZxlvQzDo=";
          } 
          )
          # cmake
          # python39
          # gcc
          # eagle 
          # kicad
        ]; };
    }
    );
}
