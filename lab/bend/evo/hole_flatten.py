"""The founder's rule: enter when the Dragon volatility hole breaks up; exit when the TE line, still rising, flattens
(slope falls below a fraction of its peak since entry, or under a floor), with the hole breaking down as the safety exit.
Tested on every ticker the engine has states for (bend/data/D), per market-cap band, against random entries with the
same exit. usage: python3.12 bend/evo/hole_flatten.py [--k 0.3] [--floor 0.02] [--max-bars 60] [--split 2024-01-01]"""
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


def te_slope(df: pd.DataFrame) -> np.ndarray:
    ohlc4 = (df.open + df.high + df.low + df.close) / 4
    trip = (ta.ema(ohlc4, 21) + ta.ema(ohlc4, 34) + ta.ema(ohlc4, 68)) / 3
    x = np.arange(6); xm = x.mean(); den = ((x - xm) ** 2).sum()
    slope = np.convolve(trip.values, (x - xm)[::-1], mode="valid") / den
    v7 = pd.Series(np.concatenate([np.full(5, np.nan), slope * (5 - xm)]), index=df.index) + trip.rolling(6).mean()
    return ((v7 - v7.shift(1)) / ta.atr(df, 14)).values


def simulate(o, h, l, c, hole_up, hole_dn, slope, entries, k, floor, max_bars):
    n = len(c); rows = []
    for i in entries:
        if i + 2 >= n:
            continue
        ent = o[i + 1]; peak = 0.0; j = i + 1; reason = "time"
        while j < n - 1 and j - i <= max_bars:
            s = slope[j]
            if not np.isnan(s):
                peak = max(peak, s)
                if peak > 0 and (s < k * peak or s < floor):
                    reason = "flatten"; break
            if hole_dn[j]:
                reason = "hole down"; break
            j += 1
        j = min(j, n - 1)
        rows.append({"bar": i, "exit_bar": j, "ret": (c[j] / ent - 1) * 100, "bars": j - i, "reason": reason, "mfe": (h[i + 1:j + 1].max() / ent - 1) * 100})
    return rows


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--k", type=float, default=0.3); ap.add_argument("--floor", type=float, default=0.02); ap.add_argument("--max-bars", type=int, default=60); ap.add_argument("--split", default="2024-01-01"); ap.add_argument("--dir", default="bend/data/D")
    a = ap.parse_args(); d = ROOT / a.dir
    order = [ln.split()[0] for ln in (d / "list.txt").read_text().split("\n") if ln.strip()]
    w = np.frombuffer((d / "states.bin").read_bytes(), dtype="<u4"); pos = 0; states = {}
    for t in order:
        n = int(w[pos]); states[t] = w[pos + 1:pos + 1 + 2 * n:2].astype(np.int64); pos += 1 + 2 * n
        m = int(w[pos]); pos += 1 + 16 * m
    caps = pd.read_csv(ROOT / "mine/caps.csv").set_index("symbol")["cap"]
    rng = np.random.RandomState(7); rows = []
    for t, f in states.items():
        try:
            df = pd.read_csv(ROOT / f"mine/cache/{t}.csv", parse_dates=["date"])
        except Exception:
            continue
        if len(df) != len(f) or len(df) < 300:
            continue
        hole_up = (f & 1) > 0; hole_dn = (f & 2) > 0; slope = te_slope(df)
        o, h, l, c = df.open.values, df.high.values, df.low.values, df.close.values
        rising = np.flatnonzero(hole_up & ~np.roll(hole_up, 1)); rising = rising[rising > 0]
        rand = np.flatnonzero(rng.rand(len(df)) < 0.03); rand = rand[rand > 250]
        for kind, ent in (("hole up", rising), ("random", rand)):
            for r in simulate(o, h, l, c, hole_up, hole_dn, slope, ent, a.k, a.floor, a.max_bars):
                rows.append({"ticker": t, "kind": kind, "date": df.date.iloc[r["bar"]], "cap": caps.get(t, np.nan), **r})
    T = pd.DataFrame(rows); T["band"] = pd.cut(T.cap, [0, 5e8, 2e9, 5e9, 1e15], labels=["100M-500M", "500M-2B", "2B-5B", ">5B"]); T["year"] = T.date.dt.year
    T.to_csv(ROOT / "bend/data/hole_flatten.csv", index=False)
    pd.set_option("display.width", 250)

    def st(g):
        r = g.ret
        return pd.Series({"trades": len(g), "avg%": r.mean(), "median%": r.median(), "win": (r > 0).mean(), "≥+20%": (r >= 20).mean(), "≥+50%": (r >= 50).mean(), "≥+200%": (r >= 200).mean(), "bars": g.bars.mean(), "mfe%": g.mfe.mean(), "PF": r[r > 0].sum() / max(1e-9, -r[r <= 0].sum())})
    for per, sel in (("2016-23", T.date < a.split), ("2024+ holdout", T.date >= a.split)):
        print(f"\n===== {per}: entry = hole broke up (next open) · exit = TE slope flattens (k={a.k}, floor={a.floor} ATR/bar) or hole breaks down · random entries same exit =====")
        print(T[sel].groupby(["band", "kind"], observed=True).apply(st).round(3).to_string())
    print("\n===== exit reasons (hole-up entries) =====")
    print(T[T.kind == "hole up"].groupby("reason").ret.agg(["size", "mean", "median"]).round(2).to_string())
    print("\n===== per year, hole-up entries vs random (avg %) =====")
    print(T.groupby(["year", "kind"]).ret.mean().unstack().round(2).to_string())
    md = lambda dd: "| " + " | ".join(map(str, dd.columns)) + " |\n|" + "---|" * len(dd.columns) + "\n" + "\n".join("| " + " | ".join(str(v) for v in r) + " |" for r in dd.itertuples(index=False))  # noqa: E731
    out = [f"# Hole-up entry, TE-flatten exit (k={a.k}, floor={a.floor}) — every ticker with engine states, per cap band\n"]
    for per, sel in (("2016-23", T.date < a.split), ("2024+ holdout", T.date >= a.split)):
        out.append(f"## {per}\n\n" + md(T[sel].groupby(["band", "kind"], observed=True).apply(st).round(3).reset_index()))
    (ROOT / "bend/data/hole_flatten.md").write_text("\n\n".join(out) + "\n")


if __name__ == "__main__":
    main()
