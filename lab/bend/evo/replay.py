"""Replay finalist rules on EVERY bar of every ticker (not just the labelled random bars): rising edges, entry next open,
+2/−4 ATR bracket (20 bars), 10-bar close, and the random control with the same exits from the matrix. This is the proof
table: trades, signals per week, win rate, avg in ATR and %, per year, in-sample vs holdout.
usage: python3.12 bend/evo/replay.py --rules "rsi2≤p10(9.215) & hi252≥p75(90.1)" ... [--tag name]"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

import numpy as np
import pandas as pd

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT))
from mine import ta  # noqa: E402
from mine.frontier import outcome  # noqa: E402
sys.path.insert(0, str(HERE))
from blocks import blocks  # noqa: E402
from geometry import geometry  # noqa: E402

COND = re.compile(r"^(?P<b>[a-z0-9_]+)(?P<op>≤|≥|=)(?:p\d+\()?(?P<thr>-?[0-9.e+-]+)\)?$")


def parse(rule: str):
    out = []
    for c in rule.split(" & "):
        m = COND.match(c.strip())
        if not m:
            raise ValueError(c)
        out.append((m["b"], m["op"], float(m["thr"])))
    return out


def apply(B: pd.DataFrame, conds) -> np.ndarray:
    m = np.ones(len(B), bool)
    for b, op, thr in conds:
        x = B[b].values
        m &= (x <= thr) if op == "≤" else ((x >= thr) if op == "≥" else (x == thr))
        m &= ~np.isnan(x)
    return m


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--rules", nargs="+", required=True); ap.add_argument("--tag", default="replay"); ap.add_argument("--split", default="2024-01-01")
    a = ap.parse_args()
    rules = {r: parse(r) for r in a.rules}
    flags = pd.read_pickle(ROOT / "bend/data/control_D.pkl")[["ticker", "bar", "hole_up", "hole_dn", "whales50", "te_up", "mtn3", "gold", "hot", "ss", "ribbon", "poc", "macdrsi", "fresh", "panels6", "whales70", "whales_lt30", "hole_inforce"]]
    need_flags = any(b in flags.columns for conds in rules.values() for b, _, _ in conds)
    rows = []
    tickers = sorted(p.stem for p in (ROOT / "mine/cache").glob("*.csv"))
    for i, t in enumerate(tickers):
        try:
            df = pd.read_csv(ROOT / f"mine/cache/{t}.csv", parse_dates=["date"])
        except Exception:
            continue
        if len(df) < 300:
            continue
        B = pd.concat([blocks(df), geometry(df)], axis=1)
        if need_flags:
            continue   # Dragon-flag rules need the engine's per-bar states; replay only the chart-computable finalists here
        o, h, l, c = df.open.values, df.high.values, df.low.values, df.close.values; n = len(c)
        atrp = (ta.atr(df, 14) / df.close * 100).values
        for name, conds in rules.items():
            m = apply(B, conds); rising = m & ~np.roll(m, 1); rising[0] = False
            for j in np.where(rising)[0]:
                if j + 2 >= n:
                    continue
                ent = o[j + 1]; atr = ent * atrp[j] / 100.0
                if not atr > 0:
                    continue
                rows.append({"rule": name, "ticker": t, "date": df.date.iloc[j], "atr_pct": atrp[j], "r24": outcome(h, l, o, c, j, ent, atr, 2, 4, 20), "t10": (c[min(j + 10, n - 1)] / ent - 1) * 100, "r24_pct": None})
        if i % 300 == 0:
            print(i, len(rows), flush=True)
    T = pd.DataFrame(rows); T["r24_pct"] = T.r24 * T.atr_pct
    T.to_csv(ROOT / f"bend/data/{a.tag}.csv", index=False)
    M = pd.read_pickle(ROOT / "bend/data/matrix_D.pkl")[["date", "year", "band", "atr_pct", "r24", "t10"]]
    M["r24_pct"] = M.r24 * M.atr_pct
    T["year"] = T.date.dt.year; T["band"] = pd.cut(T.atr_pct, [0, 3, 5, 8, 12, 1000], labels=["<3", "3-5", "5-8", "8-12", "≥12"])
    weeks_in = (pd.Timestamp(a.split) - pd.Timestamp("2016-01-01")).days / 7; weeks_oos = (T.date.max() - pd.Timestamp(a.split)).days / 7
    out = []
    for name, g in T.groupby("rule"):
        for per, gg, wk in [("2016-23", g[g.date < a.split], weeks_in), ("2024+ holdout", g[g.date >= a.split], weeks_oos)]:
            if not len(gg):
                continue
            mm = M[(M.date < a.split) if per.startswith("2016") else (M.date >= a.split)]
            # random control matched by year × band, weighted by the rule's cell counts
            cm = mm.groupby(["year", "band"], observed=True)[["r24", "t10", "r24_pct"]].mean()
            keys = list(zip(gg.year, gg.band)); ctrl = cm.reindex(keys)
            out.append({"rule": name, "period": per, "trades": len(gg), "per_week": round(len(gg) / wk, 1), "win": round((gg.r24 > 0).mean(), 3), "avg_ATR": round(gg.r24.mean(), 3), "avg_%": round(gg.r24_pct.mean(), 2),
                        "random_ATR": round(float(ctrl.r24.mean()), 3), "random_%": round(float(ctrl.r24_pct.mean()), 2), "excess_ATR": round(gg.r24.mean() - float(ctrl.r24.mean()), 3), "t10_%": round(gg.t10.mean(), 2), "random_t10_%": round(float(ctrl.t10.mean()), 2),
                        "PF": round(gg.r24[gg.r24 > 0].sum() / max(1e-9, -gg.r24[gg.r24 <= 0].sum()), 2)})
    O = pd.DataFrame(out); pd.set_option("display.width", 250); print(O.to_string(index=False))
    yr = T.groupby(["rule", "year"]).agg(trades=("r24", "size"), win=("r24", lambda s: round((s > 0).mean(), 3)), avg_ATR=("r24", "mean"), avg_pct=("r24_pct", "mean")).round(3)
    print(yr.to_string())
    md = lambda d: "| " + " | ".join(map(str, d.columns)) + " |\n|" + "---|" * len(d.columns) + "\n" + "\n".join("| " + " | ".join(str(v) for v in r) + " |" for r in d.itertuples(index=False))  # noqa: E731
    (ROOT / f"bend/data/{a.tag}.md").write_text(f"# Replay on every bar of {len(tickers)} tickers\n\n" + md(O) + "\n\n## per year\n\n" + md(yr.reset_index()) + "\n")


if __name__ == "__main__":
    main()
