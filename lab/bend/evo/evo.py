"""EVO — the master evolutionary program for entry rules, one lineage tree per family.

Data: bend/data/matrix_D.pkl — 496,799 random-entry bars across 2,174 US stocks with ~90 building blocks (indicator
parameterisations computed from bars ≤ the signal bar) and the engine's outcome labels (entry next open; +2/−4 ATR
bracket over 20 bars is the fitness outcome, de-meaned within year × ATR-band cells so a rule is only rewarded for
beating random entries of the same year and volatility).

Evolution in years: eras E1 2016-17, E2 2018-19, E3 2020-21, E4 2022-23 are training sets taken sequentially; a rule
must be positive (lower confidence bound) in the current era AND every earlier era to survive. 2024+ is the holdout,
never used to select, reported at the end as the proof. Each era: score the population, keep the survivors, breed
children (add a block, tweak a threshold, drop a block, cross two survivors), inject Jev's proposals, re-score.
Every node keeps its parent(s), era of birth, source and per-era fitness → bend/data/evo_tree.json. Only blocks a
single chart can compute (no cross-sectional ranks) are used, so every finalist is a Pine indicator.

usage: python3.12 bend/evo/evo.py [--jev] [--top 40] [--children 400]
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
sys.path.insert(0, str(ROOT))

ERAS = [("E1 2016-17", 2016, 2017), ("E2 2018-19", 2018, 2019), ("E3 2020-21", 2020, 2021), ("E4 2022-23", 2022, 2023)]
HOLDOUT = ("2024+", 2024, 2100)
FAMILY_BLOCKS = {
    # the shape of the move: swing pivots, legs, pullback depth, ranges, distance to the long averages
    "STRUCTURE": ["dd_dur60", "du_dur60", "bubble", "deadband20", "cant20", "remission", "retreat", "quantile250", "local_time50", "d_hi250", "d_lo250", "overshoot20", "foundation60", "env20", "pid_i20", "kurt60", "skew60", "tail250", "avwap_pl", "avwap_ph", "bo_bars20", "damping", "fractal30", "asym20", "time_sym", "vwap_dev50", "recovery", "resilience", "stable20", "gap_fill10", "d_ph", "d_pl", "bars_ph", "bars_pl", "hh", "hl", "struct", "leg_atr", "leg_bars", "retrace", "up_leg", "bars_52w_hi", "hi20", "hi50", "hi126", "hi252", "lo20", "lo50", "lo126", "lo252", "hl_pos20", "dsma50", "dsma100", "dsma200", "dema50", "dema100", "dema200", "dstreak", "ustreak", "clv", "body", "mom12_1"],
    # the force of the move: velocity, acceleration, efficiency, oscillators, flow
    "FORCE": ["win_mom20", "bandwagon", "kyle60", "red_queen", "inertia", "sg_slope11", "price_disc10", "pid_d5", "info_ratio20", "work10", "vfi20", "obv_osc20", "weis_ratio", "kinetic20", "p_mass20", "impulse10", "phase", "resonance20", "vratio50", "predator_prey10", "pp_drift", "r0", "thrust5", "aggression10", "snr20", "flatten", "ou_speed60", "lr5", "lr20", "lr60", "accel", "slope10", "slope20", "slope50", "vel_ratio", "eff10", "eff20", "eff50", "rsi2", "rsi3", "rsi5", "rsi7", "rsi14", "rsi21", "stoch5", "stoch14", "cci14", "cci20", "roc1", "roc3", "roc5", "roc10", "roc20", "roc60", "roc120", "roc252", "dsma5", "dsma10", "dsma20", "dema5", "dema10", "dema20", "bbpct10", "bbpct20", "adx14", "dmi_diff", "macd_hist", "macd_dif", "obv_slope20", "ret_gap_5", "whales50", "whales70", "whales_lt30", "te_up", "macdrsi", "ribbon", "ss", "poc", "mtn3", "gold"],
    # the energy state: compression / expansion, gaps, ranges, volume, liquidity, the hole and the bubble
    "ENERGY": ["amihud20", "capit5", "euph5", "crit_mass", "mad20", "pot60", "roll60", "anneal20", "siege20", "vratio20", "regime_age", "mi60", "sqz_on", "sqz_mom20", "st_dist", "st_dir", "st_age", "spring20", "impact20", "strain", "metabolic", "atrr5_20", "atrr5_50", "atrr14_50", "atrr20_100", "rng5", "rng10", "rng20", "bbw10", "bbw20", "atrpct", "squeeze_bars", "volr5", "volr10", "volr20", "volr50", "vol_z20", "dvol20", "gap", "range_atr", "hole_up", "hole_dn", "hole_inforce", "fresh", "hot", "panels6", "dow", "dom"],
}
ICON = {"STRUCTURE": "🏔", "FORCE": "⚡", "ENERGY": "🌊"}
INVENTED = {"dd_dur60", "du_dur60", "bubble", "deadband20", "cant20", "remission", "retreat", "quantile250", "local_time50", "d_hi250", "d_lo250", "overshoot20", "foundation60", "env20", "pid_i20", "kurt60", "skew60", "tail250", "win_mom20", "bandwagon", "kyle60", "red_queen", "inertia", "sg_slope11", "price_disc10", "pid_d5", "info_ratio20", "work10", "amihud20", "capit5", "euph5", "crit_mass", "mad20", "pot60", "roll60", "anneal20", "siege20", "vratio20", "regime_age", "mi60", "avwap_pl", "avwap_ph", "bo_bars20", "vfi20", "obv_osc20", "weis_ratio", "sqz_on", "sqz_mom20", "st_dist", "st_dir", "st_age", "damping", "fractal30", "asym20", "time_sym", "vwap_dev50", "recovery", "resilience", "stable20", "gap_fill10", "kinetic20", "p_mass20", "impulse10", "phase", "resonance20", "vratio50", "predator_prey10", "pp_drift", "r0", "thrust5", "aggression10", "snr20", "flatten", "ou_speed60", "spring20", "impact20", "strain", "metabolic"}
QS = [(0.1, "≤p10", "le"), (0.25, "≤p25", "le"), (0.75, "≥p75", "ge"), (0.9, "≥p90", "ge")]


def load_env():
    f = ROOT / ".env"
    if f.exists():
        for ln in f.read_text().splitlines():
            if "=" in ln and not ln.strip().startswith("#"):
                k, v = ln.split("=", 1); os.environ.setdefault(k.strip(), v.strip().strip('"'))


class Lib:
    """the condition library: name → (mask, family, block, op, threshold)"""

    def __init__(self, M: pd.DataFrame, ins: np.ndarray):
        self.conds: dict[str, np.ndarray] = {}; self.meta: dict[str, dict] = {}
        for fam, blocks in FAMILY_BLOCKS.items():
            for b in blocks:
                if b not in M:
                    continue
                x = M[b].replace([np.inf, -np.inf], np.nan)
                if x.dropna().nunique() <= 2:
                    for val in (1, 0):
                        n = f"{b}={val}"; self.conds[n] = (x == val).values; self.meta[n] = {"family": fam, "block": b, "op": "eq", "thr": val}
                    continue
                if b == "dow":
                    for d in range(5):
                        n = f"dow={d}"; self.conds[n] = (x == d).values; self.meta[n] = {"family": fam, "block": b, "op": "eq", "thr": d}
                    continue
                qs = x[ins].quantile([q for q, _, _ in QS] + [0.05, 0.15, 0.35, 0.65, 0.85, 0.95])
                for q, sym, op in QS:
                    thr = float(qs[q]); n = f"{b}{sym}({thr:.4g})"
                    self.conds[n] = ((x <= thr) if op == "le" else (x >= thr)).values & x.notna().values
                    self.meta[n] = {"family": fam, "block": b, "op": op, "thr": thr, "q": q, "x": x}
        self.by_family = {fam: [n for n in self.conds if self.meta[n]["family"] == fam] for fam in FAMILY_BLOCKS}

    def tweak(self, name: str) -> str | None:
        """neighbouring quantile of the same block (a threshold mutation)"""
        m = self.meta[name]
        nxt = {0.1: 0.05, 0.25: 0.15, 0.75: 0.85, 0.9: 0.95, 0.05: 0.02, 0.15: 0.1, 0.85: 0.9, 0.95: 0.98}
        if "q" not in m or m["q"] not in nxt:
            return None
        q2 = nxt[m["q"]]
        thr = float(m["x"].quantile(q2)); sym = ("≤" if m["op"] == "le" else "≥") + f"p{int(q2*100)}"
        n = f"{m['block']}{sym}({thr:.4g})"
        if n not in self.conds:
            self.conds[n] = ((m["x"] <= thr) if m["op"] == "le" else (m["x"] >= thr)).values & m["x"].notna().values
            self.meta[n] = {**m, "thr": thr, "q": q2}
        return n


class Evo:
    def __init__(self, M: pd.DataFrame, a):
        self.M = M; self.a = a
        self.y = M.x_r24.values; self.win = (M.r24.values > 0).astype(float); self.t10 = M.x_t10.values
        self.year = M.year.values
        self.era_masks = {name: (self.year >= y0) & (self.year <= y1) for name, y0, y1 in ERAS}
        self.hold = (self.year >= HOLDOUT[1])
        self.ins = ~self.hold
        from mine import data
        spy = data.fetch("SPY").set_index("date")["close"]; ma = spy.rolling(200).mean(); reg = (spy > ma) | ma.isna()   # undefined average (data start) = not a bear day
        self.bull = M.date.map(reg).fillna(True).astype(bool).values   # SPY above its 200-day on the signal date
        self.bear = ~self.bull
        self.lib = Lib(M, self.ins)
        self.nodes: dict[str, dict] = {}; self.nid = 0

    # ── fitness ──
    def stat(self, mask, era_mask):
        m = mask & era_mask; n = int(m.sum())
        if n < 400:
            return None
        v = self.y[m]; mean = float(v.mean()); se = float(v.std() / np.sqrt(n))
        return {"n": n, "mean": round(mean, 4), "lcb": round(mean - se, 4), "z": round(mean / se, 1) if se else 0.0, "win": round(float(self.win[m].mean()), 3), "t10": round(float(self.t10[m].mean()), 2)}

    def fitness(self, mask, upto: int):
        """per-era stats through era index `upto`; survives only if lcb > 0 in every era so far AND on the pooled bear-regime
        bars AND on the pooled bull-regime bars of those eras (a rule must work in both markets); rank = the weakest of those"""
        eras = {}
        sofar = np.zeros(len(self.M), bool)
        for name, _, _ in ERAS[: upto + 1]:
            s = self.stat(mask, self.era_masks[name])
            if s is None or s["lcb"] <= 0:
                return None
            eras[name] = s; sofar |= self.era_masks[name]
        bear = self.stat(mask, sofar & self.bear); bull = self.stat(mask, sofar & self.bull)
        if bear is None or bear["lcb"] <= 0 or bull is None or bull["lcb"] <= 0:
            return None
        return {"eras": eras, "bear": bear, "bull": bull, "rank": min([s["mean"] for s in eras.values()] + [bear["mean"], bull["mean"]]), "n_in": int((mask & self.ins).sum())}

    def mask_of(self, rule: str):
        m = np.ones(len(self.M), bool)
        for c in rule.split(" & "):
            m &= self.lib.conds[c]
        return m

    def add_node(self, rule, family, era, source, parents, fit):
        self.nid += 1; nid = f"n{self.nid}"
        self.nodes[nid] = {"id": nid, "rule": rule, "family": family, "era": era, "source": source, "parents": parents, **fit}
        return nid

    # ── one family ──
    def run_family(self, family: str, jev) -> list[str]:
        pop: dict[str, str] = {}   # rule → node id
        log = []
        for ei, (era, _, _) in enumerate(ERAS):
            t0 = time.time()
            # 1. seeds (era 1 only): every single condition of the family
            if ei == 0:
                for c in self.lib.by_family[family]:
                    if self.a.seeds == 'invented' and self.lib.meta[c]['block'] not in INVENTED:
                        continue   # the roots of every lineage are self-written blocks; the rest may only join later
                    f = self.fitness(self.lib.conds[c], ei)
                    if f:
                        pop[c] = self.add_node(c, family, era, "seed", [], f)
            else:
                # 2. re-score the survivors on the new era; drop the ones that fail it
                dead = []
                for rule, nid in pop.items():
                    f = self.fitness(self.mask_of(rule), ei)
                    if f:
                        self.nodes[nid].update(f)
                    else:
                        dead.append(rule); self.nodes[nid]["died"] = era
                for r in dead:
                    pop.pop(r)
            ranked = sorted(pop.items(), key=lambda kv: -self.nodes[kv[1]]["rank"])[: self.a.top]
            # 3. breed from the top survivors
            children: dict[str, tuple[str, list[str]]] = {}
            all_conds = [c for c in self.lib.conds if self.lib.meta[c]['family'] == family]   # breeding stays inside the family
            rng = np.random.RandomState(ei + 7)
            for rule, nid in ranked:
                blocks = rule.split(" & ")
                cand = [c for c in all_conds if self.lib.meta[c]["block"] not in {self.lib.meta[b]["block"] for b in blocks}]
                for c in rng.choice(cand, size=min(len(cand), self.a.children // max(1, len(ranked))), replace=False):
                    if len(blocks) < 4:
                        key = " & ".join(sorted(blocks + [c])); children.setdefault(key, ("add", [nid]))
                for b in blocks:
                    t = self.lib.tweak(b)
                    if t:
                        key = " & ".join(sorted([x for x in blocks if x != b] + [t])); children.setdefault(key, ("tweak", [nid]))
                    if len(blocks) >= 3:
                        key = " & ".join(sorted(x for x in blocks if x != b)); children.setdefault(key, ("drop", [nid]))
            for (r1, n1), (r2, n2) in itertools.combinations(ranked[:12], 2):
                b1, b2 = r1.split(" & "), r2.split(" & ")
                if len({self.lib.meta[b]["block"] for b in b1 + b2}) == len(b1 + b2) and len(b1 + b2) <= 4:
                    children.setdefault(" & ".join(sorted(b1 + b2)), ("cross", [n1, n2]))
            # 4. Jev's proposals
            if jev:
                for rule, why in jev.propose(family, [(r, self.nodes[n]) for r, n in ranked[:6]], self.lib, era):
                    if rule and rule not in pop:
                        children.setdefault(rule, ("jev: " + why, [n for _, n in ranked[:1]]))
            born = 0
            for key, (src, parents) in children.items():
                if key in pop:
                    continue
                f = self.fitness(self.mask_of(key), ei)
                if f and f["rank"] > max(self.nodes[p]["rank"] for p in parents) * 1.02:
                    pop[key] = self.add_node(key, family, era, src, parents, f); born += 1
            # 5. cap the population
            ranked = sorted(pop.items(), key=lambda kv: -self.nodes[kv[1]]["rank"])
            for rule, nid in ranked[self.a.top * 2:]:
                self.nodes[nid]["died"] = era + " (cut)"; pop.pop(rule)
            log.append(f"{family} {era}: population {len(pop)}, born {born}, best {ranked[0][0]} rank {self.nodes[ranked[0][1]]['rank']:.3f} ({time.time()-t0:.0f}s)")
            print(log[-1], flush=True)
        return [nid for _, nid in sorted(pop.items(), key=lambda kv: -self.nodes[kv[1]]["rank"])]

    def holdout(self, nid: str):
        m = self.mask_of(self.nodes[nid]["rule"])
        s = self.stat(m, self.hold); self.nodes[nid]["holdout"] = s
        self.nodes[nid]["holdout_bear"] = self.stat(m, self.hold & self.bear); self.nodes[nid]["holdout_bull"] = self.stat(m, self.hold & self.bull)
        return s


class Jev:
    def __init__(self):
        load_env()
        from typesafe_judge import ask  # noqa
        self.ask = ask; self.calls = 0; self.notes = []

    def propose(self, family, top, lib: Lib, era):
        """typed proposals: for each of the top rules, which block to add (choice) — and whether the rule is a plausible effect (noul)"""
        out = []
        for rule, node in top[:4]:
            blocks = rule.split(" & ")
            cands = [c for c in lib.conds if lib.meta[c]["family"] == family and lib.meta[c]["block"] not in {lib.meta[b]["block"] for b in blocks}]
            rs = np.random.RandomState(len(rule) + len(era)); pick = list(rs.choice(cands, size=min(8, len(cands)), replace=False))
            try:
                a = self.ask({"family": family, "era": era, "rule": rule, "fitness_by_era": {k: v["mean"] for k, v in node["eras"].items()}, "win_rate": node["eras"][era]["win"], "note": "excess +2/−4 ATR bracket outcome over random entries of the same year and volatility band"},
                             {"add": {"type": "choice", "instructions": "Which building block would most plausibly strengthen this entry rule as a repeatable market effect (not an artefact of the exit)?", "criteria": {c: c for c in pick}},
                              "plausible": {"type": "noul", "instructions": "Is this rule a plausible, documented market effect rather than an artefact of ATR normalisation or of the exit rule?"}})
                self.calls += 1
                add = a["add"].get("choice"); pl = a["plausible"].get("noul")   # typed answers: a choice + a probability
                self.notes.append({"era": era, "family": family, "rule": rule, "jev_add": add, "jev_plausible": pl})
                if add in lib.conds:
                    out.append((" & ".join(sorted(blocks + [add])), f"add {add}"))
            except Exception as e:  # noqa: BLE001
                self.notes.append({"era": era, "family": family, "rule": rule, "error": str(e)[:120]})
        return out


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--jev", action="store_true"); ap.add_argument("--top", type=int, default=40); ap.add_argument("--children", type=int, default=400); ap.add_argument("--seeds", default="all", choices=["all", "invented"]); ap.add_argument("--tag", default="")
    a = ap.parse_args()
    M = pd.read_pickle(ROOT / "bend/data/matrix_D.pkl")
    ev = Evo(M, a); jev = Jev() if a.jev else None
    finals = {}
    for fam in FAMILY_BLOCKS:
        top = ev.run_family(fam, jev)
        for nid in top[:10]:
            ev.holdout(nid)
        finals[fam] = top[:10]
    tree = {"eras": [e[0] for e in ERAS], "holdout": HOLDOUT[0], "families": {f: {"icon": ICON[f], "finalists": finals[f]} for f in FAMILY_BLOCKS}, "nodes": ev.nodes, "jev": jev.notes if jev else []}
    json.dump(tree, open(ROOT / f"bend/data/evo_tree{a.tag}.json", "w"), indent=1, default=str)
    print(f"\n{len(ev.nodes)} nodes; jev calls: {jev.calls if jev else 0}")
    for fam, ids in finals.items():
        print(f"\n== {ICON[fam]} {fam}: finalists (rank = weakest training era; holdout 2024+ never used to select) ==")
        for nid in ids[:6]:
            n = ev.nodes[nid]; h = n.get("holdout") or {}
            hb = n.get("holdout_bear") or {}
            print(f"  {n['rule']:<75s} born {n['era']} via {n['source']:<12s} weakest {n['rank']:+.3f} (bear {n['bear']['mean']:+.3f}/bull {n['bull']['mean']:+.3f})  holdout {h.get('mean', float('nan')):+.3f} ATR z {h.get('z', 0)} win {h.get('win', 0)} n {h.get('n', 0)}  holdout-bear {hb.get('mean', float('nan')):+.3f} (n {hb.get('n', 0)})")


if __name__ == "__main__":
    main()
