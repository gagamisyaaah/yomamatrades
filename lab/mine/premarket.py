"""Pre-market sheet: turn last night's candidate list into today's trade plan.

usage: python3.12 -m mine.premarket [--candidates mine/candidates_<date>.csv] [--capital 100000] [--f 0.005] [--slots 20]
                                    [--exit bracket|scalp|runner] [--top 15]
For each candidate (tiers E+F > E > D+F > D > C+F > C): live quote from Nasdaq (last price; pre-market price when the
session is open), gap vs last close in ATR, earnings inside the next 7 calendar days, 20-day dollar volume, the
market guard (SPY vs 200-day), and the plan: entry style, target / stop / time stop for the chosen exit, position size
(1-ATR move = f × capital, ≤ 20 % of capital per name), expected win rate and edge for that exit from the holdout
frontier. Writes mine/premarket_<date>.md + .csv.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.request
from datetime import date, timedelta
from pathlib import Path

import pandas as pd

from . import data, ta

HERE = Path(__file__).parent
UA = data.UA
RANK = {"E+F": 0, "E": 1, "D+F": 2, "D": 3, "C+F": 4, "C": 5}
# holdout frontier (546 tickers never used for tuning, 2024+): (win rate, expectancy in ATR) by exit and ATR band
EXPECT = {"bracket": {"hi": (0.68, 0.42), "mid": (0.61, 0.19), "lo": (0.63, 0.16)},
          "scalp": {"hi": (0.87, 0.04), "mid": (0.88, 0.11), "lo": (0.88, 0.05)},
          "runner": {"hi": (0.49, 5.0), "mid": (0.40, 1.0), "lo": (0.38, 0.5)}}
GEOM = {"bracket": (2.0, 4.0, 20), "scalp": (0.5, 3.0, 10), "runner": (None, None, None)}


def get_json(url: str):
    req = urllib.request.Request(url, headers={"User-Agent": UA, "Accept": "application/json"})
    return json.load(urllib.request.urlopen(req, timeout=20))


def quote(sym: str) -> dict:
    try:
        j = get_json(f"https://api.nasdaq.com/api/quote/{sym}/info?assetclass=stocks")["data"]
    except Exception as e:  # noqa: BLE001
        return {"error": str(e)[:60]}
    num = lambda s: float(str(s).replace("$", "").replace(",", "")) if s and str(s) not in ("N/A", "None") else None  # noqa: E731
    p = j.get("primaryData") or {}
    s = j.get("secondaryData") or {}
    return {"status": j.get("marketStatus"), "last": num(p.get("lastSalePrice")), "last_chg_pct": num((p.get("percentageChange") or "").replace("%", "")),
            "pre": num(s.get("lastSalePrice")), "pre_chg_pct": num((s.get("percentageChange") or "").replace("%", "")), "session": s.get("lastTradeTimestamp") or p.get("lastTradeTimestamp")}


def earnings_soon(days: int = 7) -> set[str]:
    out = set()
    for k in range(0, days + 1):
        d = date.today() + timedelta(days=k)
        try:
            j = get_json(f"https://api.nasdaq.com/api/calendar/earnings?date={d.isoformat()}")
            for r in (j.get("data") or {}).get("rows") or []:
                out.add(r.get("symbol", "").upper())
        except Exception:  # noqa: BLE001
            pass
        time.sleep(0.4)
    return out


def dollar_volume(sym: str) -> float | None:
    df = data.fetch(sym)
    if df is None:
        return None
    return float((df["close"] * df["volume"]).tail(20).mean())


def guard_ok() -> tuple[bool, float, float]:
    spy = data.fetch("SPY", force=True)
    c = spy["close"]
    return bool(c.iloc[-1] > c.rolling(200).mean().iloc[-1]), float(c.iloc[-1]), float(c.rolling(200).mean().iloc[-1])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--candidates", default=None)
    ap.add_argument("--capital", type=float, default=100000)
    ap.add_argument("--f", type=float, default=0.005, help="a 1-ATR move = f × capital")
    ap.add_argument("--slots", type=int, default=20)
    ap.add_argument("--exit", default="bracket", choices=list(GEOM))
    ap.add_argument("--top", type=int, default=15)
    ap.add_argument("--min-dollar-vol", type=float, default=5e6)
    args = ap.parse_args()
    cand = Path(args.candidates) if args.candidates else sorted(HERE.glob("candidates_*.csv"))[-1]
    c = pd.read_csv(cand)
    c = c[c["tier"].isin(RANK)].copy()
    c["rank"] = c["tier"].map(RANK)
    c = c.sort_values(["rank", "quality", "atr_pct"], ascending=[True, False, False]).head(args.top)
    ok, spy_px, spy_ma = guard_ok()
    earn = earnings_soon(7)
    X, Y, N = GEOM[args.exit]
    rows = []
    for r in c.itertuples(index=False):
        q = quote(r.ticker)
        atr = r.close * r.atr_pct / 100.0
        ref = q.get("pre") or q.get("last") or r.close
        gap_atr = (ref - r.close) / atr if atr else 0.0
        dv = dollar_volume(r.ticker)
        band = "hi" if r.atr_pct >= 5 else ("mid" if r.atr_pct >= 3 else "lo")
        win, edge = EXPECT[args.exit][band]
        notional = min(args.capital * args.f / (r.atr_pct / 100.0), 0.2 * args.capital)
        reasons = []
        if not ok:
            reasons.append("market guard OFF (SPY below 200-day)")
        if r.ticker in earn:
            reasons.append("earnings within 7 days")
        if dv is not None and dv < args.min_dollar_vol:
            reasons.append(f"thin: ${dv/1e6:.1f}M/day")
        if gap_atr > 1.0:
            reasons.append(f"gapping +{gap_atr:.1f} ATR — do not chase; limit at {r.close + 0.5*atr:.2f}")
        decision = "TAKE" if not reasons else "SKIP"
        entry = ref if not reasons else None
        rows.append({"ticker": r.ticker, "tier": r.tier, "quality": r.quality, "last_close": r.close, "live": q.get("pre") or q.get("last"), "session": q.get("status"),
                     "gap_atr": round(gap_atr, 2), "atr_pct": r.atr_pct, "dollar_vol_M": round(dv / 1e6, 1) if dv else None, "earnings_7d": r.ticker in earn,
                     "decision": decision, "why": "; ".join(reasons), "entry": round(entry, 2) if entry else None,
                     "target": round(entry + X * atr, 2) if entry and X else None, "stop": round(entry - Y * atr, 2) if entry and Y else ("hole breaks down" if entry else None),
                     "time_stop_bars": N, "notional": round(notional), "shares": int(notional / ref) if ref else None,
                     "exp_win": win, "exp_edge_atr": edge, "exp_edge_usd": round(edge * atr / ref * notional) if ref else None,
                     "panels": r.panels, "whales": r.whales, "mountain": r.mountain, "bars_since_break": r.bars_since_break})
        time.sleep(0.3)
    t = pd.DataFrame(rows)
    today = date.today().isoformat()
    t.to_csv(HERE / f"premarket_{today}.csv", index=False)
    take = t[t["decision"] == "TAKE"]
    md = [f"# Pre-market sheet {today} — from {cand.name}, exit = {args.exit} ({'+%s / −%s ATR, %s bars' % (X, Y, N) if X else 'hole breaks down'}), ${args.capital:,.0f}, f = {args.f:.1%}, ≤ {args.slots} names",
          "", f"Market guard: {'ON — SPY %.0f above its 200-day %.0f' % (spy_px, spy_ma) if ok else 'OFF — SPY %.0f below its 200-day %.0f: no new entries' % (spy_px, spy_ma)} · earnings calendar checked for the next 7 days · quotes: Nasdaq ({t['session'].iloc[0] if len(t) else 'n/a'})", "",
          f"**{len(take)} to take, {len(t) - len(take)} skipped.** Expected win rate / edge per trade come from the 546-ticker holdout frontier for this exit and the name's ATR band; they are averages, not promises.", "",
          "| ticker | tier | quality | last close | live | gap (ATR) | ATR % | $vol/day | decision | entry | target | stop | shares | notional | exp win | exp edge | why |",
          "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for r in t.itertuples(index=False):
        md.append(f"| {r.ticker} | {r.tier} | {r.quality} | {r.last_close} | {r.live} | {r.gap_atr} | {r.atr_pct} | {r.dollar_vol_M} | **{r.decision}** | {r.entry or ''} | {r.target or ''} | {r.stop or ''} | {r.shares or ''} | {r.notional} | {r.exp_win:.0%} | {r.exp_edge_atr} ATR ≈ ${r.exp_edge_usd or 0} | {r.why} |")
    (HERE / f"premarket_{today}.md").write_text("\n".join(md), encoding="utf-8")
    print("\n".join(md))


if __name__ == "__main__":
    main()
