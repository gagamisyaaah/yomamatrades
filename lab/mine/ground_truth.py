"""Ground truth: do our total-signal readings fire where the founders themselves called it?

usage: python3.12 -m mine.ground_truth [--claims ../x_claims.csv] [--features features_broad]
Input: x_claims.csv from mine_x.py (TypeSafe-typed posts: account, date, tickers, direction bullish/bearish/neutral,
signal event, conviction). For every post with a ticker we have bars for, look up our states on that date (or the
last trading day before it) and report: agreement between their direction and our Dragon/Diamond states, how often
our readings were ON, and what happened next (10-bar forward return) for their calls vs. our concurrent signals.
Writes mine/ground_truth.md.
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

import pandas as pd

from . import features

HERE = Path(__file__).parent


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--claims", default=str(HERE.parent / "x_claims.csv"))
    ap.add_argument("--features", default="features_broad")
    args = ap.parse_args()
    cl = pd.read_csv(args.claims)
    cl["date"] = pd.to_datetime(cl["date"], errors="coerce", utc=True).dt.tz_localize(None).dt.normalize()
    cl = cl.dropna(subset=["date"])
    cl["tick_list"] = cl["tickers"].fillna("").apply(lambda s: [t.strip().upper().lstrip("$") for t in re.split(r"[,\s]+", str(s)) if t.strip()])
    cl = cl.explode("tick_list").rename(columns={"tick_list": "ticker"})
    cl = cl[cl["ticker"].notna() & (cl["ticker"] != "")]
    f = features.load(args.features)
    f["date"] = pd.to_datetime(f["date"])
    f = f.sort_values(["ticker", "date"])
    have = set(f["ticker"].unique())
    cl = cl[cl["ticker"].isin(have)].copy()
    if cl.empty:
        (HERE / "ground_truth.md").write_text("# Ground truth\n\nNo founder posts with a ticker we have bars for.\n")
        print("no overlap between posts and our universe")
        return
    rows = []
    for r in cl.itertuples(index=False):
        g = f[(f["ticker"] == r.ticker) & (f["date"] <= r.date)]
        if g.empty:
            continue
        s = g.iloc[-1]
        drag_c = bool(s["dr_hole_state"] == 1 and s["dr_whales"] >= 50 and s["dr_te_bull"] and not s["di_overheated"] and s["di_mountain"] >= 3 and s["di_above_gold"])
        dia = bool(s["di_n_blue_c"] == 3 and s["di_mountain"] == 4 and s["di_above_gold"] and not s["di_overheated"])
        ours = "bullish" if (drag_c or dia or s["dr_n_bull"] >= 5 or s["di_n_blue_c"] >= 2) else ("bearish" if (s["dr_n_bear"] >= 5 or s["di_n_pink_c"] >= 2 or s["dr_hole_state"] == -1) else "neutral")
        rows.append({"account": r.account, "post_date": r.date.date(), "ticker": r.ticker, "their_call": r.direction, "event": r.signal, "conviction": r.conviction,
                     "our_state_date": s["date"].date(), "dragon_C": drag_c, "diamond_strict": dia, "panels": int(s["dr_n_bull"]), "hole": int(s["dr_hole_state"]), "whales": round(float(s["dr_whales"]) if pd.notna(s["dr_whales"]) else 0),
                     "blue_c": int(s["di_n_blue_c"]), "mountain": int(s["di_mountain"]), "ours": ours, "agree": (ours == r.direction) if r.direction in ("bullish", "bearish") else None,
                     "fwd10": round(float(s["fwd_10"]), 2) if pd.notna(s["fwd_10"]) else None, "url": r.url})
    t = pd.DataFrame(rows)
    calls = t[t["their_call"].isin(["bullish", "bearish"])]
    out = [f"# Ground truth — {len(t)} founder posts matched to our bars ({t['account'].nunique()} accounts, {t['ticker'].nunique()} tickers)", ""]
    if len(calls):
        out.append(f"Directional calls: {len(calls)} · we agreed on {calls['agree'].mean():.0%} · Dragon C was ON at {calls[calls['their_call']=='bullish']['dragon_C'].mean():.0%} of their bullish calls · Diamond strict ON at {calls[calls['their_call']=='bullish']['diamond_strict'].mean():.0%}")
        out.append(f"Their bullish calls: 10-bar forward return avg {calls[calls['their_call']=='bullish']['fwd10'].mean():.2f} % · their bearish calls: {calls[calls['their_call']=='bearish']['fwd10'].mean():.2f} %")
        out.append("")
        out.append("| account | post | ticker | their call | event | ours | Dragon C | Diamond | panels | hole | whales | Mountain | fwd10 % | link |")
        out.append("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
        for r in calls.sort_values("post_date", ascending=False).itertuples(index=False):
            out.append(f"| {r.account} | {r.post_date} | {r.ticker} | {r.their_call} | {r.event} | {r.ours} | {'ON' if r.dragon_C else ''} | {'ON' if r.diamond_strict else ''} | {r.panels} | {r.hole} | {r.whales} | {r.mountain} | {r.fwd10} | [x]({r.url}) |")
    else:
        out.append("No directional calls with tickers found in the posts.")
    (HERE / "ground_truth.md").write_text("\n".join(out), encoding="utf-8")
    t.to_csv(HERE / "ground_truth.csv", index=False)
    print("\n".join(out[:12]))


if __name__ == "__main__":
    main()
