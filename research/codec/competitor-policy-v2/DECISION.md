# Decision P2 — type-first, competitor-bounded, layered compression planning

Approved by the user on 12 November 2026. This is the prospective improvement rule on
`fix/planner-record-ownership-isolated-20261112`, not authorization to merge, release,
change another branch, or reinterpret a wire identifier. Prior registered study
verdicts remain historical evidence. This decision supersedes fixed percentage-gain
and baseline-relative performance bars for NEW research selection/acceptance.

## 1. Objective and comparator

For mode m and original byte-exact input x, choose the smallest legal COMPLETE plan:

    g* = argmin_g StoredBytes(g, x)
         subject to MeasuredCost_m(g, x) <= MeasuredCost_m(rival_m, x)

All framing, lexical reconstruction, validity/escape maps, cross-column dependencies,
dictionaries/model distribution and checksums are in StoredBytes. Encoding cost
includes typing, sampling, graph/column search, rejected trials, model inference,
allocations/copies and final coding. Decoding includes verification and original-byte
reconstruction. A candidate must actually be smaller than the rival to claim a size
win. Equal size and lower cost is a distinct cost win; parity is not improvement.
There is no arbitrary 3%, 5%, 20% or baseline-relative 1.10x minimum/ceiling.

Mode mapping follows the public aliases: speed -> rt, balanced -> fast, ratio -> max.
rt/fast use zstd level 3; eco/hc/max/ultra use OpenZL. This changes the reference, not
which backend implementation is allowed in a mode. LZ4 and other leaders remain
informative controls; beating zstd-3 alone is NOT evidence of beating LZ4.

The preserved repository cost definitions are explicit, so 'within cost' cannot be
quietly switched after observing the outputs:

* Speed: existing TRADEOFFS.md round-trip model, encode + transfer + decode at a
  1-Gbit/s reference link (125,000,000 bytes/s). Report raw encode and decode times
  separately. This scalar budget does not imply each direction is faster. Optional
  application latency/memory SLAs must be registered in advance.
* Archive: existing CPU/storage model: encode once, 3.3 reads, 30-day retention;
  historical parameter defaults are storage $0.023/GB-month and CPU $(3.89/72)/hour.
  Cost = (charged encode + reads * charged decode + amortized training) * CPU rate
  + compressed GB * retention months * storage rate. Optional egress uses the
  existing $0.09/GB parameter with an explicitly registered transfer count (default
  zero, since the original archive comparison reported egress separately).
  These are model parameters from the repository, NOT newly verified market prices.

Cost measurements follow the repository's single-thread rule: charge CPU time rather
than elapsed time where CPU exceeds elapsed by >3%. Pin prices, retention, reads,
link and SLAs before evaluation. Memory/security/random-access requirements remain
hard deployment limits; this policy is not a license to exhaust memory for ratio.

OpenZL must be a pinned build, applicable typed profile, level and graph/training
regime. Preserve the same original-byte boundary and semantic information available
to both systems. Evaluate a declared strong OpenZL frontier, not an arbitrarily weak
profile. Charge training/model storage at the declared deployment reuse horizon on
BOTH sides. Neither training on validation data nor free inline training is allowed.

The old benchmark/accept/TARGETS.md and accept.py remain frozen for historical
reproducibility. ACTIVE_POLICY.json points to this policy. A legacy ACCEPTED message
is not P2 acceptance. No benchmark or release win is established by this commit.

## 2. Type columns FIRST

Order: exact parse/schema -> typed columns + lexical/validity side streams ->
within-column AND cross-column candidate proposals -> layered graph planning ->
leaf coding -> complete frame. Never discard a numeric candidate merely because a
mixed byte histogram was unpromising before typing.

Reuse/audit existing tbl2/typed graph facilities before duplicating them. The shipped
Python typed_columns.py is an executable contract/reference, not a replacement for
the production C parser. It retains exact CSV lexemes/separators, supports quoting,
doubled quotes, CRLF, embedded quoted newlines and trailing empty fields. It treats
empty fields as empty lexical tokens, not invented null semantics. Unsupported/ragged
input becomes one opaque stream. It validates all values before giving a numeric
descriptor. Decimal spelling and exponent forms remain side information; binary
float normalization is not a byte-lossless shortcut. Schema/header intent is explicit.

For non-tables, produce an explicit typed-array or opaque-byte stream descriptor.
For tables, cost the parse even if the selected graph is an identity fallback.
Cross-column equality/offset proposals are bounded and exact on the observed input;
they are not accepted solely on correlation. Shared dictionaries, reference columns,
join keys, exception positions, group schemas and ordering all count. The selected
plan is a joint acyclic graph: independent column winners can be globally suboptimal.
Do not sum costs for a shared node twice, or omit it entirely.

## 3. Layered candidates: preserve order and inverse semantics

Initial bounded registry (existing primitives first):

| Family | Legal sequence | Main control |
|---|---|---|
| Numeric native | typed int -> predictor/GCD -> residual planes -> compatible HUF/rANS chunks + singleton handling -> frame | unchanged typed native path |
| Numeric packing | typed int -> predictor/FOR -> local or patched packing -> terminal packing OR suitable entropy stage | no universal 'pack then entropy' assumption |
| Sparse residual | typed int -> predictor -> zero map/run positions + nonzero values -> magnitude tokens/exact offsets -> separate coding | dense and random controls |
| Patched residual | residual -> width metadata / low bits / exception positions / high parts -> suitable separate coders | existing interleaved patched representation |
| Related columns | exact column group -> shared dictionary or cross-column residual + exact escapes -> per-stream numeric/categorical encoding | independent columns including schema cost |
| Repeated records | typed batch -> shared model/table setup -> per-stream coding | latency/random-access/reset budgets |

Plane alignment and bitpacking are usually alternative representations of the same
residual; do not compose them in an order that destroys the meaning of the plane
boundaries. Keep compatibility-preserving leaves separate from experiments requiring
new signaling. Reusing tables or changing a graph operation requires a declared
format contract. No new IDs or production format changes are made here.

Search retains Pareto candidates, not only a standalone leaderboard. Evaluate base,
A, B and A+B on the same byte/cost boundary, plus conditional contributions inside a
retained larger stack. Overlap, enabling effects, and contextual amplification are
all legitimate. Low standalone gain is not automatic rejection; neither is it proof
of a valuable conjunction. Report full-cohort, applicable-subset and family-balanced
results; do not pool nested prefixes as independent families.

## 4. Learning strategy

RL is a candidate planner technology, not a prerequisite for a better codec. Start
with a deterministic bounded graph/beam search and measured cost/size predictors.
Then compare a supervised ranker or small contextual bandit for a finite candidate
menu. Only promote sequential constrained RL when multi-step graph dependencies give
it incremental value beyond those cheaper controls at the same total cost budget.

State: typed schemas, distribution sketches, partial graph, available cross-column
references, remaining cost/memory budget, recursion depth and compatibility version.
Actions: legal reversible node/parameters, split/group, terminate/identity fallback.
Reward: final complete bytes saved relative to the designated rival, with explicit
cost constraints. An infeasible graph must be rejected, not 'paid for' by a large
size reward. A finite reward penalty alone cannot enforce the user's hard budget.
Use action masks, bounded graph size, conservative resource admission and a safe
fallback; measure total realized cost independently.

Train offline on DEVELOPMENT traces with reproducible graph and measurement hashes.
Log context/action probabilities for bandit exploration. No deployment-time random
exploration on user files. The model runs only in the encoder; store the explicit
reversible plan so the decoder needs no ML model. Measure inference, graph search,
training amortization and distribution overhead. A runtime estimator cannot promise
an exact wall-clock budget on every unseen input; distinguish admission estimates
from measured promotion evidence and enforce practical runtime limits separately.

OpenZL's 'Compression Transformer' is an encoder-only small-MLP graph scorer in its
current primary description, not proof that a large Transformer or RL is required.
Any RL proposal must beat that kind of inexpensive selection baseline as well as the
deterministic implementation, using fresh source-disjoint validation.

## 5. Implemented now vs outstanding

Implemented: policy.py validates comparable whole-pipeline records and selects the
smallest feasible candidate under the new rule. It rejects missing competitors,
wrong levels, incomparable input/machine/scope, untyped planning, incomplete work
accounting, unverified reconstruction and development-only acceptance claims.
It checks all paired repeated costs; this conservative check is NOT a confidence
interval. typed_columns.py provides exact parse/type/reconstruction and bounded
cross-column offset proposals. test_policy.py exercises these reference contracts.

Not implemented here: production C planner integration, compression of the lexical
side streams, learned models, a full graph search engine or new rival measurements.
Caller-provided verification fields are evidence contracts, not self-proving facts.
An offline smallest-of-measured-plans oracle must not be deployed as a 'free' planner:
a realizable search policy must be timed end to end including its unsuccessful trials.

R3 and earlier component measurements lack the matched whole-file zstd-3/OpenZL
records needed by P2. Preserve their old decisions. Reopen candidates for evaluation
under P2, but do not relabel 14.70% native-component savings as a competitive win.

Next executable task: adapt the existing C typed-column pipeline to export exact
schema/lexical side streams and complete candidate-DAG traces; run the strongest
retained native/hybrid stacks and matched zstd-3/OpenZL controls in one end-to-end
harness. Freeze mode cost parameters and the competitor configuration registry first.
Only after that baseline exists should an RL experiment consume engineering effort.

## Primary references and source identities

Repository reference: 3919f5ccbc573fde549eb27c7e5d22373d658fab.
- benchmark/TRADEOFFS.md blob 9beecac6bff56fe4dd5e84338cccefddc8d5bbdc.
- benchmark/tradeoff_model.py blob d9fda29ef71a21acb65b447c9a6b5cb5ac1d3977.
- benchmark/accept/TARGETS.md blob 0ead740705a96bf0f08caef133f70aba8b35eac1.
- README.md modes/typed-column overview; benchmark/openzl/README.md rival methodology.
- https://openzl.org/getting-started/examples/c/custom-formats/
- https://openzl.org/blog/2026-09-24-compression-transformer/
- https://proceedings.mlr.press/v70/achiam17a
- https://vowpalwabbit.org/docs/vowpal_wabbit/python/latest/tutorials/python_Contextual_bandits_and_Vowpal_Wabbit.html

References motivate mechanisms only; their reported performance is not Kolmo evidence.
