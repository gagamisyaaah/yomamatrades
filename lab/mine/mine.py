"""Mine the feature table: where were the big moves, and what were Dragon and Diamond showing right before?

usage: python3.12 -m mine.mine [--target big_up|big_dn] [--min-support 300]
writes mine/report.md (lift tables, learned rules, per-rule trade stats) and mine/rules.json.

Method (transparent, then a tree on top):
  1. base rate of the target (e.g. "≥ +10 % within 10 bars before −5 %") over every ticker-day;
  2. lift of every panel state: P(target | state) / base — the "what did the indicators do at the profit points";
  3. a shallow decision tree (depth ≤ 4, big leaves) over the same states → conjunction rules; each rule is scored
     out-of-sample (train ≤ 2023, test 2024+) with precision, lift, support, trades/week across the universe,
     mean next-open→10-bar return, and the same in bear-regime days only (anti-bias);
  4. exits: the same tables at "peak points" (close is the 21-bar max and drops ≥ 8 % within 10 bars).
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
import pandas as pd

from . import features

HERE = Path(__file__).parent

BOOL_FEATS = ["dr_ss_bull", "dr_ss_fresh", "dr_ss_confirm", "dr_ss_confirm_s", "dr_ribbon_bull", "dr_poc_bull", "dr_poc_bear", "dr_te_bull",
              "dr_macd_bull", "dr_rsi_up", "dr_rsi_dn", "dr_retail_shrinking", "di_above_gold", "di_below_gold", "di_overheated"]
CAT_FEATS = ["dr_hole_state", "dr_n_bull", "dr_n_bear", "di_st_echo", "di_st_tango", "di_st_bravo", "di_c_echo", "di_c_tango", "di_c_bravo",
             "di_n_blue", "di_n_pink", "di_n_blue_c", "di_n_pink_c", "di_mountain", "di_echo", "di_tango", "di_bravo"]
NUM_FEATS = ["dr_j", "dr_whales", "dr_retail", "dr_poc_ratio", "dr_te_slope", "dr_macd_dif", "dr_rsi14", "di_hype", "di_flow", "di_gold_dist",
             "di_purple_dist", "di_green_dist", "di_bubble", "di_silver_pos"]


def lift_table(df: pd.DataFrame, target: str, min_support: int) -> pd.DataFrame:
    base = df[target].mean()
    tb = df.groupby("ticker")[target].transform("mean")           # expected hit rate given the ticker alone
    df = df.assign(_exp=tb)
    rows = []
    for f in BOOL_FEATS + CAT_FEATS:
        if f not in df:
            continue
        for val, g in df.groupby(f):
            if len(g) < min_support:
                continue
            rows.append({"feature": f, "value": val, "n": len(g), "p": g[target].mean(), "lift": g[target].mean() / g["_exp"].mean() if g["_exp"].mean() else np.nan,
                         "fwd10": g["fwd10_atr"].mean(), "fwd5": g["fwd5_atr"].mean(), "bear_fwd10": g.loc[~g["regime_bull"], "fwd10_atr"].mean()})
    for f in NUM_FEATS:
        if f not in df:
            continue
        q = pd.qcut(df[f], 5, duplicates="drop")
        for val, g in df.groupby(q, observed=True):
            if len(g) < min_support:
                continue
            rows.append({"feature": f, "value": str(val), "n": len(g), "p": g[target].mean(), "lift": g[target].mean() / g["_exp"].mean() if g["_exp"].mean() else np.nan,
                         "fwd10": g["fwd10_atr"].mean(), "fwd5": g["fwd5_atr"].mean(), "bear_fwd10": g.loc[~g["regime_bull"], "fwd10_atr"].mean()})
    t = pd.DataFrame(rows)
    return t.sort_values("lift", ascending=False)


def tree_rules(df: pd.DataFrame, target: str, feats: list[str], depth: int = 4, min_leaf: int = 250):
    from sklearn.tree import DecisionTreeClassifier, _tree
    X = df[feats].astype(float).fillna(0.0).values
    y = df[target].astype(int).values
    clf = DecisionTreeClassifier(max_depth=depth, min_samples_leaf=min_leaf, class_weight="balanced", random_state=0).fit(X, y)
    t = clf.tree_
    rules = []

    def walk(node, conds):
        if t.feature[node] == _tree.TREE_UNDEFINED:
            n = int(t.n_node_samples[node])
            rules.append({"conds": conds, "n_train": n})
            return
        f = feats[t.feature[node]]
        thr = float(t.threshold[node])
        walk(t.children_left[node], conds + [(f, "<=", round(thr, 3))])
        walk(t.children_right[node], conds + [(f, ">", round(thr, 3))])
    walk(0, [])
    return rules


def apply_rule(df: pd.DataFrame, conds) -> pd.Series:
    m = pd.Series(True, index=df.index)
    for f, op, thr in conds:
        v = df[f].astype(float).fillna(0.0)
        m &= (v <= thr) if op == "<=" else (v > thr)
    return m


def score_rule(df: pd.DataFrame, mask: pd.Series, target: str) -> dict:
    g = df[mask]
    if len(g) == 0:
        return {"n": 0}
    weeks = max(1.0, (df["date"].max() - df["date"].min()).days / 7.0)
    bear = g[~g["regime_bull"]]
    return {"n": int(len(g)), "precision": round(float(g[target].mean()), 3), "lift": round(float(g[target].mean() / df[target].mean()), 2) if df[target].mean() else None,
            "fwd5": round(float(g["fwd5_atr"].mean()), 2), "fwd10": round(float(g["fwd10_atr"].mean()), 2), "fwd10_median": round(float(g["fwd10_atr"].median()), 2),
            "win10": round(float((g["fwd_10"] > 0).mean()), 3), "mfe10": round(float(g["mfe_10"].mean()), 2), "mae10": round(float(g["mae_10"].mean()), 2),
            "signals_per_week": round(len(g) / weeks, 2), "tickers": int(g["ticker"].nunique()),
            "bear_n": int(len(bear)), "bear_fwd10": round(float(bear["fwd10_atr"].mean()), 2) if len(bear) else None,
            "excess10": round(float(g["fwd10_atr"].mean() - df["fwd10_atr"].mean()), 2)}


def fmt_conds(conds) -> str:
    return " AND ".join(f"{f} {op} {thr}" for f, op, thr in conds) or "(all)"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--min-support", type=int, default=300)
    ap.add_argument("--split", default="2024-01-01", help="train before, test from this date")
    args = ap.parse_args()
    df = features.load()
    df = df.dropna(subset=["fwd_10"]).reset_index(drop=True)
    from . import data as _data
    spy = _data.fetch("SPY")
    if spy is not None:
        s_ = spy.set_index("date")["close"]
        df["regime_bull"] = pd.to_datetime(df["date"]).map(s_ > s_.rolling(200).mean()).fillna(True).astype(bool)
    # volatility-normalised outcomes: everything in units of the stock's own ATR (as % of price)
    a = df["atr_pct"].replace(0, np.nan)
    df["fwd10_atr"] = df["fwd_10"] / a
    df["fwd5_atr"] = df["fwd_5"] / a
    df["big_up"] = (df["mfe_10"] >= 3.0 * a) & (df["mae_10"] > -1.5 * a)     # +3 ATR reached, never −1.5 ATR
    df["big_dn"] = (df["mae_10"] <= -3.0 * a) & (df["mfe_10"] < 1.5 * a)
    in_long = (df["dr_n_bull"] >= 4) | (df["di_n_blue_c"] >= 2)               # days a Dragon/Diamond long would be open
    df["peak"] = in_long & (df["mae_10"] <= -3.0 * a) & (df["mfe_10"] < 1.0 * a)   # the exit question: a −3 ATR drop starts here
    train = df[df["date"] < args.split]
    test = df[df["date"] >= args.split]
    out = [f"# Mining report — {df['ticker'].nunique()} tickers, {len(df):,} ticker-days ({df['date'].min().date()} → {df['date'].max().date()})", ""]
    out.append(f"Train < {args.split}: {len(train):,} rows · Test ≥ {args.split}: {len(test):,} rows · bear-regime share {(~df['regime_bull']).mean():.0%}")
    rules_out = {}
    for target, label in [("big_up", "LONG spike: +3 ATR reached within 10 bars without a −1.5 ATR drawdown (entry next open)"),
                          ("big_dn", "SHORT spike: −3 ATR within 10 bars without a +1.5 ATR rally"),
                          ("peak", "EXIT / top: while a Dragon (≥4/7) or Diamond (≥2 confirmed) long is open, a −3 ATR drop starts here")]:
        base = df[target].mean()
        out += ["", f"## {label}", "", f"Base rate: {base:.1%} of all ticker-days · mean fwd10 {df['fwd10_atr'].mean():.2f} ATR (bull days {df.loc[df['regime_bull'],'fwd10_atr'].mean():.2f}, bear days {df.loc[~df['regime_bull'],'fwd10_atr'].mean():.2f}) · fwd columns below are in ATR units, lift is within-ticker", ""]
        lt = lift_table(df, target, args.min_support)
        out.append("### What the panels showed (lift = P(target | state) / base; top 25 by lift, support ≥ %d)" % args.min_support)
        out.append("")
        out.append("| feature | value | n | P(target) | lift | fwd10 % | fwd5 % | bear fwd10 % |")
        out.append("|---|---|---|---|---|---|---|---|")
        for _, r in lt.head(25).iterrows():
            out.append(f"| {r.feature} | {r.value} | {r.n} | {r.p:.1%} | {r.lift:.2f} | {r.fwd10:.2f} | {r.fwd5:.2f} | {r.bear_fwd10:.2f} |")
        out.append("")
        out.append("### Lowest-lift states (what NOT to see)")
        out.append("")
        out.append("| feature | value | n | P(target) | lift | fwd10 % |")
        out.append("|---|---|---|---|---|---|")
        for _, r in lt.tail(10).iterrows():
            out.append(f"| {r.feature} | {r.value} | {r.n} | {r.p:.1%} | {r.lift:.2f} | {r.fwd10:.2f} |")
        for group, feats in [("Dragon only", [f for f in BOOL_FEATS + CAT_FEATS + NUM_FEATS if f.startswith("dr_")]),
                             ("Diamond only", [f for f in BOOL_FEATS + CAT_FEATS + NUM_FEATS if f.startswith("di_")]),
                             ("Dragon + Diamond", BOOL_FEATS + CAT_FEATS + NUM_FEATS)]:
            feats = [f for f in feats if f in df]
            rules = tree_rules(train, target, feats)
            scored = []
            for r in rules:
                m_tr = apply_rule(train, r["conds"])
                m_te = apply_rule(test, r["conds"])
                s_tr = score_rule(train, m_tr, target)
                s_te = score_rule(test, m_te, target)
                if s_tr["n"] >= args.min_support and s_te.get("n", 0) >= 30:
                    scored.append({"rule": fmt_conds(r["conds"]), "conds": r["conds"], "train": s_tr, "test": s_te})
            scored.sort(key=lambda s: -(s["test"].get("lift") or 0))
            out += ["", f"### Learned rules — {group} (tree depth ≤ 4; ranked by out-of-sample lift)", ""]
            out.append("| rule | train n | train lift | train fwd10 | TEST n | TEST lift | TEST fwd10 | TEST win10 | TEST bear fwd10 | signals/wk (test) | tickers |")
            out.append("|---|---|---|---|---|---|---|---|---|---|---|")
            for s in scored[:8]:
                a, b = s["train"], s["test"]
                out.append(f"| {s['rule']} | {a['n']} | {a['lift']} | {a['fwd10']} | {b['n']} | {b['lift']} | {b['fwd10']} | {b['win10']} | {b['bear_fwd10']} | {b['signals_per_week']} | {b['tickers']} |")
            rules_out[f"{target}:{group}"] = scored[:8]
    (HERE / "report.md").write_text("\n".join(out), encoding="utf-8")
    (HERE / "rules.json").write_text(json.dumps(rules_out, indent=1, default=str), encoding="utf-8")
    print("\n".join(out))
    print(f"\nwrote {HERE / 'report.md'} and rules.json")


if __name__ == "__main__":
    main()
