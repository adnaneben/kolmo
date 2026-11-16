#!/usr/bin/env python3
"""Deterministic JSON-lines and web-log samples for the bwt lane's checks (seeded, no downloads)."""
import json, random, sys, os
out = sys.argv[1]; os.makedirs(out, exist_ok=True)
r = random.Random(20261116)
paths = ["/", "/index.html", "/api/v1/items", "/api/v1/users/%d", "/static/app.%s.js", "/img/%d.png", "/search?q=%s"]
agents = ["Mozilla/5.0 (X11; Linux x86_64)", "Mozilla/5.0 (Macintosh; Intel Mac OS X 14_5)", "curl/8.5.0", "python-requests/2.31"]
words = "alpha beta gamma delta kolmo river stone cloud north south data block sort run".split()
with open(os.path.join(out, "web.log"), "w") as f:
    t = 1_700_000_000
    for i in range(400_000):
        t += r.choice((0, 0, 1, 1, 2, 5))
        p = r.choice(paths)
        if "%d" in p: p = p % r.randint(1, 50000)
        elif "%s" in p: p = p % (r.choice(words) + str(r.randint(0, 99)))
        f.write('%d.%d.%d.%d - - [%d] "GET %s HTTP/1.1" %d %d "-" "%s"\n' % (
            r.randint(1, 223), r.randint(0, 255), r.randint(0, 255), r.randint(1, 254), t, p,
            r.choice((200, 200, 200, 304, 404, 500)), r.randint(0, 90000), r.choice(agents)))
with open(os.path.join(out, "events.jsonl"), "w") as f:
    for i in range(250_000):
        f.write(json.dumps({"id": i, "ts": 1_700_000_000 + i * 3 + r.randint(0, 2), "user": "u%05d" % r.randint(0, 20000),
                            "event": r.choice(("view", "click", "buy", "scroll")), "price": round(r.uniform(0, 500), 2),
                            "tags": r.sample(words, r.randint(0, 3)), "ok": r.random() < 0.9}) + "\n")
