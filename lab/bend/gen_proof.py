"""Writes LAWS.bend (the claims about the engine's pure decisions) and PROOF.bend (their proofs: finite case analyses).
usage: python3.12 bend/gen_proof.py && (cd bend && bend PROOF.bend)"""
from pathlib import Path
HERE = Path(__file__).resolve().parent
V = ["up", "wh", "te", "hot", "mt", "gd"]
P = ["a", "b", "c", "d", "e", "f", "g"]
laws = []; proofs = []


def law(name, fors, claim):
    laws.append(f"law {name}:\n" + "".join(f"  for {f}\n" for f in fors) + f"  {claim}\n")


def proof(name, params, body):
    proofs.append(f"def Laws.{name}({', '.join(params)}):\n" + body + "\n")


def split(vars_, leaf, ind=1):
    """nested matches over Bool vars; leaf(assignment) → text of the closing term."""
    def go(i, asg):
        pad = "  " * ind + "  " * (2 * i)
        if i == len(vars_):
            return pad + leaf(asg)
        out = pad + f"match {vars_[i]}:\n"
        for val in ("False", "True"):
            out += pad + f"  case {val}{{}}:\n" + go(i + 1, {**asg, vars_[i]: val})
            if val == "False":
                out += "\n"
        return out
    return go(0, {})


# ── hole state machine ──
law("hole_pivot_rearms", ["+up: Bool", "+dn: Bool", "s: D.Hole"], "{D.hole.next(True{}, up, dn, s) == D.hole.arm(up, dn) : D.Hole}")
proof("hole_pivot_rearms", ["up", "dn", "s"], "  {==}")
law("hole_up_sticks", ["up: Bool", "dn: Bool"], "{D.hole.next(False{}, up, dn, D.BrokeUp{}) == D.BrokeUp{} : D.Hole}")
proof("hole_up_sticks", ["up", "dn"], "  {==}")
law("hole_down_sticks", ["up: Bool", "dn: Bool"], "{D.hole.next(False{}, up, dn, D.BrokeDown{}) == D.BrokeDown{} : D.Hole}")
proof("hole_down_sticks", ["up", "dn"], "  {==}")
law("hole_armed_checks_break", ["+up: Bool", "+dn: Bool"], "{D.hole.next(False{}, up, dn, D.InForce{}) == D.hole.arm(up, dn) : D.Hole}")
proof("hole_armed_checks_break", ["up", "dn"], "  {==}")
law("hole_breaks_up_first", ["dn: Bool"], "{D.hole.arm(True{}, dn) == D.BrokeUp{} : D.Hole}")
proof("hole_breaks_up_first", ["dn"], "  {==}")
law("hole_breaks_down", [], "{D.hole.arm(False{}, True{}) == D.BrokeDown{} : D.Hole}")
proof("hole_breaks_down", [], "  {==}")
law("hole_holds", [], "{D.hole.arm(False{}, False{}) == D.InForce{} : D.Hole}")
proof("hole_holds", [], "  {==}")
law("hole_never_both", ["+s: D.Hole"], "{Bool.and(D.hole.is_up(s), D.hole.is_dn(s)) == False{} : Bool}")
proof("hole_never_both", ["s"], "  match s:\n    case D.InForce{}:\n      {==}\n    case D.BrokeUp{}:\n      {==}\n    case D.BrokeDown{}:\n      {==}")
# ── the C reading implies each condition ──
# Bend allows one match per def in a proof, so every deeper case is its own def whose return type is the refined goal.
helpers = []


def lit(asg, v):
    return f"{asg[v]}{{}}" if v in asg else v


def sig_goal(asg, k):
    v = V[k]
    call = f"D.sig_c.b({', '.join(lit(asg, x) for x in V)})"
    return f"{{Bool.or({'Bool.not(' + lit(asg, v) + ')' if v == 'hot' else lit(asg, v)}, Bool.not({call})) == True{{}} : Bool}}"


def sig_chain(name, k, j, asg):
    """helper that splits V[j] (j < k) under assignment asg; returns its name."""
    free = [x for x in V if x not in asg]
    hname = f"pf.{name}.{j}"
    cont, close = ("False", "True") if V[j] == "hot" else ("True", "False")   # the reading survives hot = False, every other condition = True
    nxt = {**asg, V[j]: cont}
    on_cont = "{==}" if j == k - 1 else sig_chain(name, k, j + 1, nxt) + "(" + ", ".join(x for x in V if x not in nxt) + ")"
    helpers.append(f"def {hname}({', '.join('+' + x + ': Bool' for x in free)}) -> {sig_goal(asg, k)}:\n  match {V[j]}:\n    case {close}{{}}:\n      {{==}}\n    case {cont}{{}}:\n      {on_cont}\n")
    return hname


for k, v in enumerate(V):
    name = "sig_vetoed_when_hot" if v == "hot" else f"sig_needs_{v}"
    law(name, [f"+{x}: Bool" for x in V], sig_goal({}, k))
    good, bad = ("False", "True") if v == "hot" else ("True", "False")
    if k == 0:
        on_bad = "{==}"
    else:
        h = sig_chain(name, k, 0, {v: bad}); on_bad = h + "(" + ", ".join(x for x in V if x != v) + ")"
    proof(name, V, f"  match {v}:\n    case {good}{{}}:\n      {{==}}\n    case {bad}{{}}:\n      {on_bad}")
law("sig_when_all_hold", [], "{D.sig_c.b(True{}, True{}, True{}, False{}, True{}, True{}) == True{} : Bool}")
proof("sig_when_all_hold", [], "  {==}")
# ── panels: full case trees, one def per node ──


def panels_tree(name, goal_fn, asg):
    free = [x for x in P if x not in asg]
    if len(free) == 1:
        return f"match {free[0]}:\n    case False{{}}:\n      {{==}}\n    case True{{}}:\n      {{==}}", None
    v = free[0]
    calls = {}
    for val in ("False", "True"):
        sub = {**asg, v: val}
        hname = f"pf.{name}.{''.join('T' if sub.get(x) == 'True' else ('F' if x in sub else '') for x in P)}"
        body, _ = panels_tree(name, goal_fn, sub)
        rest = [x for x in P if x not in sub]
        helpers.append(f"def {hname}({', '.join('+' + x + ': Bool' for x in rest)}) -> {goal_fn(sub)}:\n  {body}\n")
        calls[val] = f"{hname}({', '.join(rest)})"
    return f"match {v}:\n    case False{{}}:\n      {calls['False']}\n    case True{{}}:\n      {calls['True']}", None


def pb_goal_le7(asg):
    return f"{{U32.is_le(D.panels.b({', '.join(lit(asg, x) for x in P)}), 7) == True{{}} : Bool}}"


def pb_goal_pack(asg):
    call = f"D.panels.b({', '.join(lit(asg, x) for x in P)})"
    return f"{{(U32.shrn(U32.shln({call}, 24n), 24n) .&. 15 : U32) == {call} : U32}}"


law("panels_at_most_seven", [f"+{x}: Bool" for x in P], pb_goal_le7({}))
body, _ = panels_tree("panels_at_most_seven", pb_goal_le7, {}); proof("panels_at_most_seven", P, "  " + body)
law("panels_all_seven", [], "{D.panels.b(True{}, True{}, True{}, True{}, True{}, True{}, True{}) == 7 : U32}")
proof("panels_all_seven", [], "  {==}")
law("panels_none", [], "{D.panels.b(False{}, False{}, False{}, False{}, False{}, False{}, False{}) == 0 : U32}")
proof("panels_none", [], "  {==}")
law("panels_survive_packing", [f"+{x}: Bool" for x in P], pb_goal_pack({}))
body, _ = panels_tree("panels_survive_packing", pb_goal_pack, {}); proof("panels_survive_packing", P, "  " + body)
# ── bracket legs ──
law("bracket_decided_stays", ["hs: Bool", "ht: Bool", "+cur: F32", "s: F32", "t: F32"], "{D.bk.pick(True{}, hs, ht, cur, s, t) == cur : F32}")
proof("bracket_decided_stays", ["hs", "ht", "cur", "s", "t"], "  {==}")
law("bracket_stop_before_target", ["ht: Bool", "cur: F32", "+s: F32", "t: F32"], "{D.bk.pick(False{}, True{}, ht, cur, s, t) == s : F32}")
proof("bracket_stop_before_target", ["ht", "cur", "s", "t"], "  {==}")
law("bracket_target", ["cur: F32", "s: F32", "+t: F32"], "{D.bk.pick(False{}, False{}, True{}, cur, s, t) == t : F32}")
proof("bracket_target", ["cur", "s", "t"], "  {==}")
law("bracket_untouched_keeps_running_value", ["+cur: F32", "s: F32", "t: F32"], "{D.bk.pick(False{}, False{}, False{}, cur, s, t) == cur : F32}")
proof("bracket_untouched_keeps_running_value", ["cur", "s", "t"], "  {==}")

head = "# The claims. Human-owned: states what the engine's decisions must satisfy; PROOF.bend must prove every one.\nimport Base\nimport ./dragon.bend as D\n\n"
(HERE / "LAWS.bend").write_text(head + "\n".join(laws))
(HERE / "PROOF.bend").write_text("# Proofs of LAWS.bend — finite case analyses (one match per def; deeper cases are their own defs); `bend PROOF.bend` is the gate.\nimport Base\nimport ./dragon.bend as D\nimport ./LAWS.bend as Laws\n\n" + "\n".join(helpers) + "\n" + "\n".join(proofs))
print(f"{len(laws)} laws, {len(proofs)} proofs, {len(helpers)} helper defs")
