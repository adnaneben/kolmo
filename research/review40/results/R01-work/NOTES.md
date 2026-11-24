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
- 24 Nov 04:20Z-05:35Z (second execution session): resumed at 853de09d. Added a154f538 (estimate-based table choice, heap normalisation),
  c0e30b02 (khuf code chunks, levels 1-4), ee0c7ea3 (tANS decode loop), 3dcc5388 (results, tools), 96ad7155 (bindings sync).
  Final natbench rounds: final/v3 (853de09d), v4 (a154f538), v5 (c0e30b02), v6 (ee0c7ea3). check-native OK, determinism 122 frames OK,
  local fuzz driver OK (72 seeds + 300k random). Patches exported (10), claim #4 status implemented. Results: results/R01.md.
- Next in this session: claim #5 klz-w4-compat (lane fetched into the clone as refs/remotes/lanes/w4-lz).
- 24 Nov 05:40Z-06:15Z: claim #5 klz-w4-compat built on the same branch (8eca114d..0f198b7e): lane commits cherry-picked, 0dc7a8b1 ported as
  KLZS codec 31 with rc3's codec 28 kept. natbench round final/w4-* (commit 1fe6a718), e2e-w4.tsv, dec_time-w4-L{1,3}.txt.
  check-native/arch/kernels OK, determinism 122 OK, local fuzz (81 seeds) OK. Negative: conditional refills in codec 32's sequence loop
  (variant R01-exp/tt2/klz-stop-w4b.c) slower: 118.9-122.8 -> 134.8-142.6 ms. Patches re-exported (18), claim #5 status implemented.
