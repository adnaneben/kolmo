#!/usr/bin/env python3
"""Shared claim registry for the 40-reviewer campaign. First claim wins; everyone else must not duplicate it.
  claim.py list                       -> print every claim so far (always read before claiming)
  claim.py add --reviewer R07 --slug short-id --tier execute|planned --title T --scope S [--files F] --seen N
      N must equal the number of claims you saw in your latest `list`; if others claimed meanwhile, the add is
      refused (exit 2) and the new claims are printed: re-read them, drop or narrow anything now covered, retry.
  claim.py status --reviewer R07 --slug short-id --state S --note N   -> append a status line
"""
import argparse, fcntl, json, os, sys, time
D = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
F = os.path.join(D, "claims.jsonl"); L = os.path.join(D, ".claims.lock"); ST = os.path.join(D, "status.jsonl")
def load():
    if not os.path.exists(F): return []
    return [json.loads(x) for x in open(F) if x.strip()]
def show(cs):
    for c in cs:
        print(f"#{c['n']} [{c['reviewer']}] ({c['tier']}) {c['slug']}: {c['title']}\n     scope: {c['scope']}\n     files: {c.get('files','')}")
    print(f"TOTAL_CLAIMS={len(cs)}")
p = argparse.ArgumentParser(); s = p.add_subparsers(dest="cmd", required=True)
s.add_parser("list")
a = s.add_parser("add")
for k in ("reviewer","slug","tier","title","scope"): a.add_argument("--"+k, required=True)
a.add_argument("--files", default=""); a.add_argument("--seen", type=int, required=True)
t = s.add_parser("status")
for k in ("reviewer","slug","state","note"): t.add_argument("--"+k, required=True)
x = p.parse_args()
with open(L, "w") as lk:
    fcntl.flock(lk, fcntl.LOCK_EX)
    cs = load()
    if x.cmd == "list": show(cs)
    elif x.cmd == "add":
        if x.tier not in ("execute", "planned"): sys.exit("tier must be execute or planned")
        if x.seen != len(cs):
            print(f"REFUSED: you saw {x.seen} claims, there are now {len(cs)}. New ones:"); show(cs[x.seen:]); sys.exit(2)
        if any(c["slug"] == x.slug for c in cs): print("REFUSED: slug already claimed"); sys.exit(3)
        if x.tier == "execute" and sum(1 for c in cs if c["reviewer"] == x.reviewer and c["tier"] == "execute") >= 2:
            print("REFUSED: at most 2 execute-tier claims per reviewer; use --tier planned"); sys.exit(4)
        c = dict(n=len(cs)+1, reviewer=x.reviewer, slug=x.slug, tier=x.tier, title=x.title, scope=x.scope, files=x.files, t=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()))
        with open(F, "a") as f: f.write(json.dumps(c) + "\n")
        print(f"CLAIMED #{c['n']} {x.slug}; TOTAL_CLAIMS={len(cs)+1}")
    else:
        with open(ST, "a") as f: f.write(json.dumps(dict(reviewer=x.reviewer, slug=x.slug, state=x.state, note=x.note, t=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()))) + "\n")
        print("ok")
