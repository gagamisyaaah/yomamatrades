"""Nightly candidate list: run the Dragon / Diamond total readings on fresh daily bars for every ticker in the cache
(or a list), rank the live setups, and write the pre-market sheet.

usage: python3.12 -m mine.scan [--tickers ...] [--refresh] [--top 40]
  --refresh   re-download bars older than 12 h (Nasdaq history API; ~1 s per ticker)
Output: mine/candidates_<date>.md (+ .csv): ticker, tier (C/D/E/F or armed), bars since the hole broke up, panels,
whales %, Mountain, ATR %, gold distance, plus a TypeSafe 0–1 "setup quality" score when TYPESAFE_API_KEY is set.
"""
from __future__ import annotations

import argparse
import os
import sys
import time
from datetime import date
from pathlib import Path

import pandas as pd

from . import data, diamond, dragon, ta

HERE = Path(__file__).parent


def read(ticker: str, refresh: bool) -> dict | None:
    df = data.fetch(ticker, force=refresh)
    if df is None or len(df) < 260:
        return None
    df = df.reset_index(drop=True)
    dr = dragon.states(df)
    di = diamond.states(df)
    i = len(df) - 1
    atr_pct = float(ta.atr(df, 14).iloc[i] / df["close"].iloc[i] * 100.0)
    hole = int(dr["hole_state"].iloc[i])
    since_break = None
    if hole == 1:
        k = i
        while k > 0 and dr["hole_state"].iloc[k - 1] == 1:
            k -= 1
        since_break = i - k
    whales = float(dr["whales"].iloc[i]) if pd.notna(dr["whales"].iloc[i]) else 0.0
    mountain = int(di["mountain"].iloc[i])
    above_gold = bool(di["above_gold"].iloc[i])
    hot = bool(di["overheated"].iloc[i])
    n_bull = int(dr["n_bull"].iloc[i])
    te_up = bool(dr["te_bull"].iloc[i])
    c_ok = hole == 1 and whales >= 50 and te_up and not hot and mountain >= 3 and above_gold
    tier = ""
    if c_ok:
        tier = "C"
        if since_break is not None and since_break <= 3:
            tier = "D"
            if n_bull >= 6:
                tier = "E"
        if atr_pct >= 5:
            tier += "+F"
    elif hole == 1 and whales >= 50 and te_up:
        tier = "ref (vetoed)"
    elif hole == 0 and whales >= 50 and te_up and mountain >= 3 and above_gold:
        tier = "armed (hole in force)"
    dia = int(di["n_blue_c"].iloc[i]) == 3 and mountain == 4 and above_gold and not hot
    return {"ticker": ticker, "date": str(df["date"].iloc[i].date()), "close": round(float(df["close"].iloc[i]), 2), "tier": tier,
            "diamond_strict": dia, "bars_since_break": since_break, "panels": n_bull, "whales": round(whales), "retail": round(float(dr["retail"].iloc[i]) if pd.notna(dr["retail"].iloc[i]) else 0),
            "mountain": mountain, "blue_confirmed": int(di["n_blue_c"].iloc[i]), "pink_confirmed": int(di["n_pink_c"].iloc[i]), "above_gold": above_gold,
            "gold_dist_atr": round(float(di["gold_dist"].iloc[i]), 1), "bubble": round(float(di["bubble"].iloc[i]), 1), "hot": hot, "atr_pct": round(atr_pct, 1),
            "hole": {1: "BROKE UP", 0: "in force", -1: "BROKE DOWN"}[hole], "te_up": te_up}


def judge_quality(row: dict) -> float | None:
    if not os.environ.get("TYPESAFE_API_KEY"):
        return None
    sys.path.insert(0, str(HERE.parent))
    import typesafe_judge as judge
    state = {k: v for k, v in row.items() if k not in ("date",)}
    try:
        a = judge.ask(state, {"quality": {"type": "score", "instructions": "How good is this Dragon/Diamond long setup for a next-open entry, given the lab's findings: fresh hole break (≤3 bars) best, ≥6 of 7 panels, whales ≥ 50 (no extra value above 60), Mountain ≥ 3 and above gold required, bubble hot = veto, ATR ≥ 5 % names pay most, retail 5-50 % fine, price far above gold band is fine.",
                                          "criteria": ["vetoed or stale", "weak", "decent", "strong", "textbook"]}})
        return round(float(a["quality"]["score"]), 2)
    except Exception:  # noqa: BLE001
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tickers", nargs="*")
    ap.add_argument("--refresh", action="store_true")
    ap.add_argument("--top", type=int, default=40)
    args = ap.parse_args()
    ticks = args.tickers or sorted(p.stem for p in (HERE / "cache").glob("*.csv") if p.stem not in ("SPY", "QQQ", "IWM"))
    rows = []
    t0 = time.time()
    for i, t in enumerate(ticks):
        try:
            r = read(t, args.refresh)
        except Exception as e:  # noqa: BLE001
            print(f"  {t}: {type(e).__name__}: {str(e)[:60]}", file=sys.stderr)
            continue
        if r:
            rows.append(r)
        if i % 50 == 0:
            print(f"  {i}/{len(ticks)} scanned ({time.time() - t0:.0f} s)", flush=True)
    df = pd.DataFrame(rows)
    live = df[(df["tier"] != "") | df["diamond_strict"]].copy()
    order = {"E+F": 0, "E": 1, "D+F": 2, "D": 3, "C+F": 4, "C": 5, "ref (vetoed)": 8, "armed (hole in force)": 7, "": 9}
    live["rank"] = live["tier"].map(order).fillna(6)
    live.loc[live["diamond_strict"], "rank"] -= 0.5
    live = live.sort_values(["rank", "atr_pct"], ascending=[True, False]).head(args.top)
    live["quality"] = [judge_quality(r) for r in live.to_dict("records")]
    today = date.today().isoformat()
    cols = ["ticker", "close", "tier", "diamond_strict", "quality", "bars_since_break", "panels", "whales", "retail", "mountain", "blue_confirmed", "pink_confirmed", "gold_dist_atr", "bubble", "atr_pct", "hole", "date"]
    md = [f"# Candidates — scan of {len(df)} tickers, bars through {df['date'].max()}", "",
          "Tier C = hole BROKE UP + whales ≥ 50 + TE up, not bubble-hot, Mountain ≥ 3, above gold (enter next open, exit when the hole breaks down). "
          "D = C and the break is ≤ 3 bars old. E = D and ≥ 6 of 7 panels. +F = ATR ≥ 5 %. 'armed' = everything but the break. quality = TypeSafe 0–1.", "",
          "| " + " | ".join(cols) + " |", "|" + "---|" * len(cols)]
    for r in live[cols].itertuples(index=False):
        md.append("| " + " | ".join("" if v is None or (isinstance(v, float) and pd.isna(v)) else str(v) for v in r) + " |")
    (HERE / f"candidates_{today}.md").write_text("\n".join(md), encoding="utf-8")
    live[cols].to_csv(HERE / f"candidates_{today}.csv", index=False)
    print("\n".join(md))


if __name__ == "__main__":
    main()
