# E02C memory measurement correction

This note supersedes the initial README's interpretation of `peak_process_rss_kib`.

The first successful full experiment (run 43232093686, commit c880659f4490bfe49831411fb30f91bb8af1e321) reported approximately 187,120-189,936 KiB for its native probe processes. That value is NOT a valid estimate of the post-exec C codec's resident working memory. Linux `getrusage(RUSAGE_SELF).ru_maxrss` in this execution path retains a high-water observation from the launcher before exec.

A separate local reproduction launched the same tiny native program from Python before and after touching a 200 MiB allocation. Its ru_maxrss changed from 95,120 to 299,920 KiB; `/proc/self/status` VmHWM stayed approximately 644-648 KiB. These are measurements of that reproducer, not Kolmo results.

The research probe now records BOTH:
- `pre_exec_inclusive_ru_maxrss_kib`: retained for diagnosis only, not codec comparisons.
- `post_exec_image_peak_rss_kib`: Linux `/proc/self/status` VmHWM of the executing native image.

The corrected high-water observation still includes the input, capture streams, retained reference frame, allocations, library/runtime and output exports. It is not an isolated coder scratch estimate, not a theoretical upper bound and not separate encode/decode peaks. No memory advantage is claimed until corrected measurements are reviewed.

The change occurs after encoding/decoding and outside the timing intervals; it does not change the codec, the corpus, parameters or frame semantics. The initial sizes, frame hashes, correctness checks and descriptive times remain historical evidence. Keep the first run and explicitly compare subsequent frame/capture/corpus identities. Do not overwrite its logs or present its old RSS column as corrected data.

At this correction's publication, re-execution of the amended probe is pending. Check the actual E02C workflow status and evidence.
