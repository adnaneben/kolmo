"""Rebuild the matched size/throughput figure from score.csv and manifest.json (ReportLab)."""
import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path

from reportlab.graphics import renderPDF, renderSVG
from reportlab.graphics.shapes import Circle, Drawing, Path as ShapePath, Polygon, Rect, String
from reportlab.lib.colors import HexColor

INK = "#152c3b"
MUTED = "#4c6471"
GRID = "#d8e2e5"
ARMS = (("pre", "Historical  4db0425b", "#718491", "square"),
        ("main", "Main  a1d39559", "#ab5a17", "diamond"),
        ("combined", "Experimental  ac10ce80", "#007d7e", "circle"))
PANELS = (
    ("enwik8.zip", "enwik8 ZIP archive", (350, 600), (350, 450, 550), (.98, 1.02), (.98, 1.0, 1.02)),
    ("owid-co2.csv", "OWID CO₂ table", (125, 575), (150, 350, 550), (1, 8), (2, 4, 6, 8)),
    ("prose-64k", "Prose, 64 KiB excerpt", (4.5, 6.3), (4.5, 5.3, 6.1), (3.35, 3.47), (3.36, 3.41, 3.46)),
    ("pydub.wav", "PCM audio", (0, 22), (0, 10, 20), (3.26, 3.49), (3.30, 3.36, 3.42, 3.48)),
)

# ReportLab's bundled Helvetica uses a limited glyph set. The figure uses ASCII CO2
# and standard typographic multiplication signs for readable searchable PDF text.
def color(value):
    return HexColor(value)

def draw(rows, manifest):
    (W, H) = (1190, 807)
    figure = Drawing(W, H)

    def rect(x, y, width, height, fill):
        figure.add(Rect(x, y, width, height, fillColor=color(fill), strokeColor=None))

    def line(x1, y1, x2, y2, tone=GRID, width=0.7, dash=None):
        shape = ShapePath(strokeColor=color(tone), strokeWidth=width, fillColor=None)
        shape.moveTo(x1, y1)
        shape.lineTo(x2, y2)
        if dash:
            shape.strokeDashArray = dash
        figure.add(shape)

    def text(x, y, value, size=10, tone=INK, bold=False, anchor='start'):
        figure.add(String(x, y, str(value), fontName='Helvetica-Bold' if bold else 'Helvetica', fontSize=size, fillColor=color(tone), textAnchor=anchor))

    def dot(x, y, tone, shape, radius=3.7):
        style = dict(fillColor=color(tone), strokeColor=color('#ffffff'), strokeWidth=0.55)
        if shape == 'circle':
            figure.add(Circle(x, y, radius, **style))
        elif shape == 'diamond':
            figure.add(Polygon([x, y + radius + 1, x + radius + 1, y, x, y - radius - 1, x - radius - 1, y], **style))
        else:
            figure.add(Rect(x - radius, y - radius, radius * 2, radius * 2, **style))
    # Each source/preset/thread setting has three builds and a repeat main control.
    settings = [(s, str(level), int(t)) for (s, level, t) in manifest['settings']]
    groups = defaultdict(dict)
    for r in rows:
        key = (r['source'], r['level'], int(r['threads']))
        assert r['arm'] not in groups[key]
        groups[key][r['arm']] = r
    assert len(settings) == 12 and set(groups) == set(settings)
    for (key, data) in groups.items():
        assert set(data) == {'pre', 'main', 'main-AA', 'combined'}
        assert data['pre']['stored_bytes'] == data['main']['stored_bytes'] == data['main-AA']['stored_bytes']
        assert data['pre']['frame_sha256'] == data['main']['frame_sha256'] == data['main-AA']['frame_sha256']
        for item in data.values():
            assert abs(float(item['ratio']) - int(item['input_bytes']) / int(item['stored_bytes'])) < 1e-08
    rect(0, 0, W, H, '#ffffff')
    text(32, 762, 'Compression ratio and round-trip throughput', 30, bold=True)
    text(33, 739, 'Four workloads and 12 matched settings on Apple M5, 25 November 2026', 14, MUTED)
    lx = 35
    for (arm, label, tone, shape) in ARMS:
        dot(lx, 714, tone, shape, 4.3)
        text(lx + 12, 710, label, 11.5, bold=True)
        lx += 208
    line(728, 714, 756, 714, MUTED, 1.4)
    text(763, 710, 'T1', 11)
    line(805, 714, 833, 714, MUTED, 1.4, (3, 2))
    text(840, 710, 'T4', 11)
    text(900, 710, 'Each line connects one matched setting.', 10.5, MUTED)
    line(32, 695, W - 30, 695, INK, 1)
    for (idx, (source, title, xlim, xticks, ylim, yticks)) in enumerate(PANELS):
        left = 32 + idx * 286
        x0 = left + 38
        x1 = left + 267
        y0 = 536
        y1 = 641
        text(left, 670, title.replace('CO₂', 'CO2'), 15, bold=True)
        text(left, 652, 'Ratio (original / stored bytes)', 9, MUTED)
        X = lambda v: x0 + (v - xlim[0]) * (x1 - x0) / (xlim[1] - xlim[0])
        Y = lambda v: y0 + (v - ylim[0]) * (y1 - y0) / (ylim[1] - ylim[0])
        for t in yticks:
            line(x0, Y(t), x1, Y(t))
            text(x0 - 5, Y(t) - 3, f'{t:.2f}'.rstrip('0').rstrip('.'), 9, MUTED, anchor='end')
        for t in xticks:
            line(X(t), y0, X(t), y1, '#edf1f2', 0.65)
            text(X(t), y0 - 14, f'{t:g}', 9, MUTED, anchor='middle')
        line(x0, y0, x1, y0, MUTED, 0.8)
        line(x0, y0, x0, y1, MUTED, 0.8)
        points = []
        for key in settings:
            if key[0] != source:
                continue
            coords = []
            for (arm, _, tone, shape) in ARMS:
                row = groups[key][arm]
                (xx, yy) = (float(row['combined_MBps']), float(row['ratio']))
                if not (xlim[0] <= xx <= xlim[1] and ylim[0] <= yy <= ylim[1]):
                    raise ValueError(f'Point outside panel limits for {key}, {arm}')
                coords.append((X(xx), Y(yy)))
                points.append((*coords[-1], tone, shape))
            for (a, b) in zip(coords, coords[1:]):
                line(*a, *b, '#788c96', 1.15, (3, 2) if key[2] == 4 else None)
        for (x, y, tone, shape) in points:
            dot(x, y, tone, shape)
        text((x0 + x1) / 2, 502, 'Combined throughput (MB/s)', 10, MUTED, anchor='middle')
    text(33, 479, 'Higher and farther right is better. Axes vary by workload. The table gives exact values for overlapping marks.', 10.5, MUTED)
    line(32, 465, W - 30, 465, INK, 1)
    # Historical and main rows have equal byte counts and identical frame hashes.
    text(32, 441, 'Matched results', 18, bold=True)
    text(32, 419, 'Input', 11, bold=True)
    text(223, 419, 'Preset', 11, bold=True)
    text(291, 419, 'Threads', 11, bold=True)
    text(520, 435, 'Complete stored size (bytes)', 12, bold=True, anchor='middle')
    text(481, 415, 'Historical = main', 10, MUTED, anchor='end')
    text(627, 415, 'Experimental', 10, ARMS[2][2], anchor='end')
    text(910, 435, 'Combined encode + decode throughput (MB/s)', 12, bold=True, anchor='middle')
    for (x, (_, label, tone, _)) in zip((802, 973, 1148), ARMS):
        text(x, 415, label.split('  ')[0], 10, tone, bold=True, anchor='end')
    y = 393
    previous = None
    for (source, level, t) in settings:
        is_new = source != previous
        if previous is not None and is_new:
            y -= 5
        if is_new:
            line(32, y + 13, W - 30, y + 13, GRID, 0.8)
        data = groups[source, level, t]
        if is_new:
            text(32, y, source, 11, bold=True)
        text(223, y, 'Ultra' if level == 'ultra' else 'L' + level, 10.8)
        text(307, y, t, 10.8, anchor='middle')
        for (x, arm) in ((481, 'main'), (627, 'combined')):
            text(x, y, f"{int(data[arm]['stored_bytes']):,}", 10.8, INK if arm == 'main' else ARMS[2][2], anchor='end')
        for (x, (arm, _, tone, _)) in zip((802, 973, 1148), ARMS):
            text(x, y, f"{float(data[arm]['combined_MBps']):,.2f}", 10.8, tone, bold=arm == 'combined', anchor='end')
        previous = source
        y -= 22
    line(32, 114, W - 30, 114, INK, 1)
    text(32, 92, 'One warmup and seven scored rounds per arm, shuffled within setting. Each timed run verified exact recovery.', 11.3, INK)
    text(32, 73, 'Combined MB/s = original bytes / median paired (encode seconds + decode seconds) / 1,000,000. Archive time includes creation and full restore.', 10.7, MUTED)
    text(32, 54, 'Experimental options were enabled for eligible inputs. Measurements use a shared host. T1/T4 are requested threads. The second main control is retained in score.csv.', 10.7, MUTED)
    return figure
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', nargs='?', type=Path, default=Path(__file__).resolve().parent)
    args = parser.parse_args()
    root = args.directory
    manifest = json.loads((root / 'manifest.json').read_text())
    if manifest['rounds'] != 7 or manifest['binaries']['combined'] != 'f1d02b873c62d2130f0bc6a311617742af0c8df4d26a3106c2cbdcd28b716213':
        raise ValueError('Verify figure labels and trial count against the changed executable or manifest.')
    with (root / 'score.csv').open() as f:
        figure = draw(list(csv.DictReader(f)), manifest)
    renderPDF.drawToFile(figure, str(root / 'all-options-performance.pdf'))
    renderSVG.drawToFile(figure, str(root / 'all-options-performance.svg'))
