"""docs/ACCEPTANCE.md as code: tiers, exchange rates, always-keep and never-allowed checks.

Ratios (B against A), per file or per cell:
  bytes = B/A (0.99 = 1% smaller)
  enc, dec = A cost / B cost (0.90 = 10% slower); already snapped to 1.0 when inside the A/A noise band
  comb = (A enc + A dec) / (B enc + B dec); lat = 1/comb
  rss = B/A peak RSS, the larger phase
"""

GAIN_MIN = 0.005          # a change must gain 0.5% on some type
LOSS_MAX = 0.005          # a larger cell passes only under 0.5% larger, within its tier's reverse rate
RSS_MAX = 1.10


def tier(level):
    if level == "ultra":
        return "ultra"
    n = int(level)
    return "fast" if n <= 3 else "middle" if n <= 7 else "high"


def _reverse_ok(t, bytes_, enc, comb):
    """a size loss bought with speed, within the tier's rate in the other direction"""
    loss = bytes_ - 1
    if loss <= 0:
        return True
    if loss >= LOSS_MAX:
        return False
    if t == "fast":
        return comb >= 1 + 10 * loss           # 1% larger per 10% faster combined (mirror of 1% smaller at 0.90)
    if t == "middle":
        return enc > 1 and (1 - 1 / enc) >= 10 * loss
    if t == "high":
        return enc >= 1 + 10 * loss
    return enc >= 1 + 10 * loss


def tier_check(level, m):
    """m: dict bytes, enc, dec, comb, rss (rss may be None). -> (ok, reasons[])"""
    t = tier(level)
    b, enc, dec, comb = m["bytes"], m["enc"], m["dec"], m["comb"]
    rss = m.get("rss") or 1.0
    lat = 1 / comb if comb else float("inf")
    why = []
    if b > 1 and not _reverse_ok(t, b, enc, comb):
        why.append(f"larger by {100 * (b - 1):.3f}% beyond the {t} rate")
    if t == "fast":
        if not ((comb >= 0.95 and b <= 1.0) or (b <= 0.99 and comb >= 0.90) or (b > 1 and not why)):
            why.append(f"comb {comb:.3f} for bytes {b:.4f} (needs >=0.95 at equal size, or >=0.90 at <=0.99)")
    elif t == "middle":
        if dec < 0.95:
            why.append(f"decode {dec:.3f} < 0.95")
        if enc < 0.80:
            why.append(f"encode {enc:.3f} < 0.80")
        if enc < 1 and b <= 1:
            need = (1 / enc - 1) / 10
            if 1 - b < need:
                why.append(f"{100 * (1 - b):.3f}% smaller for {100 * (1 / enc - 1):.1f}% more encode time "
                           f"(needs {100 * need:.3f}%)")
    elif t == "high":
        if b <= 0.98:
            if enc < 0.67: why.append(f"encode {enc:.3f} < 0.67")
        elif b < 1:
            if enc < 0.90: why.append(f"gain under 2% needs encode >= 0.90, got {enc:.3f}")
        if dec < 0.95: why.append(f"decode {dec:.3f} < 0.95")
        if lat > 1.5: why.append(f"combined latency {lat:.2f}x > 1.5")
    else:
        if b > 0.99 and b <= 1 and (enc < 1 or dec < 1):
            why.append(f"bytes {b:.4f} > 0.99 while slower")
        if enc < 0.5: why.append(f"encode {enc:.3f} < 0.5")
        if dec < 0.8: why.append(f"decode {dec:.3f} < 0.8")
    if rss > RSS_MAX:
        why.append(f"RSS {rss:.2f}x > {RSS_MAX}")
    return (not why), why


def classify(level, m, identical):
    """-> one of KEEP-ID (byte-identical speedup), KEEP-FREE (free size gain), NEUTRAL, PASS, FAIL, TRADE.
    identical: every frame of the cell is byte-identical to A's."""
    b, enc, dec = m["bytes"], m["enc"], m["dec"]
    rss = m.get("rss") or 1.0
    slower = enc < 1 or dec < 1
    faster = enc > 1 or dec > 1
    if identical:
        if faster and not slower and rss <= RSS_MAX:
            return "KEEP-ID", []
        if not faster and not slower:
            return "NEUTRAL", []
    if b < 1 and not slower and rss <= RSS_MAX:
        return "KEEP-FREE", []
    ok, why = tier_check(level, m)
    if ok:
        return "PASS", []
    # a trade: smaller for speed beyond the rate, or faster for size beyond the rate
    if (b < 1 and slower) or (b > 1 and faster):
        return "TRADE", why
    return "FAIL", why


WIN = {"KEEP-ID", "KEEP-FREE", "PASS"}
LOSE = {"FAIL", "TRADE"}


def decode_violation(level, dec):
    """never allowed: slower decode at levels 1-7 beyond noise (dec already snapped to 1.0 inside noise)"""
    return level != "ultra" and int(level) <= 7 and dec < 1.0
