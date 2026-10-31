"""Round-trip test for the kolmo Python binding. Run after `make lib`:  python3 bindings/python/test_kolmo.py"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kolmo

def main():
    samples = [b"", b"hello", os.urandom(1000),
               (b"country,year,co2\n" + b"".join(b"Utopia,%d,%d.5\n" % (2000 + i % 20, i) for i in range(40000)))]
    fails = 0
    for data in samples:
        for mode in ("rt", "fast", "eco", "hc", "max", "ultra"):
            blob = kolmo.compress(data, mode=mode)
            back = kolmo.decompress(blob)
            ok = back == data
            if not ok: fails += 1; print(f"FAIL {mode} len {len(data)}: round trip")
            if data and mode == "fast":
                r = kolmo.compress(data, mode="fast")
                if kolmo.original_size(r) != len(data): fails += 1; print("FAIL original_size")
                mid = len(data) // 2
                if kolmo.read_range(r, mid, min(64, len(data) - mid)) != data[mid:mid + min(64, len(data) - mid)]:
                    fails += 1; print("FAIL read_range")
        ratio = len(data) / max(1, len(kolmo.compress(data, mode="hc")))
        print(f"len {len(data):>7}: round trips ok in all modes" + (f", hc ratio {ratio:.2f}" if len(data) > 1000 else ""))
    print("ALL PASSED" if not fails else f"{fails} FAILURES")
    sys.exit(1 if fails else 0)

if __name__ == "__main__":
    main()
