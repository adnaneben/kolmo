# Repetitive-input correction through installed Python and Rust packages

The exact guard commit is `516e1dca02ac8165ed91ef5068af84e44335a109`, based on integration `258ca7b60db43cfdfba308c703b2e576cb628fa1`. Encoder settings were Ultra, one thread, a 256 MiB memory limit and automatic block selection. The ordinary and experimental packages each came from a clean checkout of that commit. The wheel was installed into an isolated target, the crate unpacked and built offline, and the helper programs called the public memory APIs.

| Input | Stock or experimental flag unset/0 | Experimental =1 | Before guard, experimental =1 |
|---|---:|---:|---:|
| Repeated four-line fixture, 8,437,837 B | 29,050 B | 29,050 B | 1,851,143 B |
| Existing positive control, 8,651,356 B | 1,658,579 B | 1,351,098 B | 1,351,098 B |

Both native bindings and the CLI emit the exact stock digest on the repetitive fixture. On the positive control, the compiled option with an explicit runtime value of `1` saved 18.54%. The compiled default, an unset runtime variable and an explicit `0` retained the baseline digest. The two selected profiles exercise different paths. These figures describe frame sizes. No new timing claim is made.

The [matrix](matrix.csv) contains 30 complete released-reader readbacks, spanning six settings per consumer in the default Python and Rust packages and the opt-in Python, Rust and CLI builds. Each public binding consumer also decompresses its own result. A separately built v0.0.2 CLI on two threads independently restored every complete frame and verified its input bytes. All 176 eligible native files in each unpacked crate and each Python build source matched the canonical source. The installed wheel library matched its original archive member. The [manifest](manifest.json) identifies all packages, the released reader and builds. The earlier pre-fix results and synthetic input generator are in the [binding baseline](../README.md). The existing positive control is identified by complete input SHA and offset in the [Ultra option measurements](../../fastq/review/README.md).

The [check program](check.py) accepts a prebuilt CLI, installed Python target, compiled Rust consumer, both inputs and stock frames, the two predeclared guarded frame digests and a release reader. Compile the opt-in Python library with a C compiler command adding `-DKOLMO_FASTQ_RECORD_BLOCKS` and the Rust native library with that same flag in `CFLAGS`. Compile the ordinary packages with standard options. Use the existing [Python consumer](../consumer.py) and [Rust consumer](../consumer.rs). One invocation per build and consumer tests unset, `0`, and `1` in separate processes. Frame digests and restored input bytes are checked before each result is recorded.
