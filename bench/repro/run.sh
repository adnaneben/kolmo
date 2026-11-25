#!/bin/sh
# One command for the whole public benchmark: fetch, build, measure, write JSON, regenerate the page.
#
#   sh bench/repro/run.sh WORK [--ref v0.0.2] [--steps fetch,build,lzbench,panel,mt,dfloat11,site] [--quick]
#
# WORK       a directory with room for ~6 GB (data, tools, results). Re-running resumes: finished steps and
#            measured files are skipped only when the run identity matches.
# --ref      the kolmo tag or commit to measure (default: the tag of VERSION); WORKTREE for the working tree.
# --steps    a subset of the steps, in this order.
# --quick    a smoke test: small lzbench lists on Canterbury only, one repetition, no weights.
#
# Environment:
#   PYTHON            interpreter with bench/repro/requirements.txt installed (default: python3)
#   KOLMO_BENCH_TIMED a command prefix that serialises timed work, e.g. a lock script (default: none). Every
#                     timed step and every build runs under it, so nothing else heavy runs at the same time.
#
# Outputs: WORK/results/{tools.json, FETCHED.json, lzbench/*.csv + lzbench.json, panel.json, mt.json,
#          dfloat11.json}; then site/results.json and site/index.html are regenerated from them.
# Fairness: every tool runs on one thread (except the explicitly all-core rows of the mt step); kolmo is built
# with `make ARCH=`; each round trip is checked byte for byte. See bench/repro/README.md.
set -e
WORK=${1:?usage: run.sh WORK [--ref REF] [--steps a,b] [--quick]}; shift
here=$(cd "$(dirname "$0")" && pwd); root=$(cd "$here/../.." && pwd)
REF="v$(cat "$root/VERSION")"; STEPS=fetch,build,lzbench,panel,mt,dfloat11,site; QUICK=
while [ $# -gt 0 ]; do
    case $1 in
        --ref) REF=$2; shift 2 ;;
        --steps) STEPS=$2; shift 2 ;;
        --quick) QUICK=--quick; shift ;;
        *) echo "run.sh: unknown option $1"; exit 1 ;;
    esac
done
PY=${PYTHON:-python3}; T=${KOLMO_BENCH_TIMED:-}
mkdir -p "$WORK/results"; WORK=$(cd "$WORK" && pwd); D=$WORK/data; R=$WORK/results
lock=$R/.run.lock
if ! mkdir "$lock"; then
    echo "run.sh: WORK is locked ($lock)" >&2
    exit 1
fi
trap 'rmdir "$lock"' EXIT
trap 'exit 1' HUP INT TERM
MODE=full; [ -z "$QUICK" ] || MODE=quick
REF=$("$PY" -I "$here/identity.py" prepare "$root" "$WORK" "$REF" "$MODE")
identity() { "$PY" -I "$here/identity.py" "$1" "$root" "$WORK" "$REF" "$MODE"; }
has() { case ",$STEPS," in *",$1,"*) return 0 ;; esac; return 1; }
log() { echo "$(date -u +%FT%TZ) $*" | tee -a "$R/run.log"; }

if has fetch && [ ! -f "$R/input-identity.json" ]; then
    log "fetch corpora"; python3 -I "$root/bench/lzbench/corpora.py" "$D/corpus" > "$R/corpora.log"
    groups=holdout,weights; [ -n "$QUICK" ] && groups=holdout
    log "fetch $groups"; "$PY" -I "$here/fetch.py" "$D" --groups "$groups"
    cp "$D/FETCHED.json" "$R/FETCHED.json.part"
    mv "$R/FETCHED.json.part" "$R/FETCHED.json"
fi
if has build && [ ! -f "$R/tools.json" ]; then
    log "build (kolmo $REF)"; $T sh "$here/build.sh" "$WORK/tools" "$REF"
    cp "$WORK/tools/tools.json" "$R/tools.json.part"
    mv "$R/tools.json.part" "$R/tools.json"
fi
identity tools
if has lzbench || has panel || has mt; then
    if [ ! -f "$R/tools.json" ] || [ ! -d "$D" ]; then
        echo "run.sh: set up tools and data before running benchmarks" >&2
        exit 1
    fi
fi
if has lzbench || has panel || has mt || has dfloat11; then
    identity inputs
fi
if has lzbench; then
    log "lzbench"; $T python3 -I "$here/lzbench_run.py" "$R/tools.json" "$D/corpus" "$R/lzbench" $QUICK
fi
if has panel; then
    sets=; reps=3
    [ -n "$QUICK" ] && sets="--sets nasa-logs,silesia-sao" && reps=1
    log "panel and OpenZL rematch"; $T "$PY" -I "$here/panel.py" "$R/tools.json" "$D" "$here/suite.json" "$R/panel.json" --reps $reps $sets
fi
if has mt; then
    n=3; [ -n "$QUICK" ] && n=1
    log "multi-thread"; $T python3 -I "$here/mt_run.py" "$R/tools.json" "$D/corpus" "$R/mt.json" --n $n
fi
if has dfloat11 && [ -z "$QUICK" ] && ls "$D"/weights/*-bf16.safetensors > /dev/null 2>&1; then
    log "DFloat11 method size"; "$PY" -I "$here/dfloat11_tool.py" "$D"/weights/*-bf16.safetensors "$R/dfloat11.json"
fi
if has site; then
    log "site"; python3 -I "$root/site/build.py" --results "$R"
fi
log "done: $R"
