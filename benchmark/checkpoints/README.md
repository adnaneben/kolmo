# Recoverable milestones

Keep these checkpoints immutable. They are source overlays on Git revision `31ce410ec419ccf7039724b32d0656db40afcee8`, not replacements for the repository or its vendored dependencies.

1. `cpu-planning-v1/source.tar.gz` preserves the earlier CPU planning and correctness improvements. Its manifest hashes every saved file and the reference native library. The native library is a local machine-specific reference and is ignored by Git; the source archive is not ignored.
2. `independent-gpu-v1/source.tar.gz` preserves the standalone codec, original and optimized Metal kernels, hybrid executor, tests, benchmark and executive documentation. Apply it after the CPU checkpoint; it includes the complete updated Makefile, README and CI file.
3. `byte-context-v1/source.tar.gz` preserves the improved independent byte-context candidate, standalone CLI, correctness tests and real lzbench integration. Apply it after the independent/GPU checkpoint. It retains the earlier codec as a distinct API/format and adds separate targets. Executive status and final evidence evolve outside this immutable source snapshot.

To recover, create a separate checkout of the pinned revision, verify each archive's SHA-256 against its manifest, extract the CPU overlay there, and then apply the independent/GPU and byte-context overlays in that order if desired. Rebuild on the target machine. Do not extract over an active checkout containing newer work. All archives contain repository-relative paths only. Each manifest records the required overlay order and every restored file hash.

`benchmark/next/protect_gains.py --sources-only` enforces the CPU checkpoint. The full local gate additionally needs the saved dylib and benchmark inputs. `benchmark/ratio/protect_experiments.py` enforces the earlier KRC1/GPU sources and raw evidence. `benchmark/next/final-manifest.json` records binary hashes and evidence files for the independent/GPU milestone. Experimental checkpoints preserve research results; they do not imply that the experiment was promoted to normal Kolmo.
