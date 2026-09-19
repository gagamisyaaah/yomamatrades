"""LIVE — what the engine says right now, with the honest numbers next to every name.

usage: python3.12 bend/live.py [--pack] [--run] [--quotes] [--top 30] [--split 2024-01-01]
  --pack    repack daily + weekly bars from mine/cache (after `python3.12 -m mine.data --universe broad` refreshed them)
  --run     run the Bend engine on bend/data/D and bend/data/W (≈ 1 min for the whole universe)
  --quotes  pull live Nasdaq quotes, the earnings calendar and the SPY guard for the top names (pre-market use)

Candidates = every ticker whose last daily or weekly bar has the C reading ON (hole broke up, whales ≥ 50, TE rising,
bubble not hot, Mountain ≥ 3, above gold). "new" = the reading turned on at the last bar. Expectancies come from the
full-universe backtest (bend/data/trades_D.csv, trades_W.csv): the C signals of that timeframe and ATR band since --split,
next to the random control with the same exits. Writes bend/data/live_<date>.md.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
import time
from datetime import date
from pathlib import Path

import numpy as np
import pandas as pd

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
from mine import data, premarket  # noqa: E402

BANDS = [0, 3, 5, 8, 12, 1000]
LABELS = ["<3", "3-5", "5-8", "8-12", "≥12"]


def decode_last(d: Path) -> pd.DataFrame:
    """Last two bars of every ticker: flags, ATR %, close, date."""
    order = [l.split()[0] for l in (d / "list.txt").read_text().split("\n") if l.strip()]
    w = np.frombuffer((d / "states.bin").read_bytes(), dtype="<u4")
    pos = 0
    rows = []
    for t in order:
        n = int(w[pos]); st = w[pos + 1:pos + 1 + 2 * n].reshape(n, 2); pos += 1 + 2 * n
        m = int(w[pos]); pos += 1 + 16 * m
        bars = np.fromfile(d / f"{t}.bin", dtype="<u4").reshape(-1, 6)
        f, fp = int(st[-1, 0]), int(st[-2, 0])
        rows.append({"ticker": t, "date": str(bars[-1, 0]), "close": bars[-1, 4] / 10000.0, "atr_pct": st[-1, 1] / 1000.0,
                     "C": (f & 125) == 61, "C_prev": (fp & 125) == 61, "fresh": bool(f & 2048), "panels": (f >> 24) & 15, "whales": (f >> 16) & 255,
                     "hole": "up" if f & 1 else ("down" if f & 2 else "in force"), "hot": bool(f & 64), "mtn3": bool(f & 16), "gold": bool(f & 32), "te": bool(f & 8)})
    return pd.DataFrame(rows)


def expectancy(tf: str, split: str) -> pd.DataFrame:
    tr = pd.read_csv(HERE / "data" / f"trades_{tf}.csv", parse_dates=["date"])
    tr = tr[tr.date >= split]
    tr["band"] = pd.cut(tr.atr_pct, BANDS, labels=LABELS)
    rows = []
    for b, g in tr.groupby("band", observed=True):
        c, r = g[~g.control], g[g.control]
        if len(c) < 20:
            continue
        rows.append({"tf": tf, "band": b, "n": len(c), "hole_exit_%": round(c.flip.mean(), 2), "hole_exit_win": round((c.flip > 0).mean(), 2), "hole_bars": round(c.flip_bars.mean(), 0),
                     "bracket_2_4_ATR": round(c.r24.mean(), 3), "bracket_win": round((c.r24 > 0).mean(), 2), "scalp_win": round((c.s53 > 0).mean(), 2),
                     "random_hole_%": round(r.flip.mean(), 2), "random_bracket_ATR": round(r.r24.mean(), 3), "random_bracket_win": round((r.r24 > 0).mean(), 2)})
    return pd.DataFrame(rows)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pack", action="store_true"); ap.add_argument("--run", action="store_true"); ap.add_argument("--quotes", action="store_true")
    ap.add_argument("--top", type=int, default=30); ap.add_argument("--split", default="2024-01-01")
    a = ap.parse_args()
    if a.pack:
        for tf in ("D", "W"):
            subprocess.run([sys.executable, str(HERE / "pack.py"), "--tf", tf], check=True)
    if a.run:
        for tf in ("D", "W"):
            t0 = time.time(); subprocess.run([str(HERE / "dragon"), str(HERE / "data" / tf), "--threads", "8"], check=True, capture_output=True); print(f"engine {tf}: {time.time()-t0:.0f} s")
    D = decode_last(HERE / "data" / "D"); W = decode_last(HERE / "data" / "W")
    ex = pd.concat([expectancy("D", a.split), expectancy("W", a.split)], ignore_index=True)
    m = D.merge(W[["ticker", "C", "C_prev", "fresh", "panels", "whales", "hole"]].rename(columns=lambda c: c if c == "ticker" else "w_" + c), on="ticker", how="left")
    cand = m[m.C | m.w_C.fillna(False)].copy()
    cand["new"] = (cand.C & ~cand.C_prev) | (cand.w_C.fillna(False) & ~cand.w_C_prev.fillna(True))
    cand["both"] = cand.C & cand.w_C.fillna(False)
    cand["band"] = pd.cut(cand.atr_pct, BANDS, labels=LABELS)
    cand["score"] = cand.new.astype(int) * 4 + cand.both.astype(int) * 2 + cand.fresh.astype(int) + (cand.panels >= 6).astype(int) + (cand.atr_pct >= 5).astype(int)
    cand = cand.sort_values(["score", "atr_pct"], ascending=[False, False])
    exD = ex[ex.tf == "D"].set_index("band"); exW = ex[ex.tf == "W"].set_index("band")
    cand["exp_bracket_ATR"] = [exD.loc[b, "bracket_2_4_ATR"] if b in exD.index else np.nan for b in cand.band]
    cand["exp_bracket_win"] = [exD.loc[b, "bracket_win"] if b in exD.index else np.nan for b in cand.band]
    cand["random_bracket_ATR"] = [exD.loc[b, "random_bracket_ATR"] if b in exD.index else np.nan for b in cand.band]
    top = cand.head(a.top).copy()
    if a.quotes:
        ok, spy_px, spy_ma = premarket.guard_ok(); earn = premarket.earnings_soon(7)
        live, gap, sess, e7 = [], [], [], []
        for r in top.itertuples(index=False):
            q = premarket.quote(r.ticker); px = q.get("pre") or q.get("last")
            live.append(px); sess.append(q.get("status")); e7.append(r.ticker in earn)
            gap.append(round((px - r.close) / (r.close * r.atr_pct / 100.0), 2) if px and r.atr_pct else None)
            time.sleep(0.3)
        top["live"], top["gap_ATR"], top["session"], top["earnings_7d"] = live, gap, sess, e7
        guard = f"market guard {'ON' if ok else 'OFF'} — SPY {spy_px:.0f} vs 200-day {spy_ma:.0f}"
    else:
        guard = "market guard: run with --quotes"
    today = date.today().isoformat(); last = D.date.max()
    cols = ["ticker", "date", "close", "atr_pct", "band", "new", "both", "fresh", "panels", "whales", "hole", "w_hole", "exp_bracket_ATR", "exp_bracket_win", "random_bracket_ATR"] + (["live", "gap_ATR", "session", "earnings_7d"] if a.quotes else [])
    pd.set_option("display.width", 250); pd.set_option("display.max_columns", 40)
    head = [f"# LIVE {today} — last bar {last} · {len(D)} tickers · C on daily: {int(m.C.sum())}, weekly: {int(m.w_C.fillna(False).sum())}, both: {int(cand.both.sum())}, new at the last bar: {int(cand.new.sum())} · {guard}", "",
            "Read the last three columns before anything else: the expected +2/−4 ATR bracket result for a C signal in this name's ATR band, its win rate, and the same number for RANDOM entries in the same band (the full-universe backtest). The gap between them is the whole edge.", ""]
    body = top[cols].to_string(index=False)
    exs = "\n\n## Expectancy by timeframe and ATR band (C signals vs random entries, same exits, " + a.split + "+)\n\n" + ex.to_string(index=False)
    print("\n".join(head) + body + exs)
    md = lambda d: "| " + " | ".join(map(str, d.columns)) + " |\n|" + "---|" * len(d.columns) + "\n" + "\n".join("| " + " | ".join(str(v) for v in r) + " |" for r in d.itertuples(index=False))  # noqa: E731
    (HERE / "data" / f"live_{today}.md").write_text("\n".join(head) + md(top[cols]) + "\n\n## Expectancy by timeframe and ATR band\n\n" + md(ex) + "\n", encoding="utf-8")
    top[cols].to_csv(HERE / "data" / f"live_{today}.csv", index=False)


if __name__ == "__main__":
    main()
