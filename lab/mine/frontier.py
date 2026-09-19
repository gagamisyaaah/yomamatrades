"""Hit-rate frontier for the Dragon C signals: what share of trades end profitable under each exit design,
and what the average trade is worth then. The founder's requirement is ≥ 80 % profitable trades with many trades.

usage: python3.12 -m mine.frontier [--features features] [--split 2024-01-01]
Grid: target +X ATR × stop −Y ATR × max N bars (first touched wins; both same bar → stop) × entry style
(next open / next open only if the gap ≤ 0.5 ATR / pullback limit at signal close − 0.5 ATR within 3 bars, else skip)
× setup quality (all / fresh break / fresh + ≥ 6 panels / ATR ≥ 5 %).  Also "bank half at +1 ATR, trail the rest".
Writes mine/frontier.md + frontier.csv; prints the rows with ≥ 80 % winners and positive expectancy.
"""
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd

from . import data, features

HERE = Path(__file__).parent


def signals(df: pd.DataFrame) -> pd.DataFrame:
    d = df
    fresh = (d["dr_hole_state"] == 1) & (d.groupby("ticker")["dr_hole_state"].shift(3) != 1)
    ent = (d["dr_hole_state"] == 1) & (d["dr_whales"] >= 50) & d["dr_te_bull"] & ~d["di_overheated"] & (d["di_mountain"] >= 3) & d["di_above_gold"] & d["regime_bull"]
    rising = ent & ~ent.groupby(d["ticker"]).shift(1, fill_value=False)
    s = d.loc[rising, ["ticker", "date", "atr_pct", "dr_n_bull", "dr_whales", "di_mountain"]].copy()
    s["fresh"] = fresh[rising].values
    return s.reset_index(drop=True)


def outcome(h, l, o, c, i, entry, atr, X, Y, N, half=False):
    """Return trade P&L in ATR units. i = signal bar; entry given; bars i+1..i+N."""
    tgt, stp = entry + X * atr, entry - Y * atr
    banked = 0.0
    trail = -np.inf
    n = len(c)
    for k in range(i + 1, min(i + N, n - 1) + 1):
        if half and banked == 0.0 and h[k] >= entry + 1.0 * atr:
            banked = 0.5 * 1.0                                    # half the position at +1 ATR
        if l[k] <= stp:
            rest = (stp - entry) / atr
            return banked + (0.5 if half and banked else 1.0) * rest if half else rest
        if not half and h[k] >= tgt:
            return X
        if half and banked:
            trail = max(trail, h[k] - 1.5 * atr)
            if c[k] < trail:
                return banked + 0.5 * (c[k] - entry) / atr
    k = min(i + N, n - 1)
    rest = (c[k] - entry) / atr
    return banked + (0.5 if half and banked else 1.0) * rest if half else rest


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--features", default="features")
    ap.add_argument("--split", default="2024-01-01")
    args = ap.parse_args()
    df = features.load(args.features).sort_values(["ticker", "date"]).reset_index(drop=True)
    df["date"] = pd.to_datetime(df["date"])
    spy = data.fetch("SPY")
    s_ = spy.set_index("date")["close"]
    df["regime_bull"] = df["date"].map(s_ > s_.rolling(200).mean()).fillna(True).astype(bool)
    px = {t: data.fetch(t) for t in df["ticker"].unique()}
    px = {t: p.reset_index(drop=True) for t, p in px.items() if p is not None and len(p) == (df["ticker"] == t).sum()}
    sig = signals(df[df["ticker"].isin(px)])
    print(f"{len(sig)} Dragon C (guarded) signals on {sig['ticker'].nunique()} tickers")
    rows = []
    grid = [(X, Y, N) for X in (0.5, 1.0, 1.5, 2.0, 3.0) for Y in (1.0, 1.5, 2.0, 3.0, 4.0) for N in (5, 10, 20)]
    styles = ["next open", "next open, gap ≤ 0.5 ATR", "pullback limit −0.5 ATR (3 bars)"]
    quals = {"all": lambda s: np.ones(len(s), bool), "fresh": lambda s: s["fresh"].values, "fresh + ≥6 panels": lambda s: s["fresh"].values & (s["dr_n_bull"].values >= 6),
             "ATR ≥ 5 %": lambda s: s["atr_pct"].values >= 5, "fresh + ≥6 panels + ATR ≥ 5 %": lambda s: s["fresh"].values & (s["dr_n_bull"].values >= 6) & (s["atr_pct"].values >= 5)}
    # pre-index bars
    idx = {}
    for t, g in sig.groupby("ticker"):
        p = px[t]
        pos = np.searchsorted(p["date"].values, g["date"].values)
        idx[t] = (p["high"].values, p["low"].values, p["open"].values, p["close"].values, pos, g.index.values)
    for style in styles:
        for qname, qf in quals.items():
            mask = qf(sig)
            for X, Y, N in grid + [("half@1", 1.5, 20)]:
                pnl, dates = [], []
                for t, (h, l, o, c, pos, ids) in idx.items():
                    for i, j in zip(pos, ids):
                        if not mask[j] or i + 2 >= len(c):
                            continue
                        atr = c[i] * sig.at[j, "atr_pct"] / 100.0
                        if atr <= 0:
                            continue
                        if style == "next open":
                            entry, start = o[i + 1], i
                        elif style == "next open, gap ≤ 0.5 ATR":
                            if o[i + 1] - c[i] > 0.5 * atr:
                                continue
                            entry, start = o[i + 1], i
                        else:
                            lim = c[i] - 0.5 * atr
                            start = None
                            for k in range(i + 1, min(i + 4, len(c))):
                                if l[k] <= lim:
                                    entry, start = min(lim, o[k]), k
                                    break
                            if start is None:
                                continue
                        if X == "half@1":
                            r = outcome(h, l, o, c, start, entry, atr, 1.0, Y, N, half=True)
                        else:
                            r = outcome(h, l, o, c, start, entry, atr, X, Y, N)
                        pnl.append(r)
                        dates.append(sig.at[j, "date"])
                if len(pnl) < 30:
                    continue
                pnl = np.array(pnl)
                dates = pd.to_datetime(pd.Series(dates))
                oos = pnl[(dates >= args.split).values]
                rows.append({"entry": style, "quality": qname, "target_atr": X, "stop_atr": Y, "max_bars": N, "trades": len(pnl),
                             "win": round(float((pnl > 0).mean()), 3), "exp_atr": round(float(pnl.mean()), 3), "exp_pct_approx": round(float((pnl * sig["atr_pct"].mean()).mean()), 2),
                             "oos_trades": int(len(oos)), "oos_win": round(float((oos > 0).mean()), 3) if len(oos) else None, "oos_exp_atr": round(float(oos.mean()), 3) if len(oos) else None})
    res = pd.DataFrame(rows)
    res.to_csv(HERE / "frontier.csv", index=False)
    good = res[(res["win"] >= 0.8) & (res["exp_atr"] > 0)].sort_values(["exp_atr"], ascending=False)
    out = [f"# Hit-rate frontier — Dragon C (guarded) signals, {len(sig)} signals, {sig['ticker'].nunique()} tickers", "",
           "P&L in ATR units of the signal bar (1 ATR ≈ the stock's daily range; ~5 % on these names). win = share of trades that ended > 0. "
           "exp = average trade in ATR. Grid = target × stop × max bars × entry style × setup quality.", "",
           f"## Designs with ≥ 80 % winners AND positive expectancy ({len(good)} of {len(res)})", "",
           "| entry | quality | target | stop | max bars | trades | win | exp (ATR) | OOS trades | OOS win | OOS exp |", "|---|---|---|---|---|---|---|---|---|---|---|"]
    for r in good.head(40).itertuples(index=False):
        out.append(f"| {r.entry} | {r.quality} | {r.target_atr} | {r.stop_atr} | {r.max_bars} | {r.trades} | {r.win} | {r.exp_atr} | {r.oos_trades} | {r.oos_win} | {r.oos_exp_atr} |")
    out += ["", "## Best expectancy at each win-rate band (all designs)", "", "| win band | entry | quality | target | stop | max bars | trades | win | exp (ATR) | OOS win | OOS exp |", "|---|---|---|---|---|---|---|---|---|---|---|"]
    for lo, hi in ((0.9, 1.01), (0.8, 0.9), (0.7, 0.8), (0.6, 0.7), (0.5, 0.6), (0.0, 0.5)):
        band = res[(res["win"] >= lo) & (res["win"] < hi)].sort_values("exp_atr", ascending=False).head(3)
        for r in band.itertuples(index=False):
            out.append(f"| {int(lo*100)}–{int(min(hi,1)*100)} % | {r.entry} | {r.quality} | {r.target_atr} | {r.stop_atr} | {r.max_bars} | {r.trades} | {r.win} | {r.exp_atr} | {r.oos_win} | {r.oos_exp_atr} |")
    (HERE / "frontier.md").write_text("\n".join(out), encoding="utf-8")
    print("\n".join(out))


if __name__ == "__main__":
    main()
