"""Rank the harvested grid: which signal family holds up across symbols and timeframes?

usage: python3.12 rank_grid.py [backtest_grid.csv] [--suite vcp] [--min-trades 5]
Prints per-family and per-family-per-timeframe tables; writes ranking.csv next to the grid.

Ranking rule (the "plateau, not the peak" idea): a family is only as good as its typical cell, so we score
the MEDIAN profit factor and the share of cells with PF > 1 and enough trades, then penalise drawdown.
"""
from __future__ import annotations

import argparse
import csv
import statistics as st
from collections import defaultdict
from pathlib import Path


def fnum(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


def load(path: Path, suite: str | None):
    rows = []
    with path.open(encoding="utf-8") as fh:
        for r in csv.DictReader(fh):
            if suite and r["suite"] != suite:
                continue
            for k in ("net_profit_pct", "profit_factor", "win_rate", "max_dd_pct", "trades", "robustness", "overfit_risk", "alpha_pct", "bh_pct", "tpw", "bear_pnl_pct"):
                r[k] = fnum(r.get(k))
            # TradingView prints "N/A" for the profit factor when there is no losing trade: treat as the cap
            if r["profit_factor"] is None and r["trades"] and r["net_profit_pct"] is not None and r["net_profit_pct"] > 0:
                r["profit_factor"] = 10.0
            rows.append(r)
    return rows


def summarise(rows, min_trades):
    valid = [r for r in rows if r["trades"] is not None and r["trades"] >= min_trades and r["profit_factor"] is not None]
    if not valid:
        return None
    pfs = [min(r["profit_factor"], 10.0) for r in valid]          # cap PF so 2-trade miracles don't dominate
    nets = [r["net_profit_pct"] for r in valid if r["net_profit_pct"] is not None]
    alphas = [r["alpha_pct"] for r in valid if r.get("alpha_pct") is not None]
    dds = [r["max_dd_pct"] for r in valid if r["max_dd_pct"] is not None]
    wins = [r["win_rate"] for r in valid if r["win_rate"] is not None]
    rob = [r["robustness"] for r in valid if r["robustness"] is not None]
    med_pf = st.median(pfs)
    hit = sum(1 for p in pfs if p > 1.0) / len(pfs)
    med_dd = st.median(dds) if dds else 0.0
    score = med_pf * hit * (1 - min(med_dd, 80) / 100)
    return {
        "cells": len(rows), "valid": len(valid), "median_pf": round(med_pf, 2), "pf_gt1": round(hit, 2),
        "median_net": round(st.median(nets), 1) if nets else None, "median_dd": round(med_dd, 1),
        "median_win": round(st.median(wins), 1) if wins else None, "trades": int(sum(r["trades"] for r in valid)),
        "median_alpha": round(st.median(alphas), 1) if alphas else None, "alpha_gt0": round(sum(1 for a in alphas if a > 0) / len(alphas), 2) if alphas else None,
        "typesafe_rob": round(st.mean(rob), 2) if rob else None, "score": round(score, 3),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("grid", nargs="?", default=str(Path(__file__).with_name("backtest_grid.csv")))
    ap.add_argument("--suite")
    ap.add_argument("--min-trades", type=int, default=5)
    args = ap.parse_args()
    rows = load(Path(args.grid), args.suite)
    by_sig, by_sig_tf = defaultdict(list), defaultdict(list)
    for r in rows:
        sig = r["signal"] + (" / " + r["exit"] if r.get("exit") else "")
        by_sig[(r["suite"], sig)].append(r)
        by_sig_tf[(r["suite"], sig, r["tf"])].append(r)

    table = []
    for key, rs in by_sig.items():
        s = summarise(rs, args.min_trades)
        if s:
            table.append({"suite": key[0], "signal": key[1], "tf": "all", **s})
    for key, rs in by_sig_tf.items():
        s = summarise(rs, max(3, args.min_trades // 2))
        if s:
            table.append({"suite": key[0], "signal": key[1], "tf": key[2], **s})
    table.sort(key=lambda t: (t["tf"] != "all", t["tf"], -t["score"]))

    out = Path(args.grid).with_name("ranking.csv")
    with out.open("w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=list(table[0].keys()) if table else ["suite"])
        w.writeheader()
        w.writerows(table)

    hdr = f"{'suite':9s} {'signal / exit':44s} {'tf':4s} {'cells':>5s} {'medPF':>6s} {'PF>1':>5s} {'medNet%':>8s} {'medAlpha%':>9s} {'alpha>0':>7s} {'medDD%':>7s} {'medWin%':>7s} {'trades':>6s} {'score':>6s}"
    print(hdr)
    nan = float('nan')
    for t in table:
        print(f"{t['suite']:9s} {t['signal'][:44]:44s} {t['tf']:4s} {t['cells']:5d} {t['median_pf']:6.2f} {t['pf_gt1']:5.2f} "
              f"{(t['median_net'] if t['median_net'] is not None else nan):8.1f} {(t['median_alpha'] if t['median_alpha'] is not None else nan):9.1f} {(t['alpha_gt0'] if t['alpha_gt0'] is not None else nan):7.2f} {t['median_dd']:7.1f} "
              f"{(t['median_win'] if t['median_win'] is not None else nan):7.1f} {t['trades']:6d} {t['score']:6.3f}")
    print(f"\nwrote {out}")


if __name__ == "__main__":
    main()
