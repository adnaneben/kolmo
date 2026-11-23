# 40-reviewer campaign: coordination directory (started 23 Nov 2026)
Base under review: release-rc3 head 5071f388 (read-only checkout: /home/user/kolmo-review40-base, already built).
- claims.jsonl / tools/claim.py : first-claim-wins registry of improvement points (the owner leads execution).
- reviews/RNN.md   : each reviewer's independent review and plan.
- results/RNN.md   : what the reviewer executed, with measurements (negative results are kept).
- patches/RNN/     : git format-patch series against 5071f388.
- verify/RNN.md    : independent verification of the patch series.
- tools/measure.py : one measured run at a time; wall time, peak RSS, time-average RSS.
Corpora: /home/user/kolmo-review40-data (enwik8, enwik8_10m, silesia/, silesia.tar).
