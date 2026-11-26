# Ultra+ (UPX, `--archive-max`): measured results

All rows are SHA- or `cmp`-checked round trips, run single-threaded (`-T 1`). Sizes are deterministic. Times and RSS were taken
on a heavily loaded Mac (load average 40-100 from other lanes; niced). They show the order of magnitude only; the
timed run through `timed.sh` at load < 12 is still to come.

Binaries: `-U` and `-U --codec-level 2` are main `a1d39559`. Ultra+ (up3) is #122 at `7c14df3a`; the later
`c17a3ac8` changes only the summation order and gives the same d200k bytes. The cuts are the first 256 KiB of each
impact-corpus file (PR #48), and their SHA-256 values are posted on #57. cmix v21 rows fill in as its local run
progresses (about 20 min per cut under this load).

## Dev split (22 cuts)
| cut | bytes | -U | -U level 2 | **Ultra+** | vs -U | vs level 2 | cmix v21 | enc s | dec s | RSS enc/dec MB | frame | RT |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| audio__esc50-1-100032 | 262,144 | 13,940 | 13,940 | **13,940** | +0.00% | +0.00% | 14,478 | 72 | 1 | 340/3 | v12 | OK |
| audio__esc50-1-28808 | 262,144 | 148,939 | 148,939 | **148,939** | +0.00% | +0.00% | 153,334 | 2458 | 1 | 590/3 | v12 | OK |
| code__flac-c | 262,144 | 28,793 | 28,793 | **27,264** | -5.31% | -5.31% |  | 138 | 106 | 310/313 | v13 | OK |
| container__kennedy-xls | 262,144 | 5,737 | 5,737 | **4,287** | -25.27% | -25.27% |  | 142 | 85 | 311/332 | v13 | OK |
| csv__energy-130col | 262,144 | 32,906 | 32,906 | **32,149** | -2.30% | -2.30% |  | 86 | 59 | 332/337 | v13 | OK |
| csv__ghcn | 262,144 | 3,874 | 3,874 | **3,595** | -7.20% | -7.20% |  | 94 | 66 | 330/296 | v13 | OK |
| exe__ooffice-x86 | 262,144 | 92,680 | 92,680 | **86,524** | -6.64% | -6.64% |  | 121 | 95 | 333/328 | v13 | OK |
| genomics__srr835775 | 262,144 | 48,800 | 48,800 | **47,042** | -3.60% | -3.60% |  | 106 | 88 | 296/298 | v13 | OK |
| img16__x-ray | 262,144 | 113,807 | 113,807 | **113,515** | -0.26% | -0.26% |  | 983 | 1014 | 500/524 | v13 | OK |
| img8__hubble | 262,144 | 135,082 | 135,082 | **129,233** | -4.33% | -4.33% |  | 62 | 60 | 332/329 | v13 | OK |
| img8__retina | 262,144 | 15,898 | 15,898 | **15,898** | +0.00% | +0.00% |  | 50 | 4 | 292/121 | v12 | OK |
| json__api | 262,144 | 13,876 | 13,876 | **13,066** | -5.84% | -5.84% |  | 48 | 53 | 301/300 | v13 | OK |
| log__loghub | 262,144 | 8,598 | 8,598 | **8,153** | -5.18% | -5.18% |  | 54 | 47 | 312/315 | v13 | OK |
| numeric__owid-f64 | 262,144 | 19,349 | 17,734 | **17,471** | -9.71% | -1.48% |  | 69 | 4 | 340/149 | v13 | OK |
| numeric__population-i64 | 151,152 | 37,458 | 36,039 | **35,615** | -4.92% | -1.18% |  | 79 | 29 | 463/345 | v13 | OK |
| tar__canterbury | 262,144 | 8,523 | 8,523 | **6,992** | -17.96% | -17.96% |  | 43 | 33 | 322/321 | v13 | OK |
| text__dickens | 262,144 | 65,152 | 65,152 | **63,098** | -3.15% | -3.15% |  | 60 | 46 | 363/306 | v13 | OK |
| tsv__ghcn-rows | 262,144 | 11,508 | 11,508 | **10,784** | -6.29% | -6.29% |  | 57 | 41 | 291/314 | v13 | OK |
| video__akiyo | 262,144 | 68,579 | 68,579 | **68,579** | +0.00% | +0.00% |  | 44 | 5 | 363/125 | v12 | OK |
| weights__bf16-10mb | 262,144 | 173,180 | 173,180 | **172,775** | -0.23% | -0.23% |  | 82 | 37 | 367/343 | v13 | OK |
| weights__magika-onnx | 262,144 | 199,325 | 199,325 | **198,206** | -0.56% | -0.56% |  | 90 | 45 | 350/351 | v13 | OK |
| xml__silesia-xml | 262,144 | 7,663 | 7,663 | **6,999** | -8.67% | -8.67% |  | 56 | 46 | 299/293 | v13 | OK |
| **total** | 5,656,176 | 1,253,667 | 1,250,633 | **1,224,124** | -2.36% | -2.12% | | | | | | |

Reading:
- Ultra+ is never larger than ultra, because each block keeps whichever stream is smaller.
- It is smaller on 18 of 22 cuts. The 4 ties (two audio cuts, retina, akiyo) are blocks that ultra codes with kaud, kvid or kimg. UPX is not wired into those paths yet.
- Largest gains: containers and tar (−18 to −25%), xml (−8.7%), x86 (−6.6% on the BCJ-filtered stream), tables (−6.3 to −7.2%), JSON and logs (−5.2 to −5.8%), code (−5.3%) and prose (−3.2%).
- Floats and weights: −0.2 to −0.6%. 16-bit image: −0.3%.
- Against ultra level 2 the numeric graph cuts gain −1.2 to −1.5%. Level 2 itself already took most of the rest.

## Probe slices (dickens; the 24-Nov cmix/paq8px probe)

| input | -U | Ultra+ | cmix v21 | paq8px -8 |
|---|---:|---:|---:|---:|
| first 200,000 B | 51,151 | 49,419 (−3.4%, bare stream at 7c14df3a) | 47,064 | 46,745 |
| first 1,000,000 B | 218,145 | 211,692 (−3.0%, harness, 38 contexts) | — | 198,643 |

## cmix v21 on the cuts (local, one niced process)

Only two cuts finished: ESC-50 100032 (cmix 14,478 B, Kolmo 13,940 B) and ESC-50 28808 (cmix 153,334 B, Kolmo
148,939 B). Kolmo wins both by 3-4%. Under this load cmix took 1,372 s and 6,499 s per 256 KiB cut, so the run was
stopped on 26 Nov to keep this lane to one process. Comparisons with cmix use the 24-Nov probe slice and the published
full-file figures (PLAN.md).

## Negative results

- **Kolmo's word-dictionary transform (tflag 9) before UPX:** d200k gives 52,704 B against 49,419 B without it, which is
  6.6% larger. Ultra already disables that transform for model 4. A dictionary for Ultra+ would need a cmix-style
  design (capitalisation flags, codes ordered by similarity) and is a separate step.
- **A learned final mixer over the APM outputs (par FM):** the quick dev set gave 97,964 B against 97,857 B, which is
  worse. It stays off (FM = 0).

## Sealed holdout (31 cuts)

Pending: running as one pass after the dev decision.
