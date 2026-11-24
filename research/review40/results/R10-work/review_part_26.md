
### 2.6 LZMA2 lc/lp/pb per kind of data (`run_lclppb.sh`, `lclppb_table.md`)

rc3 base binary, `kolmo -m ratio -l L -T 1` with L = (lc+1)*100 + lp*10 + pb (the encoder hook that already exists
in `src/codecs/lzma.c`; `-l 402` reproduces the default size on all three files). Exact sizes; every output decoded
and compared (`cmp`): all ok. Files where LZMA2 is the kept coder.

| lc lp pb | sao | x-ray | ooffice |
|---|---:|---:|---:|
| rc3 default (-m ratio) | 2,184,759 | 3,616,667 | 2,091,923 |
| 0 0 0 | 2,176,320 (-0.39%) | 3,606,885 (-0.27%) | 2,120,266 (+1.35%) |
| 0 0 2 | 2,176,126 (-0.40%) | 3,607,077 (-0.27%) | 2,118,016 (+1.25%) |
| 0 1 0 | 2,178,321 (-0.29%) | 3,621,570 (+0.14%) | 2,120,776 (+1.38%) |
| 0 1 1 | 2,178,850 (-0.27%) | 3,621,601 (+0.14%) | 2,117,990 (+1.25%) |
| 0 2 0 | 2,185,435 (+0.03%) | 3,637,790 (+0.58%) | 2,122,664 (+1.47%) |
| 0 2 2 | 2,184,829 (+0.00%) | 3,637,957 (+0.59%) | 2,120,189 (+1.35%) |
| 1 0 0 | 2,176,469 (-0.38%) | 3,606,803 (-0.27%) | 2,115,343 (+1.12%) |
| 2 0 0 | 2,178,623 (-0.28%) | 3,608,780 (-0.22%) | 2,106,321 (+0.69%) |
| 2 2 2 | 2,195,588 (+0.50%) | 3,633,872 (+0.48%) | 2,111,385 (+0.93%) |
| 3 0 0 | 2,183,935 (-0.04%) | 3,616,452 (-0.01%) | 2,094,216 (+0.11%) |
| 3 0 1 | 2,183,201 (-0.07%) | 3,616,491 (-0.00%) | 2,095,838 (+0.19%) |
| 3 0 2 | 2,184,759 (+0.00%) | 3,616,667 (+0.00%) | 2,091,923 (+0.00%) |
| 3 1 1 | 2,193,685 (+0.41%) | 3,628,069 (+0.32%) | 2,098,793 (+0.33%) |
| 4 0 0 | 2,192,293 (+0.34%) | 3,626,644 (+0.28%) | 2,091,899 (-0.00%) |
| 4 0 2 | 2,193,107 (+0.38%) | 3,626,856 (+0.28%) | 2,085,304 (-0.32%) |
| **best** | 0 0 2: 2,176,126 (-0.40%) | 1 0 0: 3,606,803 (-0.27%) | 4 0 2: 2,085,304 (-0.32%) |

Reading: a per-kind setting is worth 0.27 to 0.40% and the best setting differs on each file (0/0/2, 1/0/0, 4/0/2);
the best setting of one kind costs up to +1.25% on another. Small, free at decode time, and only reachable per kind:
a planned point (P3), not tonight's work.

### 2.7 The plan byte of a discarded word plan (basis of P5)

`kolmo -m ratio -T 1 dickens.1m a` against `kolmo -m ratio -n -T 1 dickens.1m b` (rc3 base binary; `-n` = no
analysis): both frames are 257,326 bytes and `cmp -l a b` prints exactly one differing byte, offset 49 (1-based):
8 against 0, the plan byte. So the word trial's only trace in a `ratio` frame of text is that byte; the round trip
of `a` was checked.
