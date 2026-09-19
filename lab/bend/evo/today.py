"""TODAY — evaluate the three super indicators on the latest bar of every cached ticker; write bend/data/today.json for
the Tree of Life terminal. Reads bend/data/supers.json ({family: {rule, name, evidence...}}).
usage: python3.12 bend/evo/today.py [--tail 420] [--quotes]"""
from __future__ import annotations

import argparse
import json
import sys
import time
from datetime import date
from pathlib import Path

import numpy as np
import pandas as pd

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT)); sys.path.insert(0, str(HERE))
from mine import ta  # noqa: E402
from blocks import blocks  # noqa: E402
from geometry import geometry  # noqa: E402
from replay import parse  # noqa: E402


def scan_one(args):
    """one ticker: every block family the rules need, on the last `tail` bars; returns the BASE-HIT candidates"""
    t, tail, rules, gold = args
    import warnings; warnings.filterwarnings("ignore")
    from invented import invented; from scouted import scouted
    try:
        df = pd.read_csv(ROOT / f"mine/cache/{t}.csv", parse_dates=["date"]).tail(tail).reset_index(drop=True)
    except Exception:
        return []
    if len(df) < 300:
        return []
    B = pd.concat([blocks(df), geometry(df), invented(df), scouted(df)], axis=1)
    if gold is not None:
        B["gold"] = np.nan; B.loc[B.index[-1], "gold"] = gold[0]; B.loc[B.index[-2], "gold"] = gold[1]
    last = B.iloc[-1]; prev = B.iloc[-2]; atr = float(ta.atr(df, 14).iloc[-1]); close = float(df.close.iloc[-1]); res = []
    for fam, conds in rules.items():
        if any(b not in B for b, _, _ in conds):
            continue
        def holds(row):
            return all((row[b] <= thr) if op == "≤" else ((row[b] >= thr) if op == "≥" else (row[b] == thr)) for b, op, thr in conds if not np.isnan(row[b]))
        on = holds(last) and all(not np.isnan(last[b]) for b, _, _ in conds)
        if not on:
            continue
        was_on = holds(prev) and all(not np.isnan(prev[b]) for b, _, _ in conds)
        res.append({"ticker": t, "family": fam, "new": not was_on, "date": str(df.date.iloc[-1].date()), "close": close, "atr": atr, "atr_pct": round(atr / close * 100, 2),
                    "blocks": [{"block": b, "op": op, "thr": thr, "value": round(float(last[b]), 4)} for b, op, thr in conds],
                    "plan": {"entry": "next open (or live price if within +0.5 ATR of the close)", "target": round(close + 2 * atr, 2), "stop": round(close - 4 * atr, 2), "time_stop_bars": 20},
                    "context": {"hi252": round(float(last.get("hi252", np.nan)), 1), "struct": float(last.get("struct", np.nan)), "retrace": round(float(last.get("retrace", np.nan)), 2), "eff20": round(float(last.get("eff20", np.nan)), 2), "rng20": round(float(last.get("rng20", np.nan)), 2), "volr20": round(float(last.get("volr20", np.nan)), 2), "dvol20_M": round(10 ** float(last.get("dvol20", np.nan)) / 1e6, 1)}})
    return res


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--tail", type=int, default=420); ap.add_argument("--quotes", action="store_true"); ap.add_argument("--procs", type=int, default=3)
    a = ap.parse_args()
    supers = json.load(open(ROOT / "bend/data/supers.json"))
    rules = {fam: parse(s["rule"]) for fam, s in supers.items() if not s.get("engine")}
    caps = pd.read_csv(ROOT / "mine/caps.csv").set_index("symbol")["cap"]
    out = []; t0 = time.time()
    # ── RUNNER lane: the engine's hole state on the last bar of every ticker (bend/data/D/states.bin) ──
    run = supers.get("RUNNER")
    if run:
        d = ROOT / "bend/data/D"; order = [ln.split()[0] for ln in (d / "list.txt").read_text().split("\n") if ln.strip()]
        w = np.frombuffer((d / "states.bin").read_bytes(), dtype="<u4"); pos = 0; u = run["universe"]
        for t in order:
            n = int(w[pos]); st = w[pos + 1:pos + 1 + 2 * n:2].astype(np.int64); pos += 1 + 2 * n; m = int(w[pos]); pos += 1 + 16 * m
            f = int(st[-1]); cap = float(caps.get(t, np.nan))
            if not (f & 1):
                continue
            try:
                df = pd.read_csv(ROOT / f"mine/cache/{t}.csv", parse_dates=["date"])
            except Exception:
                continue
            if len(df) != n:
                continue
            up = (st & 1) > 0; brk = int(np.flatnonzero(up & ~np.roll(up, 1))[-1]) if (up & ~np.roll(up, 1)).any() else None
            if brk is None:
                continue
            atr = float(ta.atr(df, 14).iloc[-1]); close = float(df.close.iloc[-1]); atr_pct = atr / close * 100
            v = df.volume.astype(float); sma20 = float(v.rolling(20).mean().iloc[brk]) if brk >= 20 else float("nan")
            bvol = float(max(v.iloc[brk], v.iloc[brk - 1]) / sma20) if sma20 and sma20 > 0 else float("nan")
            dvol = float((df.close * df.volume).tail(20).mean() / 1e6)
            in_uni = (u["cap_min"] <= cap <= u["cap_max"]) and atr_pct >= u["atr_pct_min"] if not np.isnan(cap) else False
            out.append({"ticker": t, "family": "RUNNER", "new": (n - 1 - brk) <= 3, "date": str(df.date.iloc[-1].date()), "close": close, "atr": atr, "atr_pct": round(atr_pct, 2), "cap_B": round(cap / 1e9, 2) if not np.isnan(cap) else None, "in_universe": bool(in_uni),
                        "bars_since_break": int(n - 1 - brk), "break_vol_ratio": round(bvol, 2), "dvol20_M": round(dvol, 1), "hole_level": round(float(df.close.iloc[brk]), 2),
                        "blocks": [{"block": "hole_up", "op": "=", "thr": 1, "value": 1}, {"block": "fresh", "op": "=", "thr": 1, "value": int((n - 1 - brk) <= 3)}, {"block": "break volume ×20d", "op": "≥", "thr": 1.5, "value": round(bvol, 2)}, {"block": "ATR %", "op": "≥", "thr": u["atr_pct_min"], "value": round(atr_pct, 2)}],
                        "plan": {"entry": "next open (the break is confirmed at the close)", "target": None, "stop": "hole breaks down (two closes below the lower level) — the hold IS the trade", "time_stop_bars": None},
                        "context": {"hi252": round(float(df.close.iloc[-1] / df.high.tail(252).max() * 100), 1), "struct": None, "retrace": None, "eff20": None, "rng20": None, "volr20": round(float(v.iloc[-1] / v.rolling(20).mean().iloc[-1]), 2), "dvol20_M": round(dvol, 1)}})
        print(f"RUNNER: {sum(1 for c in out if c['in_universe'])} in the universe, {sum(1 for c in out if c['new'] and c['in_universe'])} fresh; {len(out)} holes up overall ({time.time()-t0:.0f}s)", flush=True)
    tickers = sorted(p.stem for p in (ROOT / "mine/cache").glob("*.csv"))
    # the Diamond gold flag on the last two bars comes from the engine states (bit 5), not from the block functions
    gold_last = {}
    if run:
        d = ROOT / "bend/data/D"; order = [ln.split()[0] for ln in (d / "list.txt").read_text().split("\n") if ln.strip()]
        w = np.frombuffer((d / "states.bin").read_bytes(), dtype="<u4"); pos = 0
        for t in order:
            n = int(w[pos]); st = w[pos + 1:pos + 1 + 2 * n:2].astype(np.int64); pos += 1 + 2 * n; m = int(w[pos]); pos += 1 + 16 * m
            gold_last[t] = (int(bool(st[-1] & 32)), int(bool(st[-2] & 32)))
    from multiprocessing import Pool
    jobs = [(t, a.tail, rules, gold_last.get(t)) for t in tickers]
    with Pool(a.procs) as pool:
        for i, res in enumerate(pool.imap_unordered(scan_one, jobs, chunksize=8)):
            if res:
                out.extend(res)
            if i % 400 == 0:
                print(i, len(out), f"{time.time()-t0:.0f}s", flush=True)
    res = {"date": date.today().isoformat(), "scanned": len(tickers), "candidates": out}
    if a.quotes:
        from mine import premarket
        ok, px, ma = premarket.guard_ok(); earn = premarket.earnings_soon(7)
        res["guard"] = {"on": ok, "spy": px, "sma200": ma}
        for c in out:
            c["earnings_7d"] = c["ticker"] in earn
    json.dump(res, open(ROOT / "bend/data/today.json", "w"), indent=1)
    for c in out:
        c.setdefault("cap_B", round(float(caps.get(c["ticker"], np.nan)) / 1e9, 2) if c["ticker"] in caps.index else None)
    fams = pd.Series([c["family"] for c in out]).value_counts().to_dict() if out else {}
    print(f"{len(out)} candidates on {len(tickers)} tickers: {fams}  ({time.time()-t0:.0f}s)")


if __name__ == "__main__":
    main()
