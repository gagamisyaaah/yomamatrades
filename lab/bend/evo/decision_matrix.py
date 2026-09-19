"""The multi-dimensional payoff matrix: entry × exit × cap band × ATR band × regime × family agreement, each cell scored
against random entries of the same year × ATR band with the same exit. In-sample and holdout separately; the Pareto
front (excess per trade × trades per year) is the shortlist for live TradingView confirmation.
usage: python3.12 bend/evo/decision_matrix.py [--matrix bend/data/matrix_D.pkl] [--min-n 150]"""
from __future__ import annotations

import argparse
import json
import sys
from itertools import product
from pathlib import Path

import numpy as np
import pandas as pd

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT))

RULES = {"STRUCTURE": [("asym20", "≤", -0.874), ("clv", "≤", 0.08271), ("d_pl", "≤", 0.7581), ("lo50", "≤", 105.7)],
         "FORCE": [("eff10", "≤", 0.134), ("gold", "=", 0), ("lr20", "≤", -0.09698), ("macd_hist", "≤", -0.1326)],
         "ENERGY": [("gap", "≥", 0.1867), ("metabolic", "≥", 1.872), ("range_atr", "≥", 1.084), ("st_dist", "≤", -2.226)]}
EXITS = {"bracket +2/−4 20b": "r24", "bracket +3/−4 20b": "r34", "scalp +0.5/−3 10b": "s53", "close 10 bars": "t10", "hole down": "flip"}
PCT = {"t10", "flip"}


def mask(M, conds):
    m = np.ones(len(M), bool)
    for b, op, thr in conds:
        x = M[b].values.astype(float); m &= (x <= thr) if op == "≤" else ((x >= thr) if op == "≥" else (x == thr)); m &= ~np.isnan(x)
    return m


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--matrix", default="bend/data/matrix_D.pkl"); ap.add_argument("--min-n", type=int, default=150); ap.add_argument("--split", default="2024-01-01")
    a = ap.parse_args()
    M = pd.read_pickle(ROOT / a.matrix)
    if "cap" not in M:
        caps = pd.read_csv(ROOT / "mine/caps.csv").set_index("symbol")["cap"]; M["cap"] = M.ticker.map(caps)
    M["capband"] = pd.cut(M.cap, [0, 5e8, 2e9, 5e9, 1e15], labels=["100M-500M", "500M-2B", "2B-5B", ">5B"])
    from mine import data
    spy = data.fetch("SPY").set_index("date")["close"]; ma = spy.rolling(200).mean(); reg = (spy > ma) | ma.isna()
    M["regime"] = np.where(M.date.map(reg).fillna(True).astype(bool), "bull", "bear")
    M["period"] = np.where(M.date >= a.split, "holdout", "in-sample")
    fam = {f: mask(M, c) for f, c in RULES.items()}
    agree = sum(v.astype(int) for v in fam.values())
    entries = {**fam, "any family": agree >= 1, "≥2 families": agree >= 2, "Dragon C": ((M.hole_up == 1) & (M.whales50 == 1) & (M.te_up == 1) & (M.hot == 0) & (M.mtn3 == 1) & (M.gold == 1)).values, "hole up": (M.hole_up == 1).values}
    # random control mean per (period, year, band, exit) — the same-exit baseline
    ctrl = {col: M.groupby(["year", "band"], observed=True)[col].mean() for col in EXITS.values()}
    rows = []
    days = M.groupby("period").date.nunique()
    for ename, em in entries.items():
        for exname, col in EXITS.items():
            base = M[col].values - ctrl[col].reindex(list(zip(M.year, M.band))).values
            for per, cb, ab, rg in product(["in-sample", "holdout"], ["all"] + list(M.capband.cat.categories), ["all", "<3", "3-5", "5-8", "8-12", "≥12"], ["all", "bull", "bear"]):
                sel = em & (M.period.values == per)
                if cb != "all":
                    sel &= (M.capband.values == cb)
                if ab != "all":
                    sel &= (M.band.values == ab)
                if rg != "all":
                    sel &= (M.regime.values == rg)
                n = int(sel.sum())
                if n < a.min_n:
                    continue
                ex = base[sel]; raw = M[col].values[sel]
                per_year = n / days[per] * 252 * 10   # sampled bars ≈ 10 % of all bars
                rows.append({"entry": ename, "exit": exname, "period": per, "cap": cb, "atr": ab, "regime": rg, "n": n, "trades_per_year": round(per_year),
                             "excess": round(float(np.nanmean(ex)), 3), "z": round(float(np.nanmean(ex) / (np.nanstd(ex) / np.sqrt(n))), 1), "win": round(float((raw > 0).mean()), 3),
                             "unit": "%" if col in PCT else "ATR", "per_trade_pct": round(float(np.nanmean(ex * (1 if col in PCT else M.atr_pct.values[sel]))), 2)})
    D = pd.DataFrame(rows); D.to_csv(ROOT / "bend/data/decision_matrix.csv", index=False)
    pd.set_option("display.width", 250)
    H = D[(D.period == "holdout") & (D.z >= 2.5)].copy(); H["edge_x_freq"] = H.per_trade_pct * H.trades_per_year / 100
    # Pareto: no other cell has both higher per-trade % and higher frequency
    pts = H[["per_trade_pct", "trades_per_year"]].values; keep = []
    for i, (e, f) in enumerate(pts):
        keep.append(not np.any((pts[:, 0] >= e) & (pts[:, 1] >= f) & ((pts[:, 0] > e) | (pts[:, 1] > f))))
    P = H[np.array(keep)].sort_values("per_trade_pct", ascending=False)
    print(f"{len(D)} cells scored; {len(H)} holdout cells with z ≥ 2.5; Pareto front:")
    print(P[["entry", "exit", "cap", "atr", "regime", "n", "trades_per_year", "per_trade_pct", "excess", "unit", "win", "z"]].to_string(index=False))
    print("\ntop 25 holdout cells by per-trade % over random (z ≥ 2.5, n ≥ min):")
    print(H.sort_values("per_trade_pct", ascending=False).head(25)[["entry", "exit", "cap", "atr", "regime", "n", "trades_per_year", "per_trade_pct", "win", "z"]].to_string(index=False))
    print("\ntop 15 by edge × frequency (expected % per year on $1 per trade):")
    print(H.sort_values("edge_x_freq", ascending=False).head(15)[["entry", "exit", "cap", "atr", "regime", "n", "trades_per_year", "per_trade_pct", "win", "z", "edge_x_freq"]].to_string(index=False))
    json.dump(P.to_dict("records"), open(ROOT / "bend/data/decision_pareto.json", "w"), indent=1, default=str)


if __name__ == "__main__":
    main()
