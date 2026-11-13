#!/usr/bin/env python3
"""Generate a RESEARCH decoder tree: a copy of the pinned src/ with ONE change in kgraph.inc: the KG_INT inverse call in
kg_decode uses P2-E2's identity-checked fi_inverse (fastint.inc, included just before kg_decode) instead of
kg_int_inv. Same wire semantics, same validation intent; measures what a production decoder speed-up would buy.
usage: gen_fastdec.py REPO_ROOT OUT_DIR    (OUT_DIR/src is then built with the same Makefile rules by the caller)"""
import hashlib, pathlib, shutil, sys
CORE = 'e06467c8872d494b97270811bedf6fd0bac2aa31'
root, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
c = (root / 'src/kolmo.c').read_bytes()
if hashlib.sha1(b'blob ' + str(len(c)).encode() + b'\0' + c).hexdigest() != CORE: raise SystemExit('pinned core mismatch')
shutil.copytree(root / 'src', out / 'src')
k = (out / 'src/kgraph.inc').read_text()
call = 'kbuf r = {0}; if (kg_int_inv(t->par, t->plen, &o[0], &o[1], maxs, &r)) { free(r.p); goto out; }'
if k.count(call) != 1: raise SystemExit('call anchor')
k = k.replace(call, call.replace('kg_int_inv(', 'fi_inverse('))
i = k.index('static int kg_decode(') if 'static int kg_decode(' in k else None
if i is None: raise SystemExit('kg_decode anchor')
fi = (root / 'research/codec/P2-E2/fastint.inc').read_text()
k = k[:i] + '/* RESEARCH (gen_fastdec.py): P2-E2 fastint.inc */\n' + fi + '\n' + k[i:]
(out / 'src/kgraph.inc').write_text(k)
print('ok')
