"""Byte-preserving column-first RESEARCH adapter, not a production CSV codec.

Supports one-byte delimiters, quoted fields with doubled quotes, LF/CRLF,
embedded quoted newlines, trailing empty fields and no final newline. It keeps
original lexemes/separators; types describe values but never normalize bytes.
Ragged/malformed/unsupported inputs become one opaque byte stream.
"""
from __future__ import annotations

from dataclasses import dataclass
from decimal import Decimal, InvalidOperation
import hashlib
import json
import re

INTEGER = re.compile(rb'-?(?:0|[1-9][0-9]*)\Z')
NUMBER = re.compile(rb'[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?\Z')
MAX_BYTES, MAX_COLUMNS, MAX_CELLS = 16 << 20, 4096, 1_000_000


@dataclass(frozen=True)
class Cell:
    raw: bytes
    value: bytes
    quoted: bool


@dataclass
class TypedTable:
    rows: list[list[Cell]]
    separators: list[list[bytes]]
    columns: list[dict]
    opaque: bytes | None = None
    fallback_reason: str | None = None

    def restore(self) -> bytes:
        if self.opaque is not None:
            return self.opaque
        return b''.join(c.raw + sep for row, seps in zip(self.rows, self.separators)
                        for c, sep in zip(row, seps))

    def schema(self) -> dict:
        # No data samples or filenames: schema is a type contract, not a dataset ID.
        return {'version': 1, 'contract': 'byte_exact_lexemes', 'columns': self.columns,
                'row_count': len(self.rows), 'opaque': self.opaque is not None}

    def schema_sha256(self) -> str:
        b = json.dumps(self.schema(), sort_keys=True, separators=(',', ':')).encode()
        return hashlib.sha256(b).hexdigest()


def classify(values: list[bytes]) -> dict:
    nonempty = [x for x in values if x]
    empty = len(values) - len(nonempty)
    out = {'physical': 'original_lexemes', 'count': len(values), 'empty_lexemes': empty,
           'null_semantics': 'not_inferred', 'validated_all_values': True}
    ints = []
    canonical = bool(nonempty)
    for x in nonempty:
        if len(x) > 21 or not INTEGER.fullmatch(x) or x == b'-0':
            canonical = False; break
        ints.append(int(x))
    if canonical and min(ints) >= -(1 << 63) and max(ints) < 1 << 64:
        if min(ints) < 0 and max(ints) >= 1 << 63:
            canonical = False
        else:
            out.update(logical='int64' if min(ints) < 0 else 'uint64', minimum=min(ints), maximum=max(ints),
                       lexical_side_stream_required=bool(empty))
            return out
    # Decimals/exponent spellings are described, not converted to binary floats.
    numeric = bool(nonempty)
    for x in nonempty:
        if len(x) > 128 or not NUMBER.fullmatch(x):
            numeric = False; break
        try:
            d = Decimal(x.decode('ascii'))
            if not d.is_finite() or abs(d.as_tuple().exponent) > 1_000_000:
                numeric = False; break
        except (ValueError, InvalidOperation, UnicodeError, OverflowError):
            numeric = False; break
    out.update(logical='decimal_lexical' if numeric else 'bytes', lexical_side_stream_required=True)
    # Dictionary is only a proposal; a table and exceptions still have to earn their cost.
    sample = nonempty[:4096]
    out['dictionary_candidate'] = bool(sample) and len(set(sample)) * 4 <= len(sample)
    out['cardinality_sample_count'] = len(sample)
    out['cardinality_sample_distinct'] = len(set(sample))
    return out


def parse(data: bytes, delimiter: bytes = b',', *, header: bool = False) -> TypedTable:
    """Type before graph/leaf planning. Header choice is explicit, never guessed."""
    if not isinstance(data, bytes):
        raise TypeError('input must be bytes')
    if len(delimiter) != 1 or delimiter in (b'"', b'\r', b'\n'):
        raise ValueError('unsupported delimiter')
    def opaque(reason: str) -> TypedTable:
        return TypedTable([], [], [{'logical': 'opaque_bytes', 'physical': 'bytes', 'validated_all_values': True}], data, reason)
    if not data:
        return opaque('empty_input')
    if len(data) > MAX_BYTES:
        return opaque('research_input_limit')
    rows, separators, row, seps = [], [], [], []
    i, total = 0, 0
    try:
        while True:
            start = i; quoted = i < len(data) and data[i] == 34
            if quoted:
                i += 1; value = bytearray()
                while i < len(data):
                    c = data[i]
                    if c == 34:
                        if i + 1 < len(data) and data[i + 1] == 34:
                            value.append(34); i += 2; continue
                        i += 1; break
                    value.append(c); i += 1
                else:
                    raise ValueError('unclosed_quote')
                if i < len(data) and data[i:i+1] not in (delimiter, b'\r', b'\n'):
                    raise ValueError('characters_after_quote')
                val = bytes(value)
            else:
                while i < len(data) and data[i:i+1] not in (delimiter, b'\r', b'\n'):
                    if data[i] == 34:
                        raise ValueError('quote_in_unquoted_field')
                    i += 1
                val = data[start:i]
            row.append(Cell(data[start:i], val, quoted)); total += 1
            if len(row) > MAX_COLUMNS or total > MAX_CELLS:
                raise ValueError('research_shape_limit')
            if i == len(data):
                seps.append(b''); rows.append(row); separators.append(seps); break
            if data[i:i+1] == delimiter:
                seps.append(delimiter); i += 1; continue
            if data[i:i+2] == b'\r\n':
                sep = b'\r\n'; i += 2
            elif data[i:i+1] == b'\n':
                sep = b'\n'; i += 1
            else:
                raise ValueError('bare_carriage_return')
            seps.append(sep); rows.append(row); separators.append(seps); row, seps = [], []
            if i == len(data):
                break
        if any(len(r) != len(rows[0]) for r in rows):
            raise ValueError('ragged_rows')
        body = rows[1:] if header else rows
        cols = []
        for index in range(len(rows[0])):
            desc = classify([r[index].value for r in body])
            # Quoting and delimiters always remain exact reconstruction information.
            desc.update(index=index, quoted_cells=sum(c[index].quoted for c in body),
                        header_preserved=header, framing_side_stream_required=True)
            cols.append(desc)
        table = TypedTable(rows, separators, cols)
        if table.restore() != data:
            raise ValueError('internal_reconstruction_mismatch')
        return table
    except ValueError as exc:
        return opaque(str(exc))


def relation_proposals(table: TypedTable, *, header: bool = False, max_pairs: int = 64) -> list[dict]:
    """Exact same-row integer offsets on this input; forward references form a DAG.

    This only proposes candidates, not selects or compresses them. Replacing the
    source data requires a measured full group plan and exact inverse metadata.
    """
    if max_pairs < 0 or max_pairs > 4096:
        raise ValueError('pair limit')
    if table.opaque is not None:
        return []
    body = table.rows[1:] if header else table.rows
    eligible = [i for i, c in enumerate(table.columns) if c['logical'] in ('uint64', 'int64') and not c['empty_lexemes']]
    proposals, tried = [], 0
    for a, left in enumerate(eligible):
        for right in eligible[a + 1:]:
            if tried >= max_pairs:
                return proposals
            tried += 1
            if not body:
                continue
            delta = int(body[0][right].value) - int(body[0][left].value)
            if not -(1 << 63) <= delta < 1 << 63:
                continue
            if all(int(r[right].value) - int(r[left].value) == delta for r in body):
                proposals.append({'operation': 'exact_cross_column_offset', 'source': left, 'target': right,
                                  'delta': delta, 'validated_rows': len(body),
                                  'status': 'unmeasured_candidate_not_accepted',
                                  'retain_lexemes_and_validity': True})
    return proposals
