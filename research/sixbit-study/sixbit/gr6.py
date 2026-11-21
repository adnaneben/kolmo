"""GR6: repeat model + graph map, then 6-bit output (research prototype, lossless, decoder included).

1. Tokenise: runs of [A-Za-z0-9.-] are one token, every other byte (space, comma, newline, quote...) is its own token.
2. For each token decide, in this order:
     DITTO   same token as in the line above at the same position          -> value 0
     GRAPH   found in the graph: the move-to-front list of tokens that      -> value 2 + 2*rank
             followed the same context before (context = position in line and previous token)
     KNOWN   seen before anywhere (global id by first appearance)           -> value 3 + 2*id
     NEW     never seen: value 1, then its characters in 6-bit codes, END
3. Values are written in 6-bit units (5 data bits + continuation bit), so 0..31 cost one unit.
Characters of NEW tokens use a per-file table: the 62 most frequent bytes, END = 62, ESC = 63 (+ 2 units).

usage: python3 gr6.py file units_out     -> prints statistics; units_out holds one 6-bit unit per byte
"""
import collections, re, sys

TOK = re.compile(rb'[A-Za-z0-9.\-]+|[^A-Za-z0-9.\-]')
CAP = 1024   # longest graph list kept per context


def put_v(out, v):
    while v >= 32:
        out.append(32 | (v & 31)); v >>= 5
    out.append(v)


def get_v(units, i):
    v = 0; s = 0
    while True:
        u = units[i]; i += 1
        v |= (u & 31) << s; s += 5
        if not u & 32:
            return v, i


class State:
    def __init__(self):
        self.above = []; self.line = []; self.graph = {}; self.gid = {}; self.tokens = []

    def ctx(self):
        j = len(self.line)
        return (j, self.line[-1] if self.line else b'')

    def update(self, t):
        key = self.ctx()
        lst = self.graph.setdefault(key, [])
        if t in lst:
            lst.remove(t)
        lst.insert(0, t)
        del lst[CAP:]
        if t not in self.gid:
            self.gid[t] = len(self.tokens); self.tokens.append(t)
        self.line.append(t)
        if t == b'\n':
            self.above = self.line; self.line = []


def encode(data):
    freq = collections.Counter(data)
    table = [b for b, _ in freq.most_common(62)]
    code = {b: i for i, b in enumerate(table)}
    st = State(); out = bytearray(); kinds = collections.Counter()
    for m in TOK.finditer(data):
        t = m.group()
        j = len(st.line)
        lst = st.graph.get(st.ctx(), ())
        if j < len(st.above) and st.above[j] == t:
            put_v(out, 0); kinds['ditto'] += 1
        elif t in lst:
            put_v(out, 2 + 2 * lst.index(t)); kinds['graph'] += 1
        elif t in st.gid:
            put_v(out, 3 + 2 * st.gid[t]); kinds['known'] += 1
        else:
            put_v(out, 1); kinds['new'] += 1
            for c in t:
                if c in code:
                    out.append(code[c])
                else:
                    out += bytes([63, c >> 6, c & 63])
            out.append(62)
        st.update(t)
    return bytes(table), bytes(out), kinds


def decode(table, units):
    st = State(); res = bytearray(); i = 0
    while i < len(units):
        v, i = get_v(units, i)
        j = len(st.line)
        if v == 0:
            t = st.above[j]
        elif v == 1:
            t = bytearray()
            while units[i] != 62:
                if units[i] == 63:
                    t.append(units[i + 1] << 6 | units[i + 2]); i += 3
                else:
                    t.append(table[units[i]]); i += 1
            i += 1; t = bytes(t)
        elif v % 2 == 0:
            t = st.graph[st.ctx()][(v - 2) // 2]
        else:
            t = st.tokens[(v - 3) // 2]
        res += t
        st.update(t)
    return bytes(res)


if __name__ == '__main__':
    data = open(sys.argv[1], 'rb').read()
    table, units, kinds = encode(data)
    assert decode(table, units) == data, 'round trip failed'
    open(sys.argv[2], 'wb').write(units)
    n = len(data); size = (len(units) * 6 + 7) // 8 + 1 + len(table)
    tot = sum(kinds.values())
    print(f'{sys.argv[1].split("/")[-1]}: {n:,} B, {tot:,} tokens ('
          + ', '.join(f'{k} {v / tot:.0%}' for k, v in kinds.most_common()) + f'), {len(units):,} units '
          f'-> {size:,} B packed 6-bit = {100 * size / n:.1f}% of original (ratio {n / size:.2f}), round trip OK')
