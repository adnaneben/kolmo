import json,sys
for l in open(sys.argv[1]):
    r=json.loads(l); print(r["set"], "train", r["train_files"], r["train_bytes"], "test", r["test_files"], r["test_bytes"], "profile bytes", r.get("profile_bytes_hc"))
    for k in ("untrained_balanced","untrained_hc","untrained_ratio","untrained_ratio-n","untrained_x","untrained_x-n","profile_balanced","profile_hc","profile_hc_on_ratio","profile_hc_on_x","primed_hc","primed_ratio-n","primed_x-n"):
        if k in r: print("   %-22s %8d  ratio %.3f" % (k, r[k], r["test_bytes"]/r[k]))
