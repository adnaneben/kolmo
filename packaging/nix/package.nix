# nixpkgs draft for pkgs/by-name/ko/kolmo/package.nix. For a new release, change `version` and `hash`:
#   nix-prefetch-url https://github.com/adnaneben/kolmo/archive/refs/tags/vX.Y.Z.tar.gz   (then `nix hash to-sri`)
# nixpkgs reviewers may ask for fetchFromGitHub instead; its hash is of the unpacked tree, which
#   nix-prefetch-url --unpack <same url>
# prints. Local build: `nix-build packaging/nix` (default.nix) or `nix build -f packaging/nix`.
{
  lib,
  stdenv,
  fetchurl,
  python3,
  fixDarwinDylibNames,
  versionCheckHook,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "kolmo";
  version = "0.0.2";

  src = fetchurl {
    url = "https://github.com/adnaneben/kolmo/archive/refs/tags/v${finalAttrs.version}.tar.gz";
    hash = "sha256-+rKkSJiAaMZ1V1WvVkFbuS3k9SwU8C7xDVLNd4HBL+w=";
  };

  outputs = [
    "out"
    "lib"
    "dev"
  ];

  nativeBuildInputs = lib.optionals stdenv.hostPlatform.isDarwin [ fixDarwinDylibNames ];

  # ARCH= drops the Makefile's -march=native/-mcpu=native default. The Makefile has no install target.
  makeFlags = [
    "ARCH="
    "CC=${stdenv.cc.targetPrefix}cc"
  ];
  buildFlags = [
    "build/kolmo"
    "lib"
  ];
  enableParallelBuilding = true;

  # The fast part of `make check`: levels, CLI, the frozen frames of earlier releases, hardening, resource and
  # threading tests. `make check` also runs kolmotest and the decoder fuzz regressions, which take tens of minutes.
  doCheck = true;
  nativeCheckInputs = [ python3 ];
  checkPhase = ''
    runHook preCheck
    make $makeFlags build/test-levels build/test-v12-corpus build/test-harden build/kolmoresource build/kolmoparallel
    ./build/test-levels
    python3 tests/test_cli.py build/kolmo
    ./build/test-v12-corpus tests/v12
    ./build/test-harden
    ./build/kolmoresource
    ./build/kolmoparallel
    runHook postCheck
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 build/kolmo $out/bin/kolmo
    install -Dm644 src/kolmo.h $dev/include/kolmo.h
    install -Dm755 build/libkolmo${stdenv.hostPlatform.extensions.sharedLibrary} -t $lib/lib
    install -Dm644 README.md docs/LEVELS.md -t $out/share/doc/kolmo
    runHook postInstall
  '';

  doInstallCheck = true;
  nativeInstallCheckInputs = [ versionCheckHook ];
  versionCheckProgramArg = "--version";

  meta = {
    description = "Structure-aware lossless compressor for tables, logs and scientific data";
    longDescription = ''
      Kolmo is a lossless compressor that detects the structure of its input (columns, records, numeric fields,
      images, audio) and chooses a model for each part. It has levels 1 (fastest) to 9 (max) and an ultra mode,
      random-access reads of a byte range, and decoding that scales across cores.
    '';
    homepage = "https://github.com/adnaneben/kolmo";
    changelog = "https://github.com/adnaneben/kolmo/blob/v${finalAttrs.version}/CHANGELOG.md";
    license = with lib.licenses; [
      asl20
      llvm-exception # src/algo/sais/ (libsais)
    ];
    maintainers = [ ]; # add your nixpkgs maintainer handle (maintainers/maintainer-list.nix)
    mainProgram = "kolmo";
    platforms = lib.platforms.unix;
  };
})
