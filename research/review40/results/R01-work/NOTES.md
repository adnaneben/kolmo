# R01 stage 2 working notes (klzh-entropy-v2)

- 24 Nov 02:20Z: resumed. /home/user/kolmo-review40-wt/R01 is a CLONE of /home/user/kolmo-review40-base (made in stage 1),
  not a registered worktree; branch review40/R01 created inside it at 5071f388. Stage-1 tools: /home/user/kolmo-review40-wt/R01-exp.
- Plan: phase 1 = token layout + input-aligned sub-blocks on khuf blocks (codec 32); phase 2 = compact/reused tables for small inputs.
- 02:45Z phase 1 (commit f08302a5; khuf chunks, token layout, 64 KiB grid): 13 files, 4 MiB blocks: k2h1 72,260,473 (klzh1 73,541,314; zstd1 77,410,318),
  k2h3 70,010,861 (klzh3 71,516,330; zstd3 69,941,272). Raw: R01-exp/out/p1/v1-T1-*.txt.
- 03:00Z tANS for the four code streams (2 states, bit-reversal spread): xml+dickens+samba+nci level 3: 12,264,540 -> 12,224,397 (-0.33%).
- 03:10Z experiment (exp-streams-level{1,3}.txt, instrumentation diff exp-stats-instrumentation.diff, not committed): raw extra bits are 50-62% of output.
  Entropy bounds per 4 MiB block: the low 4 bits of new offsets are far from uniform on binary data (mozilla 985,335 -> 660,568 bytes),
  finer match-length codes (48 direct values, then 2 codes per octave) save 56 KiB on nci. Both go into the format.
- 02:50Z decode loop variants, natbench best of 9, 1 thread, mozilla+samba+dickens+nci+xml (121,918,051 B), level 3, three rounds, MB/s (indicative):
  branchy selects (kept) 904 / 927 / 933; masks for ll/ml selects + branchy offsets 840 / 836 / 817; fully branch-free (masks + 4-entry rep table) 770 / 803 / 745;
  codec 29 in the same runs 1033 / 1036 / 1005. NEGATIVE: removing the data-dependent branches made the loop slower here (callgrind: mispredictions 311k -> 156k
  on 8 MiB of mozilla, instructions 83.4M -> 103.5M). The branchy loop stays.
- tANS vs khuf for the code streams (level 3, same 5 files): tANS 30,113,374 B at 703-751 MB/s decode; khuf blocks for chunks >= 2048 symbols 30,221,466 B (+0.36%) at 839-843 MB/s
  (measured with the mask loop); codec 29 31,685,855 B at 944-1026 MB/s; zstd -3 30,451,885 B at 932-1149 MB/s.
