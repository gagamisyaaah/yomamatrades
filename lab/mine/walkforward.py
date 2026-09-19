"""Walk-forward / holdout test of the refined Dragon readings (and Diamond strict) on a feature table.

usage: python3.12 -m mine.walkforward [--features features_broad] [--holdout] [--split 2024-01-01]
  --holdout  drop the ~95 tickers the readings were tuned on (mine.data.CORE) → a true out-of-universe test
Writes mine/walkforward.md + walkforward.csv: per rule, per signal-year (2017…2026) trades / avg % / PF / win,
the 2024+ summary, per-ticker consistency, and the random-entry control. Nothing here is fitted: the rules are
the ones chosen on the 94-ticker set, evaluated as-is.
"""
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd

from . import data, evaluate as ev, features

HERE = Path(__file__).parent


def refined_rules(d: pd.DataFrame) -> dict[str, tuple[pd.Series, pd.Series, bool]]:
    fresh = (d["dr_hole_state"] == 1) & (d.groupby("ticker")["dr_hole_state"].shift(3) != 1)
    base = (d["dr_hole_state"] == 1) & (d["dr_whales"] >= 50) & d["dr_te_bull"]
    hole_dn = d["dr_hole_state"] == -1
    vet = ~d["di_overheated"] & (d["di_mountain"] >= 3) & d["di_above_gold"]
    r = {}
    r["ref: hole up + whales≥50 + TE up, exit TE turn or hole down"] = (base, hole_dn | ~d["dr_te_bull"], False)
    r["A: exit hole broke down only"] = (base, hole_dn, False)
    r["C: A + Diamond vetoes (bubble, Mountain≥3, above gold)"] = (base & vet, hole_dn, False)
    r["D: C + fresh break (≤3 bars)"] = (base & vet & fresh, hole_dn, False)
    r["E: D + ≥6 of 7 panels"] = (base & vet & fresh & (d["dr_n_bull"] >= 6), hole_dn, False)
    r["F: C + ATR ≥ 5 % names"] = (base & vet & (d["atr_pct"] >= 5), hole_dn, False)
    if "trend200" in d:
        r["C + stock above its own 200-day SMA"] = (base & vet & d["trend200"], hole_dn, False)
        r["C + SPY above its 200-day line"] = (base & vet & d["regime_bull"], hole_dn, False)
        r["C + both regime filters"] = (base & vet & d["trend200"] & d["regime_bull"], hole_dn, False)
        r["C + SPY above 200-day, exit also when SPY drops below it"] = (base & vet & d["regime_bull"], hole_dn | ~d["regime_bull"], False)
    r["Diamond strict, flip exit"] = ((d["di_n_blue_c"] == 3) & (d["di_mountain"] == 4) & d["di_above_gold"] & ~d["di_overheated"], (d["di_n_pink_c"] >= 2) | (d["di_mountain"] <= 2) | d["di_below_gold"], False)
    rng = np.random.RandomState(7)
    r["CONTROL: random days, ~25-bar hold"] = (pd.Series(rng.rand(len(d)) < 0.1, index=d.index), pd.Series(np.arange(len(d)) % 25 == 0, index=d.index), False)
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--features", default="features_broad")
    ap.add_argument("--holdout", action="store_true")
    ap.add_argument("--split", default="2024-01-01")
    args = ap.parse_args()
    df = features.load(args.features).sort_values(["ticker", "date"]).reset_index(drop=True)
    df["date"] = pd.to_datetime(df["date"])
    tuned = set(data.CORE)
    label = "all tickers"
    if args.holdout:
        df = df[~df["ticker"].isin(tuned)].reset_index(drop=True)
        label = f"HOLDOUT: {df['ticker'].nunique()} tickers never used to choose the rules"
    spy = data.fetch("SPY")
    if spy is not None:
        s_ = spy.set_index("date")["close"]
        df["regime_bull"] = df["date"].map(s_ > s_.rolling(200).mean()).fillna(True).astype(bool)
    px = {t: data.fetch(t) for t in df["ticker"].unique()}
    px = {t: p.reset_index(drop=True) for t, p in px.items() if p is not None and len(p) == (df["ticker"] == t).sum()}
    df = df[df["ticker"].isin(px)].reset_index(drop=True)
    tr200 = []
    for t, g in df.groupby("ticker", sort=False):
        c = px[t]["close"]
        tr200.append(pd.Series((c > c.rolling(200).mean()).values, index=g.index))
    df["trend200"] = pd.concat(tr200).sort_index().fillna(False).astype(bool)
    years = list(range(int(df["date"].dt.year.min()) + 1, int(df["date"].dt.year.max()) + 1))
    wk_oos = max(1.0, (df["date"].max() - pd.Timestamp(args.split)).days / 7.0)
    out = [f"# Walk-forward on `{args.features}` — {label}, {len(df):,} ticker-days, {df['date'].min().date()} → {df['date'].max().date()}", "",
           "Entries next open, one unit per trade, exit on the rule's own flip. Rules fixed in advance (chosen on the 94-ticker set).", ""]
    rows = []
    for name, (e, x, short) in refined_rules(df).items():
        tr = ev.simulate(df, px, e.fillna(False), x.fillna(False), short)
        if len(tr) == 0:
            out.append(f"## {name}\n\nno trades\n")
            continue
        tr["date"] = pd.to_datetime(tr["date"])
        te = tr[tr["date"] >= args.split]
        s = ev.stats(te, "flip", wk_oos)
        out += [f"## {name}", "", f"**2024+ ({args.split} on):** {s['n']} trades · avg **{s['avg']} %** · median {s['med']} % · win {s['win']} · PF {s['pf']} · {s['per_wk']} signals/week · {int(round(s['tick_pos']*100))} % of tickers positive · bear-regime {s['bear_n']} trades avg {s['bear_avg']} %", "",
                "| signal year | trades | avg % | median % | win | PF | tickers+ |", "|---|---|---|---|---|---|---|"]
        for y in years:
            g = tr[tr["date"].dt.year == y]
            if len(g) < 5:
                continue
            sy = ev.stats(g, "flip", 52.0)
            out.append(f"| {y} | {sy['n']} | {sy['avg']} | {sy['med']} | {sy['win']} | {sy['pf']} | {sy['tick_pos']} |")
            rows.append({"rule": name, "year": y, **sy})
        rows.append({"rule": name, "year": "2024+", **s})
        out.append("")
    (HERE / ("walkforward_holdout.md" if args.holdout else "walkforward.md")).write_text("\n".join(out), encoding="utf-8")
    pd.DataFrame(rows).to_csv(HERE / ("walkforward_holdout.csv" if args.holdout else "walkforward.csv"), index=False)
    print("\n".join(out))


if __name__ == "__main__":
    main()
