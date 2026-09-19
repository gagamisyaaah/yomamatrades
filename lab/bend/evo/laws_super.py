"""Turn the three super indicators into Bend: each composite is a Bool function of its blocks (SUPER.bend), with laws that
the signal implies every block, that it holds when all blocks hold, and that it is monotone in each block
(LAWS_SUPER.bend); proofs are generated as one-match-per-def case trees (PROOF_SUPER.bend). `bend PROOF_SUPER.bend` is
the gate the terminal checks before it trusts a rule.
usage: python3.12 bend/evo/laws_super.py   (reads bend/data/supers.json)"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
OUT = ROOT / "bend"
COND = re.compile(r"^(?P<b>[a-z0-9_]+)(?P<op>≤|≥|=)(?:p\d+\()?(?P<thr>-?[0-9.e+-]+)\)?$")


def ident(fam: str) -> str:
    return fam.lower()


def main():
    supers = json.load(open(ROOT / "bend/data/supers.json"))
    code, laws, proofs, helpers = ["import Base\n"], ["import Base\nimport ./SUPER.bend as S\n"], ["import Base\nimport ./SUPER.bend as S\nimport ./LAWS_SUPER.bend as Laws\n"], []
    for fam, s in supers.items():
        blocks = [COND.match(c.strip())["b"] for c in s["rule"].split(" & ")]
        f = ident(fam); k = len(blocks); params = ", ".join(f"{b}: Bool" for b in blocks)
        # the composite as a match chain (each block a Bool: "the block's condition holds on this bar")
        body = ""
        for i, b in enumerate(blocks):
            pad = "  " * (i + 1)
            body += f"{pad}match {b}:\n{pad}  case False{{}}:\n{pad}    False{{}}\n{pad}  case True{{}}:\n"
        body += "  " * (k + 1) + "  True{}\n"
        code.append(f"# {fam}: {s['rule']}\ndef {f}({params}) -> Bool:\n{body}")
        # laws
        allT = ", ".join("True{}" for _ in blocks)
        laws.append(f"law {f}_when_all_hold:\n  {{S.{f}({allT}) == True{{}} : Bool}}\n")
        proofs.append(f"def Laws.{f}_when_all_hold():\n  {{==}}\n")
        for j, b in enumerate(blocks):
            fors = "".join(f"  for +{x}: Bool\n" for x in blocks)
            call = f"S.{f}({', '.join(blocks)})"
            laws.append(f"law {f}_needs_{b}:\n{fors}  {{Bool.or({b}, Bool.not({call})) == True{{}} : Bool}}\n")
            # proof: split b; True → {==}; False → chain over the preceding blocks (each False closes; all-True closes)
            if j == 0:
                proofs.append(f"def Laws.{f}_needs_{b}({', '.join(blocks)}):\n  match {b}:\n    case True{{}}:\n      {{==}}\n    case False{{}}:\n      {{==}}\n")
            else:
                names = []; chain = []
                for i in range(j):
                    asg = {b: "False{}", **{blocks[t]: "True{}" for t in range(i)}}
                    free = [x for x in blocks if x not in asg]
                    goal = f"{{Bool.or(False{{}}, Bool.not(S.{f}({', '.join(asg.get(x, x) for x in blocks)}))) == True{{}} : Bool}}"
                    names.append(f"pf.{f}.{b}.{i}")
                    nxt_free = [x for x in blocks if x not in {**asg, blocks[i]: 'True{}'}]
                    on_true = "{==}" if i == j - 1 else f"pf.{f}.{b}.{i + 1}({', '.join(nxt_free)})"
                    chain.append(f"def pf.{f}.{b}.{i}({', '.join('+' + x + ': Bool' for x in free)}) -> {goal}:\n  match {blocks[i]}:\n    case False{{}}:\n      {{==}}\n    case True{{}}:\n      {on_true}\n")
                helpers.extend(reversed(chain))   # callee first: the deepest helper is defined before the one that calls it
                free0 = [x for x in blocks if x != b]
                proofs.append(f"def Laws.{f}_needs_{b}({', '.join(blocks)}):\n  match {b}:\n    case True{{}}:\n      {{==}}\n    case False{{}}:\n      pf.{f}.{b}.0({', '.join(free0)})\n")
    (OUT / "SUPER.bend").write_text("\n".join(code))
    (OUT / "LAWS_SUPER.bend").write_text("\n".join(laws))
    (OUT / "PROOF_SUPER.bend").write_text("\n".join(proofs[:1]) + "\n" + "\n".join(helpers) + "\n" + "\n".join(proofs[1:]))
    print(f"{len(laws) - 1} laws, {len(helpers)} helper defs → bend/SUPER.bend, LAWS_SUPER.bend, PROOF_SUPER.bend")


if __name__ == "__main__":
    main()
