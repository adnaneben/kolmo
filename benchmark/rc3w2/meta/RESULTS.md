# rc3 wave 2, meta lane: what file metadata adds

Branch `claude/rc3w2-meta-20261117` on `claude/release-rc3-20261116` (`1536036`). Design and rules: `docs/METADATA.md`.
Format: `SPEC.md` (plan type 11 ARM64 filter, codec 19 plan prefix `0, 1` for AIFF; both version 11).

**Machine:** cloud VM, Intel Xeon @ 2.10 GHz, 4 vCPUs, Linux 6.18, gcc 13, `make` default flags. Every run `-T 4`
(all threads); times are the median of 3 runs, wall clock, of the CLI including process start (`scripts/bench3.sh`).
Sizes are deterministic. "rc3 base" is the integration branch head built from the same tree before this lane.

## Data (downloaded or generated; not committed)

| set | files | source |
|---|---|---|
| tensors | three safetensors checkpoints (fp32 90 MB, fp16 + uint8 166 MB, bf16 269 MB), two GGUF files (fp32 1.2 MB, q4_0 19 MB) | public model hubs |
| images | 4 Kodak photographs as PPM, PGM, BMP, NPY, 16-bit PGM, full 768x512 and cropped 765x509 ("odd"); 2 as TIFF | r0k.us Kodak PNGs, decoded by `scripts/png2.py` |
| audio | 8 ESC-50 clips (WAV) and the same as AIFF | ESC-50 repository; `scripts/aiff.py` |
| executables | x86-64 ELF: cc1 (gcc 13), libjvm.so, python3.12 from this VM; ARM64 ELF: cc1 and python3.11 from Debian arm64 packages | VM, deb.debian.org |
| archives | a 15.5 MB tar (Kolmo sources, a WAV, a PGM, an x86 ELF, a CSV); MNIST NPZ stored (55 MB) and deflated (11 MB); a mixed NPZ (two images, int16 walk, decimals, float32) | VM, `scripts/derive.py`, `scripts/mk2.py` |
| packed | gzip, xz of a CSV, gzip of SPEC.md, a JPEG | VM |
| columnar | three Parquet files (5 KB, 21 MB text, 48 MB taxi trips), Y4M (akiyo CIF) | public datasets |

## Results, type by type

| file | mode | input | rc3 base | meta | size | compress s (base → meta) | decompress s (base → meta) |
|---|---|---:|---:|---:|---:|---|---|
| data.csv.gz | balanced | 1,248,662 | 1,248,734 | 1,248,734 | +0.00% | 0.011 → 0.006 | 0.006 → 0.005 |
| data.xz | balanced | 800,004 | 800,056 | 800,056 | +0.00% | 0.010 → 0.005 | 0.005 → 0.005 |
| mnist.npz | balanced | 11,490,434 | 11,490,646 | 11,490,646 | +0.00% | 0.025 → 0.016 | 0.014 → 0.014 |
| photo.jpg | balanced | 6,412 | 6,464 | 6,464 | +0.00% | 0.006 → 0.004 | 0.004 → 0.004 |
| spec.gz | balanced | 18,719 | 18,771 | 18,771 | +0.00% | 0.005 → 0.003 | 0.004 → 0.003 |
| mnist_stored.npz | balanced | 54,951,002 | 11,619,842 | 11,613,685 | -0.05% | 0.095 → 0.085 | 0.056 → 0.053 |
| mnist_x.npy | balanced | 47,040,128 | 9,936,759 | 9,936,759 | +0.00% | 0.076 → 0.083 | 0.045 → 0.050 |
| arm_cc1 | balanced | 28,136,328 | 11,266,233 | 10,402,954 | -7.66% | 0.074 → 0.099 | 0.035 → 0.041 |
| arm_python3.11 | balanced | 6,616,896 | 2,672,819 | 2,543,107 | -4.85% | 0.033 → 0.035 | 0.013 → 0.014 |
| cc1 | balanced | 30,487,984 | 11,696,076 | 11,664,891 | -0.27% | 0.094 → 0.078 | 0.044 → 0.038 |
| libjvm.so | balanced | 26,755,496 | 8,314,977 | 8,185,145 | -1.56% | 0.066 → 0.146 | 0.035 → 0.038 |
| python3.12 | balanced | 8,020,928 | 3,012,161 | 2,863,006 | -4.95% | 0.036 → 0.034 | 0.014 → 0.015 |
| kodim05_odd.tif | balanced | 1,168,289 | 871,487 | 796,040 | -8.66% | 0.023 → 0.024 | 0.011 → 0.008 |
| kodim23_odd.tif | balanced | 1,168,289 | 663,265 | 567,591 | -14.42% | 0.026 → 0.032 | 0.009 → 0.007 |
| mixed_stored.npz | balanced | 5,937,534 | 3,777,569 | 2,858,699 | -24.32% | 0.049 → 0.189 | 0.012 → 0.016 |
| f32small.gguf | balanced | 1,185,376 | 939,237 | 971,866 | +3.47% | 0.030 → 0.068 | 0.008 → 0.007 |
| q4_0.gguf | balanced | 19,077,344 | 9,026,769 | 9,022,726 | -0.04% | 0.051 → 0.047 | 0.027 → 0.026 |
| akiyo.y4m | balanced | 45,621,044 | 8,405,152 | 8,405,152 | +0.00% | 0.653 → 0.706 | 0.194 → 0.195 |
| mixed.tar | balanced | 15,513,600 | 4,255,991 | 4,101,528 | -3.63% | 0.052 → 0.058 | 0.026 → 0.026 |
| nyc.parquet | balanced | 47,673,370 | 47,673,642 | 47,673,642 | +0.00% | 0.072 → 0.070 | 0.046 → 0.045 |
| sample.parquet | balanced | 5,230 | 2,776 | 2,776 | +0.00% | 0.005 → 0.004 | 0.003 → 0.003 |
| text.parquet | balanced | 20,979,968 | 18,198,380 | 18,198,380 | +0.00% | 0.070 → 0.050 | 0.027 → 0.030 |
| bf16.safetensors | balanced | 269,060,552 | 179,452,864 | 179,427,037 | -0.01% | 0.362 → 0.350 | 0.233 → 0.240 |
| fp16.safetensors | balanced | 166,029,852 | 114,714,481 | 114,683,946 | -0.03% | 1.107 → 0.328 | 0.189 → 0.180 |
| model.safetensors | balanced | 90,868,376 | 74,649,923 | 74,637,693 | -0.02% | 0.183 → 0.237 | 0.095 → 0.108 |
| kodim01.ppm | balanced | 1,179,663 | 768,541 | 768,990 | +0.06% | 0.028 → 0.038 | 0.008 → 0.009 |
| kodim01_odd.ppm | balanced | 1,168,170 | 862,757 | 766,767 | -11.13% | 0.017 → 0.028 | 0.007 → 0.007 |
| kodim01_odd.pgm | balanced | 389,400 | 330,520 | 272,128 | -17.67% | 0.011 → 0.013 | 0.004 → 0.005 |
| kodim01_odd.bmp | balanced | 1,168,718 | 861,954 | 766,784 | -11.04% | 0.017 → 0.026 | 0.006 → 0.007 |
| kodim13.ppm | balanced | 1,179,663 | 1,007,568 | 891,889 | -11.48% | 0.017 → 0.026 | 0.006 → 0.008 |
| kodim13.pgm | balanced | 393,231 | 349,808 | 305,993 | -12.53% | 0.011 → 0.017 | 0.005 → 0.006 |
| kodim13_odd.bmp | balanced | 1,168,718 | 1,007,143 | 889,231 | -11.71% | 0.018 → 0.022 | 0.006 → 0.007 |
| kodim05_odd.ppm | balanced | 1,168,170 | 874,423 | 795,975 | -8.97% | 0.033 → 0.024 | 0.010 → 0.007 |
| kodim23_odd.ppm | balanced | 1,168,170 | 662,722 | 567,526 | -14.36% | 0.029 → 0.025 | 0.009 → 0.008 |
| kodim23.npy | balanced | 1,179,776 | 572,135 | 572,135 | +0.00% | 0.038 → 0.033 | 0.008 → 0.009 |
| 1-100032-A-0.aiff | balanced | 441,054 | 26,149 | 14,443 | -44.77% | 0.007 → 0.015 | 0.004 → 0.005 |
| 1-100038-A-14.aiff | balanced | 441,054 | 360,160 | 218,978 | -39.20% | 0.011 → 0.024 | 0.004 → 0.009 |
| 1-100210-A-36.aiff | balanced | 441,054 | 437,431 | 338,165 | -22.69% | 0.010 → 0.021 | 0.004 → 0.009 |
| 1-101296-A-19.aiff | balanced | 441,054 | 258,000 | 152,422 | -40.92% | 0.010 → 0.027 | 0.005 → 0.013 |
| 1-101336-A-30.aiff | balanced | 441,054 | 172,609 | 96,747 | -43.95% | 0.011 → 0.031 | 0.005 → 0.008 |
| 1-101404-A-34.aiff | balanced | 441,054 | 252,052 | 158,254 | -37.21% | 0.011 → 0.030 | 0.005 → 0.009 |
| 1-103298-A-9.aiff | balanced | 441,054 | 373,946 | 245,325 | -34.40% | 0.011 → 0.023 | 0.005 → 0.009 |
| 1-104089-A-22.aiff | balanced | 441,054 | 396,208 | 274,331 | -30.76% | 0.009 → 0.026 | 0.004 → 0.009 |
| data.csv.gz | hc | 1,248,662 | 1,248,714 | 1,248,714 | +0.00% | 0.140 → 0.006 | 0.005 → 0.005 |
| data.xz | hc | 800,004 | 800,056 | 800,056 | +0.00% | 0.079 → 0.005 | 0.005 → 0.005 |
| mnist.npz | hc | 11,490,434 | 11,490,486 | 11,490,506 | +0.00% | 1.921 → 0.042 | 0.024 → 0.034 |
| photo.jpg | hc | 6,412 | 6,399 | 6,464 | +1.02% | 0.006 → 0.003 | 0.006 → 0.004 |
| spec.gz | hc | 18,719 | 18,771 | 18,771 | +0.00% | 0.006 → 0.004 | 0.004 → 0.003 |
| mnist_stored.npz | hc | 54,951,002 | 9,874,586 | 9,892,861 | +0.19% | 7.970 → 7.604 | 0.086 → 0.072 |
| mnist_x.npy | hc | 47,040,128 | 8,448,209 | 8,448,209 | +0.00% | 7.841 → 8.206 | 0.080 → 0.076 |
| arm_cc1 | hc | 28,136,328 | 9,130,308 | 8,324,572 | -8.82% | 7.326 → 7.278 | 0.077 → 0.076 |
| arm_python3.11 | hc | 6,616,896 | 2,113,862 | 1,970,146 | -6.80% | 2.156 → 1.051 | 0.023 → 0.022 |
| cc1 | hc | 30,487,984 | 9,546,909 | 9,528,003 | -0.20% | 6.606 → 6.976 | 0.076 → 0.083 |
| libjvm.so | hc | 26,755,496 | 6,549,437 | 6,420,129 | -1.97% | 7.415 → 6.795 | 0.075 → 0.074 |
| python3.12 | hc | 8,020,928 | 2,303,563 | 2,178,956 | -5.41% | 2.562 → 1.302 | 0.029 → 0.021 |
| kodim05_odd.tif | hc | 1,168,289 | 842,630 | 723,519 | -14.14% | 0.301 → 0.384 | 0.011 → 0.012 |
| kodim23_odd.tif | hc | 1,168,289 | 606,603 | 531,023 | -12.46% | 0.439 → 0.495 | 0.011 → 0.010 |
| mixed_stored.npz | hc | 5,937,534 | 3,602,340 | 2,736,497 | -24.04% | 3.068 → 1.030 | 0.026 → 0.018 |
| f32small.gguf | hc | 1,185,376 | 868,522 | 865,001 | -0.41% | 0.067 → 0.256 | 0.009 → 0.007 |
| q4_0.gguf | hc | 19,077,344 | 8,591,209 | 8,588,080 | -0.04% | 3.408 → 1.787 | 0.049 → 0.038 |
| akiyo.y4m | hc | 45,621,044 | 6,985,316 | 6,985,316 | +0.00% | 9.050 → 9.083 | 0.191 → 0.208 |
| mixed.tar | hc | 15,513,600 | 3,419,124 | 3,286,556 | -3.88% | 2.254 → 1.303 | 0.033 → 0.031 |
| nyc.parquet | hc | 47,673,370 | 47,580,080 | 47,580,080 | +0.00% | 3.600 → 3.441 | 0.075 → 0.067 |
| sample.parquet | hc | 5,230 | 2,518 | 2,518 | +0.00% | 0.006 → 0.007 | 0.003 → 0.004 |
| text.parquet | hc | 20,979,968 | 17,239,630 | 17,239,630 | +0.00% | 6.800 → 6.248 | 0.090 → 0.078 |
| bf16.safetensors | hc | 269,060,552 | 179,433,989 | 179,416,819 | -0.01% | 2.010 → 2.038 | 0.324 → 0.302 |
| fp16.safetensors | hc | 166,029,852 | 114,766,170 | 114,749,461 | -0.01% | 11.131 → 1.035 | 0.229 → 0.239 |
| model.safetensors | hc | 90,868,376 | 74,417,457 | 74,407,166 | -0.01% | 0.700 → 0.725 | 0.153 → 0.131 |
| kodim01.ppm | hc | 1,179,663 | 664,416 | 664,873 | +0.07% | 0.399 → 0.407 | 0.009 → 0.012 |
| kodim01_odd.ppm | hc | 1,168,170 | 948,064 | 662,443 | -30.13% | 0.211 → 0.401 | 0.009 → 0.009 |
| kodim01_odd.pgm | hc | 389,400 | 316,966 | 269,694 | -14.91% | 0.067 → 0.088 | 0.005 → 0.005 |
| kodim01_odd.bmp | hc | 1,168,718 | 652,366 | 663,375 | +1.69% | 0.270 → 0.423 | 0.007 → 0.010 |
| kodim13.ppm | hc | 1,179,663 | 1,012,449 | 790,601 | -21.91% | 0.199 → 0.386 | 0.008 → 0.010 |
| kodim13.pgm | hc | 393,231 | 341,345 | 304,606 | -10.76% | 0.079 → 0.095 | 0.006 → 0.005 |
| kodim13_odd.bmp | hc | 1,168,718 | 901,942 | 784,167 | -13.06% | 0.207 → 0.372 | 0.010 → 0.009 |
| kodim05_odd.ppm | hc | 1,168,170 | 837,324 | 723,452 | -13.60% | 0.284 → 0.390 | 0.011 → 0.009 |
| kodim23_odd.ppm | hc | 1,168,170 | 609,492 | 530,956 | -12.89% | 0.443 → 0.481 | 0.010 → 0.008 |
| kodim23.npy | hc | 1,179,776 | 535,155 | 535,155 | +0.00% | 0.482 → 0.482 | 0.008 → 0.009 |
| 1-100032-A-0.aiff | hc | 441,054 | 25,538 | 14,443 | -43.45% | 0.017 → 0.025 | 0.004 → 0.005 |
| 1-100038-A-14.aiff | hc | 441,054 | 310,436 | 218,978 | -29.46% | 0.151 → 0.117 | 0.005 → 0.010 |
| 1-100210-A-36.aiff | hc | 441,054 | 436,764 | 338,165 | -22.57% | 0.043 → 0.071 | 0.004 → 0.010 |
| 1-101296-A-19.aiff | hc | 441,054 | 195,010 | 152,422 | -21.84% | 0.172 → 0.180 | 0.005 → 0.011 |
| 1-101336-A-30.aiff | hc | 441,054 | 134,066 | 96,747 | -27.84% | 0.108 → 0.140 | 0.004 → 0.008 |
| 1-101404-A-34.aiff | hc | 441,054 | 222,003 | 158,254 | -28.72% | 0.132 → 0.152 | 0.005 → 0.009 |
| 1-103298-A-9.aiff | hc | 441,054 | 309,386 | 245,325 | -20.71% | 0.117 → 0.133 | 0.005 → 0.010 |
| 1-104089-A-22.aiff | hc | 441,054 | 329,310 | 274,331 | -16.70% | 0.091 → 0.108 | 0.006 → 0.010 |
| arm_python3.11 | max | 6,616,896 | 1,832,327 | 1,707,511 | -6.81% | 3.431 → 1.865 | 0.104 → 0.059 |
| arm_cc1 | max | 28,136,328 | 7,815,515 | 7,075,133 | -9.47% | 15.223 → 11.479 | 0.474 → 0.302 |
| python3.12 | max | 8,020,928 | 2,118,400 | 1,997,922 | -5.69% | 3.839 → 2.474 | 0.118 → 0.079 |
| libjvm.so | max | 26,755,496 | 5,954,144 | 5,922,291 | -0.53% | 14.293 → 10.061 | 0.336 → 0.230 |
| mnist.npz | max | 11,490,434 | 11,490,486 | 11,490,506 | +0.00% | 4.296 → 0.030 | 0.027 → 0.026 |
| data.csv.gz | max | 1,248,662 | 1,248,714 | 1,248,714 | +0.00% | 0.310 → 0.006 | 0.006 → 0.006 |
| mixed.tar | max | 15,513,600 | 3,205,566 | 3,061,972 | -4.48% | 3.924 → 2.293 | 0.123 → 0.140 |
| mixed_stored.npz | max | 5,937,534 | 3,177,215 | 2,556,506 | -19.54% | 4.190 → 1.595 | 0.165 → 0.166 |
| kodim05_odd.ppm | max | 1,168,170 | 708,142 | 622,364 | -12.11% | 0.866 → 0.591 | 0.186 → 0.160 |
| kodim13_odd.bmp | max | 1,168,718 | 695,828 | 675,729 | -2.89% | 0.842 → 0.545 | 0.178 → 0.176 |
| 1-100032-A-0.aiff | max | 441,054 | 24,478 | 13,955 | -42.99% | 0.029 → 0.041 | 0.005 → 0.011 |
| 1-100038-A-14.aiff | max | 441,054 | 306,345 | 209,144 | -31.73% | 0.286 → 0.344 | 0.011 → 0.052 |
| 1-100210-A-36.aiff | max | 441,054 | 431,472 | 324,625 | -24.76% | 0.082 → 0.137 | 0.020 → 0.050 |
| 1-101296-A-19.aiff | max | 441,054 | 184,472 | 143,513 | -22.20% | 0.386 → 0.394 | 0.013 → 0.047 |
| 1-101336-A-30.aiff | max | 441,054 | 119,433 | 92,578 | -22.49% | 0.189 → 0.225 | 0.011 → 0.031 |
| 1-101404-A-34.aiff | max | 441,054 | 193,802 | 153,212 | -20.94% | 0.354 → 0.367 | 0.056 → 0.046 |
| 1-103298-A-9.aiff | max | 441,054 | 304,196 | 238,550 | -21.58% | 0.296 → 0.377 | 0.010 → 0.051 |
| 1-104089-A-22.aiff | max | 441,054 | 325,133 | 266,370 | -18.07% | 0.313 → 0.357 | 0.012 → 0.053 |


Sizes in bytes; "size" is meta against base. The rows for safetensors, GGUF and kodim01_odd.bmp above were measured
before two final adjustments; their final values are in the next table.

### After the final adjustments

A tensor file's header shorter than 64 KiB now joins the first tensors (a separate small header moved block
boundaries: the 1.2 MB GGUF was +3.47% at `balanced` from that alone), and an image whose header Kolmo reads but
whose bytes look like text is coded both ways per block (kodim01_odd.bmp was +1.69% at `hc`).

FINALROWS

## Findings

1. **Images (biggest gain).** The sampler's lag search only tries 36 common widths, and decides "text" before trying
   images when 95% of the first 64 KiB is printable — which a bright photograph can be. A header gives the exact row
   length (BMP: padded stride) and channel count: −9% to −18% at `balanced`, −11% to −30% at `hc`, on odd widths and
   on one standard width the sampler missed (kodim13.ppm −11.5% / −21.9%). Standard widths found by the sampler:
   ±0.07%. TIFF −8.7% / −14.4% (`balanced`). NPY was already read.
2. **ARM64 executables.** No branch filter existed for ARM64. With section tables, the code alone takes BL/ADRP
   filtering: −4.85% / −7.66% at `balanced`, −6.80% / −8.82% at `hc`, −6.81% / −9.47% at `max`; compress also
   faster at `hc`/`max` (smaller data for LZMA2). The page phase matters: a filter with phase 0 at the section start
   was 0.35% larger, a wrong phase 0.54% larger; blocks must be 4-byte aligned (a misaligned 3.5 MB block size made
   the first try 5.7% worse).
3. **x86 executables.** Planning code and data apart: python3.12 −4.95% / −5.41% / −5.69% (balanced/hc/max),
   libjvm −1.56% / −1.97% / −0.53%, cc1 −0.27% / −0.20%.
4. **AIFF.** 16-bit AIFF went through the generic paths (52% larger than the same audio as WAV). Swapping the
   samples into the audio codec makes it equal to WAV: −17% to −45%. Decode is as slow as kaud's (as for WAV).
5. **Archives.** Tar members with a known header (64 KiB+) get a part and are cut further (the ELF in the test tar
   was split into sections): −3.63% / −3.88% / −4.48%. Zip/NPZ: a mixed NPZ −24.3% / −24.0% / −19.5% because each NPY
   member's header now starts a part. One-dtype NPZ (MNIST) −0.05% at `balanced`, +0.19% at `hc` (members of one
   kind lose cross-member matches in 16 MiB blocks); kept, since the mixed case gains two orders more.
6. **Packed content.** Size is unchanged (stored before, stored now) but `hc` and `max` no longer run their coders
   on it: deflated NPZ `hc` 1.92 → 0.04 s, `max` 4.30 → 0.03 s; gzip CSV `max` 0.31 → 0.006 s. The probe (collision
   entropy, integer arithmetic, + LZ4) keeps blocks with repeats coded. Cost: a 6 KB JPEG that zstd 19 squeezed by 13
   B is now stored (+65 B with the container).
7. **Tensors (nearly nothing).** Kolmo's float graph already finds bf16/fp16/fp32 element widths and splits the
   exponent. Measured: a plan per run of one dtype −0.01% to −0.03%; a plan per tensor of 1 MiB or more (−0.06% to
   −0.28%, but +0.11% on fp16) was 0.035% smaller over 4 files at twice the planning time, so type runs were kept.
   Separating the fp16 and uint8 tensors made `hc` 10.7× faster (11.1 s → 1.0 s). The entropy bounds
   (`scripts/ent*.py`) show what a dedicated tensor coder could add: per-tensor order-0 exponent and sign+mantissa
   0.76% below `balanced` on bf16; exponent with a column context −0.3% more; mantissa given the exponent −0.4% more.
   GGUF q4_0: −0.04% (the record trial already finds the 18-byte blocks).
8. **Not changed:** WAV and Y4M (already routed by header), Parquet (pages mostly compressed; output = input + 272 B
   at most), CSV/JSON (header row and keys already read by the table and JSON transforms), 16-bit PGM (IMG16's
   width search found every width tried).
9. **Speed.** Planning per part costs a little at `balanced` where the parts are many (libjvm 0.066 → 0.146 s, the
   mixed NPZ 0.049 → 0.189 s, 4 threads) and less elsewhere; `hc`/`max` are mostly faster (packed content, fewer
   whole-file trials, smaller transformed code).

## What failed or was dropped

- A plan per tensor (above).
- ADRP with a segment-relative phase (+0.35%).
- First ARM64 build: blocks of `n/8` bytes broke word alignment (+5.7%); blocks of this plan are now 4096-aligned.
- First packed probe used `log2` (floating point): replaced by an integer collision-entropy test, so the store
  decision is the same on every platform.
- Splitting off a small tensor-file header (+3.47% on a 1.2 MB GGUF at `balanced`): now joins the first tensors.

## Tests

`make check`, `make check-rc` (with the new `check-meta`: parsers on 3000 mutations of each of 18 synthetic files
under ASan/UBSan, ARM64 bijection on 2 million words, frame round trips at 1 and 3 threads, version-11 refusal),
`make check-sanitizers`, `make experimental-check GPU_CHECK=`, `make kfam-check`, and the determinism check: all 122
frames identical to `expected.sha256` (no input of the check carries a header this lane reads), so the file was not
regenerated.

## Commands

```
make && cp build/kolmo kolmo-meta            # this branch; kolmo-base from 1536036
scripts/bench3.sh kolmo-base base.csv balanced FILES...   (and hc, max; then kolmo-meta)
python3 -I scripts/ent.py FILE.safetensors   # per-tensor entropy bounds (ent2.py: contexts)
python3 -I scripts/a64.py ELF OUT adrp        # the ARM64 filter prototype used for the first measurements
```
