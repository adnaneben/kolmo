# DRAFT: registering Kolmo in c-blosc2's codec registry

**Status: draft for the owner. Nothing has been opened on Blosc/c-blosc2.**

## How c-blosc2 registers codecs (checked against c-blosc2 main and 3.8.0, 2026-11-23)

- **Global IDs 32-159** are assigned by the Blosc team. They are listed in `include/blosc2/codecs-registry.h` and registered in `plugins/codecs/codecs-registry.c`. Taken so far:
  - 32 ndlz;
  - 33-35 zfp;
  - 36 openhtj2k, 37 grok, 38 openzl, 39 j2k, 40 htj2k.

  The next free ID is **41**.
- **User IDs 160-255** are registered by an application with `blosc2_register_codec`. They are never assigned globally. Kolmo uses 244 until a global ID is granted.
- Codecs in the registry come in two kinds:
  - built in: ndlz and zfp, whose code lives in `plugins/codecs/<name>/`;
  - external: openhtj2k, grok, openzl, j2k and htj2k. These are registered with `encoder = NULL`, `decoder = NULL` and a name. On first use c-blosc2 calls `dlopen("libblosc2_<name>.so")`; if that fails, it runs `python -c "import blosc2_<name>; blosc2_<name>.print_libpath()"` and opens the path printed. It then reads the library's `info` symbol (`codec_info {char *encoder; char *decoder;}`) and looks up the two functions it names. The code lives in its own repository and wheel, for example [Blosc/blosc2_openzl](https://github.com/Blosc/blosc2_openzl).

Kolmo should be an **external** codec. The plugins README asks for "a relatively small footprint", and Kolmo's core is a whole compressor. The external route keeps Kolmo's code and releases in Kolmo's repository; c-blosc2 only records the ID and name. This plugin already implements that interface:
- `libblosc2_kolmo.so` exports `info`, `blosc2_kolmo_encoder` and `blosc2_kolmo_decoder`;
- `python/blosc2_kolmo/__init__.py` has `print_libpath()`.

`make test` checks that c-blosc2 3.8.0 itself loads the library this way and round-trips through it (`tests/test_dynamic_load.c`). Requirements from c-blosc2's `plugins/README.md`:
- the plugin is written in C;
- tests are provided and pass;
- each test's `main()` calls `blosc2_init()` first and `blosc2_destroy()` last (`tests/test_*.c` do);
- a README explains the motivation, usage, how it works, and its strengths and weaknesses ([README.md](README.md));
- the Blosc team decides acceptance.

## Steps (owner)

1. Open an issue or discussion on Blosc/c-blosc2 asking for a codec ID for Kolmo (text below), and wait for their answer.
2. Once they agree, open the PR below on c-blosc2, with the ID they give.
3. Then, in this repository:
   - set `BLOSC_CODEC_KOLMO` in `blosc2_kolmo.h`, `ID` in `python/blosc2_kolmo/__init__.py` and the define in `tests/test_dynamic_load.c` to that ID;
   - after the next Kolmo release, publish the `blosc2_kolmo` wheel, which bundles `libblosc2_kolmo.so` with that `__init__.py`. Publishing waits for the owner's release rule.

## The c-blosc2 pull request (draft)

`include/blosc2/codecs-registry.h`:

```c
    BLOSC_CODEC_HTJ2K = 40,
    //!< High Throughput JPEG 2000 compressor plugin.
    //!< See https://github.com/Blosc/blosc2_htj2k
    BLOSC_CODEC_KOLMO = 41,
    //!< Kolmo: structure-aware lossless compressor for numeric and tabular data.
    //!< See https://github.com/adnaneben/kolmo/tree/main/integration/blosc2
```

`plugins/codecs/codecs-registry.c`, after htj2k:

```c
  blosc2_codec kolmo;
  kolmo.compcode = BLOSC_CODEC_KOLMO;
  kolmo.version = 1;
  kolmo.complib = BLOSC_CODEC_KOLMO;
  kolmo.encoder = NULL;
  kolmo.decoder = NULL;
  kolmo.compname = "kolmo";
  register_codec_private(&kolmo);
```

(Also bump `BLOSC2_GLOBAL_REGISTERED_CODECS` in `include/blosc2.h` if their PR template asks for it; htj2k's PR shows the pattern.)

### Issue / PR text

> **Register the Kolmo codec (external plugin, ID 41)**
>
> Kolmo (https://github.com/adnaneben/kolmo, Apache-2.0) is a lossless, structure-aware compressor written in C. For numeric arrays it applies byte-plane, delta and integer/float series models before its own entropy coders, without configuration. We have a Blosc2 codec plugin for it, `integration/blosc2/` in the Kolmo repository. It follows the external-plugin layout of blosc2_openzl:
> - `libblosc2_kolmo.so` with an `info` symbol, the encoder and the decoder;
> - a `blosc2_kolmo` Python package with `print_libpath()`.
>
> Its tests cover in-process registration, super-chunk frames, and c-blosc2 itself loading the library through a NULL-encoder registration.
>
> Single-thread results with c-blosc2 3.8.0 (4 MiB chunks; full table in the plugin README):
> - owid_energy float64: Kolmo level 3 is 14.3x (blocksize = chunk), against 5.4x for zstd 9 + shuffle and 4.1x for lz4 + shuffle.
> - Kolmo compresses at 40-310 MB/s at level 3 and decompresses at 1.0-3.7 GB/s, so it suits write-once, read-many archives rather than hot paths.
>
> This PR adds only the ID and name to the registry, with NULL encoder and decoder, as for openzl, j2k and htj2k. Users get the codec with `pip install blosc2_kolmo` or by putting `libblosc2_kolmo.so` on the library path.
