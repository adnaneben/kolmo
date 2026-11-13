#!/usr/bin/env python3
"""Offline reference selector for competitor-bounded COMPLETE lossless pipelines.

This does not modify Kolmo's production planner. Inputs are externally measured
candidate pipelines, not isolated codec kernel times or a free per-input oracle.
All times are seconds; sizes are full frame bytes. No third-party dependencies.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass, asdict
import json
import math
from pathlib import Path
from statistics import median
from typing import Any

ALIASES = {"speed": "rt", "balanced": "fast", "ratio": "max"}
SPEED = {"rt", "fast"}
ARCHIVE = {"eco", "hc", "max", "ultra"}


@dataclass(frozen=True)
class CostModel:
    # Historical repository parameters, NOT a current cloud-price quotation.
    link_bytes_per_second: float = 125_000_000.0  # TRADEOFFS.md: 1 Gbit/s
    retention_days: float = 30.0
    reads: float = 3.3
    storage_usd_per_gb_month: float = 0.023
    vcpu_usd_per_hour: float = 3.89 / 72
    egress_usd_per_gb: float = 0.09
    transfers: float = 0.0  # Original archive CPU/storage model; egress optional.

    def validate(self) -> None:
        for key, value in asdict(self).items():
            if not isinstance(value, (int, float)) or isinstance(value, bool) or not math.isfinite(value) or value < 0:
                raise ValueError(f"invalid cost parameter {key}")
        if not self.link_bytes_per_second or not self.retention_days:
            raise ValueError("link and retention must be positive")
        if not self.storage_usd_per_gb_month and not self.vcpu_usd_per_hour:
            raise ValueError("archive cost cannot have zero storage and CPU prices")


def finite(value: Any, name: str, *, positive: bool = False) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError(f"{name}: expected finite number")
    if value < 0 or (positive and value == 0):
        raise ValueError(f"{name}: invalid sign/zero")
    return float(value)


def integer(value: Any, name: str, *, positive: bool = False) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < int(positive):
        raise ValueError(f"{name}: expected {'positive' if positive else 'nonnegative'} integer")
    return value


def digest(value: Any, name: str) -> str:
    if not isinstance(value, str) or len(value) != 64 or any(c not in '0123456789abcdef' for c in value):
        raise ValueError(f"{name}: expected lowercase SHA-256")
    return value


def charged(wall: Any, cpu: Any) -> float:
    w, c = finite(wall, "wall", positive=True), finite(cpu, "cpu")
    return c if c > 1.03 * w else w  # existing single-thread charging rule


def run_cost(row: dict, size: int, speed: bool, model: CostModel) -> float:
    e = charged(row['encode_wall_s'], row['encode_cpu_s'])
    d = charged(row['decode_wall_s'], row['decode_cpu_s'])
    amortized = finite(row.get('amortized_training_s', 0), 'amortized_training_s')
    if speed:
        return e + d + amortized + size / model.link_bytes_per_second
    return ((e + amortized + model.reads * d) * model.vcpu_usd_per_hour / 3600
            + size / 1e9 * (model.retention_days / 30 * model.storage_usd_per_gb_month
                           + model.transfers * model.egress_usd_per_gb))


def measured(candidate: dict, context: dict, speed: bool, model: CostModel,
             *, rival: bool = False) -> dict:
    """Reject incomparable, partly accounted or unverified records; never invent costs."""
    cid = candidate.get('id')
    if not isinstance(cid, str) or not cid:
        raise ValueError('candidate id missing')
    for field in ('input_sha256', 'comparison_contract_sha256', 'machine_id', 'threads'):
        if candidate.get(field) != context.get(field):
            raise ValueError(f'{cid}: mismatched {field}')
    digest(candidate['input_sha256'], 'input_sha256')
    digest(candidate['comparison_contract_sha256'], 'comparison_contract_sha256')
    if candidate.get('measurement_scope') != 'complete_pipeline':
        raise ValueError(f'{cid}: leaf or component measurements cannot establish this policy')
    if candidate.get('byte_exact_verified') is not True:
        raise ValueError(f'{cid}: missing exact reconstruction')
    if candidate.get('all_work_included') is not True:
        raise ValueError(f'{cid}: include typing, planning, trials, copies, checksums and model inference')
    if candidate.get('model_cost_accounted') is not True:
        raise ValueError(f'{cid}: model distribution and training/reuse costs must be declared')
    if not rival:
        digest(candidate.get('typed_schema_sha256'), 'typed_schema_sha256')
        if candidate.get('typing_precedes_planning') is not True:
            raise ValueError(f'{cid}: columns must be typed before plan/leaf selection')
        if candidate.get('legal_dag_verified') is not True:
            raise ValueError(f'{cid}: missing reversible, acyclic graph verification')
        if candidate.get('source_partition') != 'fresh_validation':
            raise ValueError(f'{cid}: observed development data is not fresh acceptance')
    size = integer(candidate.get('stored_bytes'), 'stored_bytes')
    peak = integer(candidate.get('peak_memory_bytes'), 'peak_memory_bytes')
    if peak > integer(context.get('memory_limit_bytes'), 'memory_limit_bytes', positive=True):
        raise ValueError(f'{cid}: exceeds registered memory budget')
    if not candidate.get('implementation_id') or not candidate.get('configuration_id'):
        raise ValueError(f'{cid}: unpinned implementation/configuration')
    rows = candidate.get('runs')
    if not isinstance(rows, list) or len(rows) < 3:
        raise ValueError(f'{cid}: at least three paired repeated runs required')
    costs = [run_cost(r, size, speed, model) for r in rows]
    for key in ('max_encode_s', 'max_decode_s'):
        if key in context:
            limit = finite(context[key], key, positive=True)
            op = 'encode' if key == 'max_encode_s' else 'decode'
            if max(charged(r[op + '_wall_s'], r[op + '_cpu_s']) for r in rows) > limit:
                raise ValueError(f'{cid}: exceeds optional registered {op} SLA')
    ids = [r.get('pair_id') for r in rows]
    if any(not isinstance(x, str) or not x for x in ids) or len(set(ids)) != len(ids):
        raise ValueError(f'{cid}: repeated/missing pair IDs')
    return {'id': cid, 'bytes': size, 'costs': costs, 'pairs': ids,
            'cost_median': median(costs), 'peak_memory_bytes': peak}


def select(document: dict) -> dict:
    """Smallest eligible complete measured plan. No new % improvement threshold.

    All paired candidate costs must fit their rival pair's measured budget. This
    is a conservative verification rule, NOT a statistical confidence guarantee.
    A tied/larger file cannot be claimed as a size victory just for fitting cost.
    """
    if document.get('schema_version') != 2:
        raise ValueError('expected schema_version=2')
    mode = ALIASES.get(document.get('mode'), document.get('mode'))
    if mode not in SPEED | ARCHIVE:
        raise ValueError('unknown mode; do not guess its competitor')
    model = CostModel(**document.get('cost_model', {})); model.validate()
    context = document['context']
    if context.get('threads') != 1 or not context.get('machine_id'):
        raise ValueError('this reference evaluator requires one registered machine and thread')
    integer(context.get('input_bytes'), 'input_bytes', positive=True)
    competitor = document.get('competitor')
    if not isinstance(competitor, dict):
        return {'status': 'BLOCKED_MISSING_COMPETITOR', 'mode': mode, 'selected': None}
    speed = mode in SPEED
    if (speed and (competitor.get('tool') != 'zstd' or competitor.get('level') != 3)) or (not speed and competitor.get('tool') != 'openzl'):
        raise ValueError('wrong designated competitor')
    if not speed:
        if not competitor.get('profile_id') or not competitor.get('training_regime'):
            raise ValueError('OpenZL profile/graph and training regime must be frozen')
    rival = measured(competitor, context, speed, model, rival=True)
    records = document.get('candidates')
    if not isinstance(records, list):
        raise ValueError('candidates must be a list')
    seen, eligible, rejected = {rival['id']}, [], []
    for candidate in records:
        cid = candidate.get('id')
        if cid in seen:
            raise ValueError('duplicate candidate identifier')
        seen.add(cid)
        try:
            m = measured(candidate, context, speed, model)
            if m['pairs'] != rival['pairs']:
                raise ValueError(f'{cid}: unmatched paired runs')
            if any(c > r for c, r in zip(m['costs'], rival['costs'])):
                raise ValueError(f'{cid}: measured competitor cost exceeded')
            eligible.append(m)
        except (ValueError, KeyError, TypeError) as exc:
            rejected.append({'id': cid, 'reason': str(exc)})
    # Stable tie-breaking: bytes, median cost, peak memory, identifier.
    selected = min(eligible, key=lambda x: (x['bytes'], x['cost_median'], x['peak_memory_bytes'], x['id'])) if eligible else None
    status = 'NO_FEASIBLE_CANDIDATE'
    if selected:
        if selected['bytes'] < rival['bytes']:
            status = 'MEASURED_SIZE_WIN'
        elif selected['bytes'] == rival['bytes'] and selected['cost_median'] < rival['cost_median']:
            status = 'MEASURED_COST_WIN_AT_SIZE_PARITY'
        elif selected['bytes'] == rival['bytes']:
            status = 'PARITY_NOT_IMPROVEMENT'
        else:
            status = 'FEASIBLE_BUT_NOT_COMPETITIVE_IN_SIZE'
    return {'policy': 'competitor-bounded-v2', 'mode': mode, 'status': status,
            'selected': selected, 'competitor': rival, 'eligible': eligible, 'rejected': rejected,
            'cost_model': asdict(model), 'cost_unit': 'seconds_at_1Gbit_link' if speed else 'USD_over_registered_lifetime',
            'limitations': ['Measured offline complete-pipeline comparison, not an implemented online search.',
                            'All-pair cost check is conservative; it is not a confidence interval or timing guarantee.',
                            'A win on this registered scope is not release approval or universal leadership.']}


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('measurements', type=Path)
    p.add_argument('--output', type=Path)
    a = p.parse_args()
    try:
        result = select(json.loads(a.measurements.read_text()))
    except (ValueError, KeyError, TypeError, OSError) as exc:
        p.exit(2, f'invalid comparison: {exc}\n')
    text = json.dumps(result, indent=2, allow_nan=False) + '\n'
    if a.output:
        a.output.write_text(text)
    else:
        print(text, end='')


if __name__ == '__main__':
    main()
