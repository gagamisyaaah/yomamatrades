"""EVO 2 — exhaustive matrix mining with a successor tree, one lineage per family.

What changed from evo.py after the first runs:
- calendar blocks (weekday, day of month) are banned: they won the ENERGY lane in training and lost −0.5 ATR in the holdout;
- a regime or era with too few samples is "unverified" (rank penalty), not a death sentence — the first runs killed 95 % of
  every lane at 2020-21 for lack of bear-regime bars, not for failing them; the bear/bull test now uses every in-sample
  bar (2016-23) at every era, the holdout stays untouched;
- children are no longer 400 random additions: every PAIR of conditions inside a family is scored (tens of thousands),
  triples grow from the best pairs, quads from the best triples — an exhaustive beam, ~1 ms per rule via grouped sums;
- fitness = excess +2/−4 ATR bracket outcome over random entries of the same year × ATR band (bend/data/matrix_D.pkl),
  survival = lower confidence bound > 0 in every verified era so far; rank = weakest verified measure × penalties.

usage: python3.12 bend/evo/evo2.py [--seeds all|invented] [--tag _x] [--beam 200] [--jev]
"""
from __future__ import annotations

import argparse
import itertools
import json
import os
import sys
import time
from pathlib import Path

import numpy as np
import pandas as pd

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT)); sys.path.insert(0, str(HERE))
from evo import FAMILY_BLOCKS, ICON, INVENTED, QS, load_env  # noqa: E402

ERAS = [("E1 2016-17", 2016, 2017), ("E2 2018-19", 2018, 2019), ("E3 2020-21", 2020, 2021), ("E4 2022-23", 2022, 2023)]
BANNED = {"dow", "dom", "whales50", "whales70", "whales_lt30", "poc", "hole_up", "hole_dn", "hole_inforce", "fresh", "panels6"}   # calendar effects (overfit) and blocks no single Pine script can compute
MIN_N = 300


class Lib:
    def __init__(self, M, ins):
        self.conds, self.meta = {}, {}
        for fam, blocks in FAMILY_BLOCKS.items():
            for b in blocks:
                if b not in M or b in BANNED:
                    continue
                x = M[b].replace([np.inf, -np.inf], np.nan)
                if x.dropna().nunique() <= 2:
                    for val in (1, 0):
                        n = f"{b}={val}"; self.conds[n] = (x == val).values; self.meta[n] = {"family": fam, "block": b}
                    continue
                for q, sym, op in QS:
                    thr = float(x[ins].quantile(q)); n = f"{b}{sym}({thr:.4g})"
                    self.conds[n] = ((x <= thr) if op == "le" else (x >= thr)).values & x.notna().values
                    self.meta[n] = {"family": fam, "block": b, "q": q}
        self.by_family = {fam: [n for n in self.conds if self.meta[n]["family"] == fam] for fam in FAMILY_BLOCKS}


class Evo:
    def __init__(self, M, a):
        self.a = a
        M = M[M.x_r24.notna()].reset_index(drop=True); self.M = M
        self.y = M.x_r24.values.astype(np.float64); self.y2 = self.y ** 2; self.win = (M.r24.values > 0).astype(np.float64); self.t10 = M.x_t10.values.astype(np.float64)
        year = M.year.values
        self.hold = year >= 2024; self.ins = ~self.hold
        from mine import data
        spy = data.fetch("SPY").set_index("date")["close"]; ma = spy.rolling(200).mean(); reg = (spy > ma) | ma.isna()   # undefined average (data start) = not a bear day
        bull = M.date.map(reg).fillna(True).astype(bool).values
        # group id per row: era 0-3 (in-sample) or 4 (holdout); +5 if bear-regime → 10 groups
        era = np.full(len(M), 4);
        for i, (_, y0, y1) in enumerate(ERAS):
            era[(year >= y0) & (year <= y1)] = i
        self.g = era + 5 * (~bull).astype(int); self.G = 10
        self.lib = Lib(M, self.ins); self.nodes = {}; self.nid = 0
        self.min_n_in = int(a.min_share * self.ins.sum())   # a finalist must fire on at least this share of training bars
        self.warm = {}
        for path in (a.warm or []):
            tree = json.load(open(ROOT / path))
            for nid, n in tree["nodes"].items():
                if not n.get("died") or nid in sum((v["finalists"] for v in tree["families"].values()), []):
                    self.warm.setdefault(n["family"], []).append(n["rule"])
        if self.warm:
            print("warm start:", {k: len(set(v)) for k, v in self.warm.items()}, flush=True)
        self.jev_notes = []

    def sums(self, mask):
        idx = np.flatnonzero(mask); g = self.g[idx]
        n = np.bincount(g, minlength=self.G).astype(float); s = np.bincount(g, weights=self.y[idx], minlength=self.G)
        s2 = np.bincount(g, weights=self.y2[idx], minlength=self.G); w = np.bincount(g, weights=self.win[idx], minlength=self.G); t = np.bincount(g, weights=self.t10[idx], minlength=self.G)
        return n, s, s2, w, t

    @staticmethod
    def stat(n, s, s2, w, t):
        if n < MIN_N:
            return None
        mean = s / n; var = max(s2 / n - mean * mean, 1e-12); se = np.sqrt(var / n)
        return {"n": int(n), "mean": round(mean, 4), "lcb": round(mean - se, 4), "z": round(mean / se, 1), "win": round(w / n, 3), "t10": round(t / n, 2)}

    def fitness(self, mask, upto):
        n, s, s2, w, t = self.sums(mask)
        eras = {}; unverified = 0
        for i, (name, _, _) in enumerate(ERAS[: upto + 1]):
            st = self.stat(n[i] + n[i + 5], s[i] + s[i + 5], s2[i] + s2[i + 5], w[i] + w[i + 5], t[i] + t[i + 5])
            if st is None:
                unverified += 1; continue
            if st["lcb"] <= 0:
                return None
            eras[name] = st
        if not eras:
            return None
        bear = self.stat(*(v[5:9].sum() for v in (n, s, s2, w, t))); bull = self.stat(*(v[0:4].sum() for v in (n, s, s2, w, t)))
        for r in (bear, bull):
            if r is None:
                unverified += 1
            elif r["lcb"] <= 0:
                return None
        # rank by the weakest LOWER CONFIDENCE BOUND, not the weakest mean: round 4 showed that ranking by mean rewards ever
        # rarer, more specific rules whose holdout thins out (n ≈ 200, z ≈ 1) while broader rules (n ≈ 1,400, z ≈ 3.6) lose
        ranks = [e["lcb"] for e in eras.values()] + [r["lcb"] for r in (bear, bull) if r]
        rank = min(ranks) * (0.7 ** unverified)
        if (n[:4].sum() + n[5:9].sum()) < self.min_n_in:
            return None
        return {"eras": eras, "bear": bear, "bull": bull, "unverified": unverified, "rank": round(rank, 4), "n_in": int(n[:4].sum() + n[5:9].sum())}

    def holdout(self, mask):
        n, s, s2, w, t = self.sums(mask)
        global MIN_N
        keep = MIN_N; MIN_N = 60   # the holdout is a report, not a selection: show thin slices too
        out = {"all": self.stat(n[4] + n[9], s[4] + s[9], s2[4] + s2[9], w[4] + w[9], t[4] + t[9]), "bear": self.stat(n[9], s[9], s2[9], w[9], t[9]), "bull": self.stat(n[4], s[4], s2[4], w[4], t[4])}
        MIN_N = keep; return out

    def mask_of(self, rule):
        m = None
        for c in rule.split(" & "):
            m = self.lib.conds[c] if m is None else (m & self.lib.conds[c])
        return m

    def add(self, rule, family, era, source, parents, fit):
        self.nid += 1; nid = f"n{self.nid}"; self.nodes[nid] = {"id": nid, "rule": rule, "family": family, "era": era, "source": source, "parents": parents, **fit}; return nid

    def run_family(self, family, jev):
        conds = [c for c in self.lib.by_family[family] if self.a.seeds == "all" or self.lib.meta[c]["block"] in INVENTED]
        allc = self.lib.by_family[family]
        pop = {}
        for ei, (era, _, _) in enumerate(ERAS):
            t0 = time.time()
            if ei == 0:
                for c in conds:
                    f = self.fitness(self.lib.conds[c], 0)
                    if f:
                        pop[c] = self.add(c, family, era, "seed", [], f)
                for rule in self.warm.get(family, []):          # retroactive injection: earlier lineages re-enter and keep evolving
                    if rule in pop or any(c not in self.lib.conds for c in rule.split(" & ")):
                        continue
                    f = self.fitness(self.mask_of(rule), 0)
                    if f:
                        pop[rule] = self.add(rule, family, era, "warm", [], f)
            else:
                for rule, nid in list(pop.items()):
                    f = self.fitness(self.mask_of(rule), ei)
                    if f:
                        self.nodes[nid].update(f)
                    else:
                        self.nodes[nid]["died"] = era; pop.pop(rule)
            singles = [r for r in pop if " & " not in r]
            # exhaustive pairs among surviving singles (era 1) / beam growth from the best rules (later eras)
            ranked = sorted(pop.items(), key=lambda kv: -self.nodes[kv[1]]["rank"])
            parents = [r for r, _ in ranked[: self.a.beam]]
            cand = {}
            for r in parents:
                blocks = r.split(" & "); used = {self.lib.meta[b]["block"] for b in blocks}
                if len(blocks) >= 4:
                    continue
                for c in allc:
                    if self.lib.meta[c]["block"] in used:
                        continue
                    key = " & ".join(sorted(blocks + [c]))
                    if key not in pop and key not in cand:
                        cand[key] = ("add" if len(blocks) > 1 else "pair", [pop[r]])
                    if len(blocks) >= 2:                              # substitution: a newer block replaces one position of a bred rule
                        for b in blocks:
                            key2 = " & ".join(sorted([x for x in blocks if x != b] + [c]))
                            if key2 not in pop and key2 not in cand:
                                cand[key2] = (f"swap {self.lib.meta[b]['block']}→{self.lib.meta[c]['block']}", [pop[r]])
            if jev:
                for rule, why in jev.propose(family, [(r, self.nodes[pop[r]]) for r in parents[:6]], self.lib, era):
                    if rule and rule not in pop and rule not in cand:
                        cand[rule] = ("jev: " + why, [pop[parents[0]]])
            born = 0; t1 = time.time()
            for key, (src, par) in cand.items():
                f = self.fitness(self.mask_of(key), ei)
                if f and f["rank"] > max(self.nodes[p]["rank"] for p in par) * 1.03:
                    pop[key] = self.add(key, family, era, src, par, f); born += 1
            ranked = sorted(pop.items(), key=lambda kv: -self.nodes[kv[1]]["rank"])
            for r, nid in ranked[self.a.beam * 3:]:
                self.nodes[nid]["died"] = era + " (cut)"; pop.pop(r)
            best = ranked[0] if ranked else None
            print(f"{family} {era}: scored {len(cand)} candidates in {time.time()-t1:.0f}s, population {len(pop)}, born {born}, best {best[0] if best else '—'} rank {self.nodes[best[1]]['rank'] if best else float('nan'):.3f} ({time.time()-t0:.0f}s)", flush=True)
        return [nid for _, nid in sorted(pop.items(), key=lambda kv: -self.nodes[kv[1]]["rank"])]


class Jev:
    def __init__(self):
        load_env(); from typesafe_judge import ask; self.ask = ask; self.calls = 0; self.notes = []

    def propose(self, family, top, lib, era):
        out = []
        for rule, node in top[:4]:
            blocks = rule.split(" & "); used = {lib.meta[b]["block"] for b in blocks}
            cands = [c for c in lib.by_family[family] if lib.meta[c]["block"] not in used]
            rs = np.random.RandomState(len(rule) + len(era)); pick = list(rs.choice(cands, size=min(8, len(cands)), replace=False))
            try:
                a = self.ask({"family": family, "era": era, "rule": rule, "fitness_by_era": {k: v["mean"] for k, v in node["eras"].items()}, "note": "excess +2/−4 ATR bracket outcome over random entries of the same year and volatility band"},
                             {"add": {"type": "choice", "instructions": "Which building block would most plausibly strengthen this entry rule as a repeatable market effect (not an artefact of the exit)?", "criteria": {c: c for c in pick}},
                              "plausible": {"type": "noul", "instructions": "Is this rule a plausible, documented market effect rather than an artefact of ATR normalisation or of the exit rule?"}})
                self.calls += 1; add = a["add"].get("choice"); pl = a["plausible"].get("noul")
                self.notes.append({"era": era, "family": family, "rule": rule, "jev_add": add, "jev_plausible": pl})
                if add in lib.conds:
                    out.append((" & ".join(sorted(blocks + [add])), f"add {add}"))
            except Exception as e:  # noqa: BLE001
                self.notes.append({"era": era, "family": family, "rule": rule, "error": str(e)[:120]})
        return out


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--seeds", default="all", choices=["all", "invented"]); ap.add_argument("--tag", default=""); ap.add_argument("--beam", type=int, default=200); ap.add_argument("--jev", action="store_true"); ap.add_argument("--matrix", default="bend/data/matrix_D.pkl"); ap.add_argument("--warm", nargs="*", help="previous evo_tree*.json files whose survivors re-enter the population"); ap.add_argument("--min-share", type=float, default=0.004); ap.add_argument("--cap-min", type=float, default=0); ap.add_argument("--cap-max", type=float, default=1e15)
    a = ap.parse_args()
    M = pd.read_pickle(ROOT / a.matrix)
    if "cap" in M and (a.cap_min > 0 or a.cap_max < 1e15):
        M = M[(M.cap >= a.cap_min) & (M.cap <= a.cap_max)].reset_index(drop=True); print(f"cap filter {a.cap_min:,.0f}–{a.cap_max:,.0f}: {M.ticker.nunique()} tickers, {len(M)} bars")
    ev = Evo(M, a); jev = Jev() if a.jev else None
    print(f"{len(ev.lib.conds)} conditions over {len(ev.M)} labelled bars; families: " + ", ".join(f"{f} {len(v)}" for f, v in ev.lib.by_family.items()), flush=True)
    finals = {}
    for fam in FAMILY_BLOCKS:
        top = ev.run_family(fam, jev)
        for nid in top[:15]:
            h = ev.holdout(ev.mask_of(ev.nodes[nid]["rule"])); ev.nodes[nid]["holdout"] = h["all"]; ev.nodes[nid]["holdout_bear"] = h["bear"]; ev.nodes[nid]["holdout_bull"] = h["bull"]
        finals[fam] = top[:15]
    tree = {"eras": [e[0] for e in ERAS], "holdout": "2024+", "families": {f: {"icon": ICON[f], "finalists": finals[f]} for f in FAMILY_BLOCKS}, "nodes": ev.nodes, "jev": jev.notes if jev else []}
    json.dump(tree, open(ROOT / f"bend/data/evo_tree{a.tag}.json", "w"), indent=1, default=str)
    print(f"\n{len(ev.nodes)} nodes; jev calls: {jev.calls if jev else 0}")
    for fam, ids in finals.items():
        print(f"\n== {ICON[fam]} {fam}: finalists (rank = weakest verified measure; holdout 2024+ never used to select) ==")
        for nid in ids[:8]:
            n = ev.nodes[nid]; h = n.get("holdout") or {}; hb = n.get("holdout_bear") or {}
            print(f"  {n['rule']:<80s} {n['era'][:2]} {n['source']:<6s} rank {n['rank']:+.3f} bear {n['bear']['mean'] if n['bear'] else float('nan'):+.3f} bull {n['bull']['mean'] if n['bull'] else float('nan'):+.3f} | holdout {h.get('mean', float('nan')):+.3f} z {h.get('z', 0)} win {h.get('win', 0)} n {h.get('n', 0)} | holdout-bear {hb.get('mean', float('nan')):+.3f} n {hb.get('n', 0)}")


if __name__ == "__main__":
    main()
