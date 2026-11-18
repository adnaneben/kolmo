# Concurrency: several Kolmo instances on one machine

Status: implemented on `claude/w4-multi-20261118` (`src/core/jobs.c`). Measurements: `benchmark/w4/multi/RESULTS.md`.
The in-process rules (the memory governor, reservations, one job always runs) are in `docs/MEMORY-MODEL.md`; this
document extends them to every Kolmo process of a user.

## The owner's questions, answered

| question | answer |
|---|---|
| Should several Kolmo instances run at the same time? | **Yes.** Scripts, `xargs -P`, build systems and services start many; refusing or serialising them would be a regression. |
| Should their number be capped? | **No hard cap on instances.** What is capped is the machine's work: a **machine-wide pool** of worker tokens (default: the physical cores) and the memory limit, shared by all instances, in the manner of GNU make's jobserver. Four `kolmo -T 10` on a ten-core machine share ten workers instead of running forty. |
| How are multithreaded runs across instances managed without deadlocks, leaks or other execution issues? | Tokens are flocks the kernel drops when a process dies; every instance always has one worker that never waits for another process; extra workers take a token per block job and give it back when the job ends; nothing ever blocks in the kernel on another process. Details below. |

## Design

### The pool

- **Worker tokens.** A token is an exclusive `flock(2)` on one slot file, `DIR/c.<i>`, i < tokens. Tokens default to
  the physical cores this process may run on (Linux: distinct `thread_siblings_list` among the CPUs of the affinity
  mask; macOS: `hw.physicalcpu`); `KOLMO_JOBS=N` sets another count (1..256). A token is taken with `LOCK_NB` only:
  no thread ever sleeps in the kernel waiting for another process.
- **Memory.** Every process publishes what it has reserved with the governor (bytes, block jobs, tokens) in its own
  record, `DIR/m.<pid>.<n>`, which it keeps exclusively flock'd for its life. The governor reads the others' records
  with its system sample (at most every 100 ms) and limits this process to *the auto limit minus what the others
  hold*. The auto limit is 50% of the machine (or what `--memlimit` / `kolmo_set_memlimit` set), so N instances now
  share 50% of RAM instead of each assuming it has 50%.
- **The directory.** `KOLMO_JOBS_DIR` if set; else `$XDG_RUNTIME_DIR/kolmo-jobs`; else `$TMPDIR` (or `/tmp`)
  `/kolmo-jobs-<uid>`. It must be a directory owned by the user and not writable by group or others (checked with
  `lstat`, so a symlink planted by someone else is refused). `--jobs-shared=system` (`KOLMO_JOBS_SHARED=system`)
  uses `/tmp/kolmo-jobs`, mode 1777, files 0666, shared by every user (a user can then slow another down by holding
  tokens, never stop them: see *Progress*).
- **Fallback.** If the directory cannot be created or used (read-only file system, sandbox, another owner, no flock),
  the pool is off and the in-process governor works alone, exactly as before. `kolmo --jobs-status` says why.

### Crash safety: nothing stale, by construction

A flock belongs to an open file description and disappears when the last descriptor to it closes, which the kernel
does when a process exits, *however* it exits (`kill -9`, OOM killer, crash). So:

- a dead process holds no token, immediately;
- a dead process's record is unlocked; the next reader that manages to lock it knows the owner is dead and removes
  it (every process scans once when it starts, and the governor scans when it needs the figure);
- a record is created under a temporary name `t.<pid>.<n>`, locked, filled, and only then renamed to `m.*`, so a
  reader never sees a live process's record unlocked. A `t.*` file left by a process killed in that instant is removed
  once it is a minute old and unlocked.
- All descriptors are `O_CLOEXEC`: a program Kolmo's caller executes inherits no token.
- `fork()`: the child shares its parent's open file descriptions, so it must neither use nor unlock them. A
  `pthread_atfork` handler closes them in the child (closing a shared description in the child does not release the
  parent's lock) and the child joins the pool afresh with its own record if it uses Kolmo. The same handlers reset the
  thread pool and the governor in the child: before this branch, a process that forked after using Kolmo with several
  threads (Python's `multiprocessing` with the fork start method does exactly that) **hung in the child**, waiting for
  pool threads that only exist in the parent. `tests/test_jobs.c` found it.

No PID liveness check is needed (PIDs are reused; locks are not), and no shared memory or robust mutex: nothing can be
left locked by a dead owner.

### Progress and deadlock freedom

1. **Every instance always runs one worker.** The caller of a pool run (the thread that called `kolmo_compress` etc.)
   is the run's guaranteed worker. Before each block it takes a token if one is free, so that it counts against the
   machine, and runs without one otherwise. It never waits for a token.
2. **Extra block workers** (the pool's helper threads) take a token before each block job and give it back when the job
   ends. While none is free, a helper waits, holding nothing: one waiting thread per process polls the slots (1 ms,
   doubling to 8 ms), the others try again when a token is given back in this process, and all look every 20 ms
   whether blocks remain; when none remain, they stop. As tokens free elsewhere, helpers pick them up again: a run that
   started under contention speeds up when the other instances finish.
3. **Optional extra threads** (the codec trials of `-x`, the CM preset race, the parts of codec 25, the planner's
   second CM sample, the inner parallel loops of `src/algo` (`kolmo_par_for`) and the suffix sorter's crew) only *try*
   for a token, as they already only try for memory. Refused, the work runs on the thread that owns it, afterwards,
   with the same result.
4. **Memory** keeps the governor's rules: a block job waits for its reservation only while this process has another job
   running, so the wait ends when that job ends; the first reservation of a process is granted whatever the others
   hold.
5. **No thread waits on another process.** Cross-process state is only polled (tokens with `LOCK_NB`, records with
   `LOCK_SH|LOCK_NB`), and every waiting thread holds nothing another process could need. A cycle of waits through
   another process is therefore impossible; within a process, the order below holds.

**Lock order** (acquire in this order, release in reverse):

| order | resource | how it is taken |
|---:|---|---|
| 1 | a machine token (`DIR/c.<i>`) | `flock(LOCK_EX\|LOCK_NB)`, under `kj_mu` for the moment of the call |
| 2 | a memory reservation | `kmem_reserve` / `kmem_try_reserve` (`kmem_mu`) |
| 3 | `kj_mu` (pool bookkeeping) | taken by 1, and inside `kmem_mu` to read the others' records (scan, no lock held across it) |

`kmem_mu` is never taken while `kj_mu` is held. A block job: token, then its memory; at its end, memory released, then
the token. A thread inside a reserved job never reserves again (no nested waits), and never waits for a token.

What this costs under oversubscription: when there are more instances than tokens, each instance still runs its
guaranteed worker, so up to *instances* workers run at once (but never *instances × threads*). That is the price of
"every instance always makes progress"; it is the same choice as the in-process rule "one job always runs".

### Determinism

Tokens decide how many threads run, never what they write. The thread counts that reach the format (the threads left
over per block, the CM part count, GR6+'s targets) come from the requested `-T` and the input, not from the tokens
granted; every place that runs work on fewer threads already gave identical bytes for every `-T`, and the optional
work above already ran inline when memory was short. `tests/test_jobs.c` compresses balanced, binary, ratio and `-x`
inputs (several blocks each) with the pool off, then with no contention, with all tokens held by another process,
with three of four held, with the tokens freed in the middle of the run, with four processes compressing at once, with
a pool of one token and with the directory unusable, and requires the same frame every time (and checks that with no
token free exactly one job ran). `tests/stress_jobs.py` requires the reference frame from every one of 16-24
concurrent CLI instances, and `benchmark/w4/multi/run.py` compares every frame between pool on and off.
`benchmark/determinism/check.py` is unchanged: 122 frames, same hashes.

### Output safety (CLI)

- Every output is staged in a private file next to the destination (`mkstemp`, `.kolmo-XXXXXX`: unique, so two
  instances writing the same path never mix bytes) and published only when complete: `link` for a new destination,
  `rename` over an existing plain file, a write-through copy for symlinks, hard links and special files. This now holds
  for every path (in-memory, file, streaming); before, only `balanced`/`speed` files of 8 MiB or more were staged.
- SIGINT, SIGTERM and SIGHUP remove the staged file, then the signal is re-raised (exit status as usual). Tokens and
  the record need no handler: the kernel releases them with the process. After `kill -9` a staged file may stay; the
  destination is never partial.

### Windows (designed, not built here)

Kolmo does not build on Windows yet (pthreads). The same design maps to objects the system releases with the process:
one file `%LOCALAPPDATA%\kolmo\jobs.lock`, a token being `LockFileEx(LOCKFILE_EXCLUSIVE_LOCK |
LOCKFILE_FAIL_IMMEDIATELY)` on byte *i*; a record being a file per process held with an exclusive byte lock. Byte-range
locks belong to the process (not the thread, unlike mutexes, which a different thread could not release), and are
released when it terminates. Named mutexes (`Local\kolmo-jobs-<SID>-<i>`, `WAIT_ABANDONED` on a dead owner) would also
work but are owned by a thread, and Kolmo gives tokens back from the thread that ends the job, which is not always the
one that took it. Until then `_WIN32` builds compile the pool as off.

## Controls

| control | effect |
|---|---|
| `--jobs-shared=on\|off\|system` | the pool for this process: per user (default), off, every user |
| `KOLMO_JOBS_SHARED=on\|off\|system` | the same, from the environment (the API and the option take precedence) |
| `KOLMO_JOBS=N` | tokens in the pool (default: physical cores). Processes with different N share slots 0..min-1 and each honours its own N. |
| `KOLMO_JOBS_DIR=path` | the pool's directory (for example one per container, or one shared by a CI runner's jobs) |
| `kolmo_params.jobs_shared` | per call: 0 = the process setting, negative = this call (and its helper threads) stays out of the pool. Memory sharing follows the process setting. |
| `kolmo_jobs_shared(mode)` | per process: 0 off, 1 user, 2 system, -1 back to the environment; Python `kolmo.jobs_shared("off")`, Rust `jobs_shared(JobsShared::Off)` |
| `kolmo --jobs-status`, `kolmo_jobs_status()` | tokens busy of the total, other live processes with their tokens, jobs and memory, the shared limit; Python `kolmo.jobs_status()`, Rust `jobs_status()` |

```
$ kolmo --jobs-status
machine-wide pool: on (this user), /tmp/kolmo-jobs-501
worker tokens: 10 busy of 10
memory reserved by Kolmo processes: 5.1 GiB (shared limit 12.0 GiB)
processes: 4
  pid 8812: 3 tokens, 3 jobs, 1.3 GiB reserved
  ...
```

## Tests

| test | what |
|---|---|
| `make check-jobs` (in `check-rc`) | `tests/test_jobs.c` (and its ASan/UBSan/LSan build, `--quick`): tokens taken, refused and given back; a child killed with SIGKILL while holding every token and 64 MiB, then 40 children killed at random moments: never a stale token or record; the memory ledger (4 GiB shared, 3 GiB held by another process: 2 GiB refused, 512 MiB granted, all of it free again once that process is killed); a forked child neither shares nor releases its parent's tokens; the frames under contention above. Then `tests/stress_jobs.py --quick`. |
| `tests/stress_jobs.py` | 16 (quick) or 24 CLI instances per round, speed/balanced/ratio/-x and decoding, -T 4 on a 4-token pool, about 40% sent SIGKILL or SIGTERM at random moments: finished instances write the reference bytes, killed ones leave no partial destination, SIGTERM leaves no staged file, the pool is empty after each round, no round hangs. |
