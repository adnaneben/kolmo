"""Count paths without overriding dispatch in an isolated fixed-source build."""
import csv
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
sys.path.insert(0, str(here.parent))
from measure import specs

fields = ["arm_crc_bytes", "c_crc_bytes", "neon_decimal_values", "c_decimal_calls", "neon_word_scan_iterations", "neon_word_output_bytes"]

def sha(data):
    return hashlib.sha256(data).hexdigest()

def instrument(tree):
    def insert(filename, before, after):
        p = tree / "src" / filename
        contents = p.read_text()
        if contents.count(before) != 1:
            raise ValueError((filename, before))
        p.write_text(contents.replace(before, after))
    c = "static unsigned long long " + ", ".join(fields) + ";\n"
    c += '__attribute__((destructor)) static void print_probe(void) {fprintf(stderr, "NOASM_TRACE ' + " ".join(["%llu"] * len(fields)) + '\\n", ' + ", ".join(fields) + ');}'
    insert("kolmo.c", '#include "arch/cpu.h"', '#include "arch/cpu.h"\n' + c)
    insert("arch/crc32c.c", "return crc32c_arm(crc, buf, len);", "arm_crc_bytes += len; return crc32c_arm(crc, buf, len);")
    insert("arch/crc32c.c", "    pthread_once(&crc_once, crc_init);", "    c_crc_bytes += len;\n    pthread_once(&crc_once, crc_init);")
    insert("arch/aarch64/decimal_neon.c", "            uint32x2_t a32 =", "            neon_decimal_values += 2;\n            uint32x2_t a32 =")
    insert("algo/decimal.c", "static inline uint8_t *fmt_dec_swar(uint8_t *o, int64_t m, int s) {", "static inline uint8_t *fmt_dec_swar(uint8_t *o, int64_t m, int s) {\n    c_decimal_calls++;")
    insert("transforms/word.c", "        uint8x16_t c = vcleq_u8", "        neon_word_scan_iterations++;\n        uint8x16_t c = vcleq_u8")
    insert("transforms/word.c", "    while (p < end) {\n        const uint8_t *s = p;", "    neon_word_output_bytes += (size_t)(o - out);\n    while (p < end) {\n        const uint8_t *s = p;")

def measure(repo, binary, out):
    fixtures = repo / "tests" / "v12"
    rows = []
    env = {k: v for k, v in os.environ.items() if not k.startswith(("KOLMO_", "CZN_"))}
    with tempfile.TemporaryDirectory() as tmp:
        dest = Path(tmp) / "restore"
        for frame, original in specs:
            f = fixtures / frame
            want = (fixtures / original).read_bytes()
            p = subprocess.run([binary, "-dq", "-T1", "-o", dest, "--", f], env=env, capture_output=True, text=True, check=True)
            actual = dest.read_bytes()
            dest.unlink()
            prefix, *values = p.stderr.strip().split()
            if actual != want or prefix != "NOASM_TRACE" or len(values) != len(fields):
                raise ValueError((frame, p.stderr))
            rows.append(dict(frame=frame, frame_sha256=sha(f.read_bytes()), output_sha256=sha(actual), **dict(zip(fields, map(int, values)))))
    with out.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=rows[0])
        writer.writeheader()
        writer.writerows(rows)
    totals = [sum(row[key] for row in rows) for key in fields]
    if totals != [0, 3080, 0, 72140, 0, 0]:
        raise ValueError(totals)
    print(sha(binary.read_bytes()), len(rows), dict(zip(fields, totals)))

if __name__ == "__main__":
    if sys.argv[1] == "instrument":
        instrument(Path(sys.argv[2]))
    elif sys.argv[1] == "measure":
        measure(*map(Path, sys.argv[2:]))
