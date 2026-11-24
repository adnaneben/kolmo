# Context-mixing test vectors (codecs 7, 17 and 25)

The 0.0.2 encoder writes codec 7 as model 1 or model 4 only, so the corpus has no stream of models 2 and 3, no tuned
header (model nibbles 14 and 15) and no legacy image tables. These vectors cover them, and more variants of model 4,
the image model and CMP.

Each `ORIGINAL~VARIANT.cN.z` is a codec-N stream that decodes to the file `ORIGINAL` in this directory. The 0.0.2
sources wrote them: `cm_compress_tm`, `cm_compress_img` and `cmp_compress_model` of `src/codecs/cm*.c`. Three variants
no 0.0.2 entry point writes were made from the same model and coding loop, with the header that `cm_compress_tm`
writes for them: `m3-legacy` (model nibble 3), `m1-exact` and `m2-exact` below 64 KiB (nibbles 5 and 6), and
`gray-legacy` (image without bit 7). The 0.0.2 C decoders (`cm_decompress`, `cm_decompress_img`, `cmp_decompress`)
decoded every vector back to its original.

| Variant | Header |
|---|---|
| `m1-legacy`, `m2-legacy` | size word only (`f` 1-7 and 9-15) |
| `m3-legacy`, `m1-exact`, `m2-exact`, `m3-exact` | model nibble 3, 5, 6, 7 |
| `m2-tuned`, `m3-tuned` | nibbles 14, 15 with tune bytes and `b2 = 0` |
| `m2-prior`, `m3-prior` | nibbles 14, 15 with `b2 = 1` |
| `m4-all`, `m4-none`, `m4-small`, `m4-flat` | model 4: every context with the text prior, no context, contexts 1, 2 and 18 with the binary prior, the binary set with no prior |
| `rgb`, `rgba-head`, `narrow`, `gray-legacy` | codec 17: 3 channels; 4 channels after 16 header bytes; 2 channels in rows of 4 bytes after 7; 1 channel with legacy tables |
| `m4-parts`, `m1-parts` | codec 25: three model-4 parts; two model-1 parts |

`prefixes.json` holds the SHA-256 of the first 1024 decoded bytes of each corpus block under a transform, for the
quick check in `make check` (FORMAT.md 24.8). `tests/cm_reference.py --write-prefixes` rewrites it, after
`make check-cm-reference` has checked every stream whole against its original.
