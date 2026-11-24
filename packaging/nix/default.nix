# Local build of the nixpkgs draft: `nix-build packaging/nix`. Uses the <nixpkgs> on NIX_PATH unless one is passed.
{
  pkgs ? import <nixpkgs> { },
}:
pkgs.callPackage ./package.nix { }
