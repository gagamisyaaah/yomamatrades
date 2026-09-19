"""Portfolio view of the RUNNER lane: many names open at once, marked daily from the cache, so the account's drawdown is
measured instead of one name's. Trades come from a hole_flatten run (bend/data/hole_flatten<tag>.csv, kind = 'hole up').
usage: python3.12 bend/evo/portfolio_runner.py --tag _stop0 [--slots 15] [--per-trade 10000] [--cap-min 5e8] [--cap-max 5e9] [--atr-min 5] [--gate]"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np
import pandas as pd

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT))
from mine import ta  # noqa: E402


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--tag", default="_stop0"); ap.add_argument("--slots", type=int, default=15); ap.add_argument("--per-trade", type=float, default=10000)
    ap.add_argument("--cap-min", type=float, default=5e8); ap.add_argument("--cap-max", type=float, default=5e9); ap.add_argument("--atr-min", type=float, default=5.0); ap.add_argument("--gate", action="store_true", help="skip entries while the 21-SMA is > 5 % below the 200-SMA")
    a = ap.parse_args()
    T = pd.read_csv(ROOT / f"bend/data/hole_flatten{a.tag}.csv", parse_dates=["date"])
    T = T[(T.kind == "hole up") & (T.cap >= a.cap_min) & (T.cap <= a.cap_max)].sort_values("date")
    marks = {}; kept = []; skipped_atr = 0; skipped_gate = 0
    for t, g in T.groupby("ticker"):
        df = pd.read_csv(ROOT / f"mine/cache/{t}.csv", parse_dates=["date"]); c = df.close.values; o = df.open.values
        atrp = (ta.atr(df, 14) / df.close * 100).values; gate = (df.close.rolling(21).mean() < df.close.rolling(200).mean() * 0.95).values
        for r in g.itertuples(index=False):
            i, j = int(r.bar), int(r.exit_bar)
            if atrp[i] < a.atr_min or np.isnan(atrp[i]):
                skipped_atr += 1; continue
            if a.gate and gate[i]:
                skipped_gate += 1; continue
            ent = o[i + 1]; path = c[i + 1:j + 1] / ent - 1.0
            if r.reason == "stop":
                path = np.minimum(path, r.ret / 100.0); path[-1] = r.ret / 100.0
            kept.append({"ticker": t, "start": df.date.iloc[i + 1], "dates": df.date.iloc[i + 1:j + 1].values, "path": path})
    kept.sort(key=lambda x: x["start"])
    # slot allocation in date order; equity marked daily = cash + open positions
    open_ = []; pnl = {}; taken = 0; missed = 0
    for tr in kept:
        d0 = tr["start"]
        open_ = [x for x in open_ if x["dates"][-1] >= d0]
        if len(open_) >= a.slots:
            missed += 1; continue
        open_.append(tr); taken += 1
        prev = 0.0
        for d, p in zip(tr["dates"], tr["path"]):
            pnl[d] = pnl.get(d, 0.0) + (p - prev) * a.per_trade; prev = p
    eq = pd.Series(pnl).sort_index().cumsum(); base = a.slots * a.per_trade
    equity = base + eq; peak = equity.cummax(); dd = (equity / peak - 1.0)
    years = (equity.index[-1] - equity.index[0]).days / 365.25
    print(f"RUNNER portfolio · {a.slots} slots × ${a.per_trade:,.0f} (base ${base:,.0f}) · cap ${a.cap_min/1e9:.1f}–{a.cap_max/1e9:.0f}B · ATR ≥ {a.atr_min} % · gate {'ON' if a.gate else 'off'} · stop tag {a.tag}")
    print(f"  trades taken {taken}, missed (slots full) {missed}, skipped ATR {skipped_atr}, skipped gate {skipped_gate}")
    print(f"  final ${equity.iloc[-1]:,.0f} → {(equity.iloc[-1]/base)**(1/years)-1:+.1%}/yr over {years:.1f} y · max drawdown {dd.min():.1%} on {dd.idxmin().date()} · worst year {equity.resample('YE').last().pct_change().min():+.1%}")
    yr = equity.resample("YE").last(); print("  per year: " + "  ".join(f"{d.year} {v:+.0%}" for d, v in yr.pct_change().dropna().items()))
    hold = equity[equity.index >= "2024-01-01"]
    if len(hold):
        print(f"  holdout 2024+: {hold.iloc[-1]/hold.iloc[0]-1:+.1%} · max DD {(hold/hold.cummax()-1).min():.1%}")


if __name__ == "__main__":
    main()
