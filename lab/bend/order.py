"""Reorder a Bend file so every callee precedes its caller (Bend requires defined-before-use; cycles are errors)."""
import re, sys
p = sys.argv[1]; s = open(p).read()
lines = s.splitlines()
blocks, cur, key = [], [], None
for ln in lines:
    m = re.match(r'^(def|type|law) ([\w.]+)', ln)
    if m:
        if cur: blocks.append((key, cur))
        cur, key = [ln], m.group(2)
    else:
        if cur and ln.startswith('#') and cur[-1].strip() == "":
            blocks.append((key, cur)); cur, key = [ln], None
        else:
            cur.append(ln)
if cur: blocks.append((key, cur))
head, defs, order0 = [], {}, []
for k, v in blocks:
    if k is None and not defs: head += v
    elif k: defs[k] = "\n".join(v).rstrip(); order0.append(k)
    else: defs[order0[-1]] += "\n" + "\n".join(v).rstrip()
names = sorted(defs, key=len, reverse=True)
def code_of(body):
    return "\n".join(re.sub(r'#.*$', '', ln) for ln in body.split("\n")[1:])
deps = {k: {n for n in names if n != k and re.search(r'(?<![\w.])' + re.escape(n) + r'(?![\w.])', code_of(b))} for k, b in defs.items()}
placed, out = set(), []
def place(k, stack=()):
    if k in placed: return
    if k in stack: raise SystemExit(f"cycle: {' -> '.join(stack + (k,))}")
    for d in sorted(deps[k]): place(d, stack + (k,))
    placed.add(k); out.append(k)
for k in order0: place(k)
open(p, 'w').write("\n".join(head).rstrip() + "\n\n" + "\n\n".join(defs[k] for k in out) + "\n")
print(f"ordered {len(out)} defs, callees first")
