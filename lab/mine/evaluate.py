"""Evaluate the Dragon and Diamond TOTAL signals as written in the READMEs, event by event, across every ticker.

usage: python3.12 -m mine.evaluate [--split 2024-01-01]
writes mine/evaluate.md — per rule × exit: trades, win rate, avg/median return, profit factor, expectancy in ATR,
signals per week across the universe, bear-regime results, share of tickers with a positive average.

Entries fill at the NEXT OPEN after the signal bar. Exits:
  time5 / time10      close 5 / 10 bars after entry
  flip                first bar the rule's STATE is no longer true (close of that bar)
  bracket 3:1.5 ATR   +3 ATR target or −1.5 ATR stop (whichever the path hits first; both same bar → stop)
  trail 3 ATR         chandelier: highest high since entry − 3 ATR, exit on close below
"""
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd

from . import data, features

HERE = Path(__file__).parent


def rules(df: pd.DataFrame) -> dict[str, tuple[pd.Series, pd.Series]]:
    """name → (entry_state, exit_state). Entries are the rising edge of entry_state (computed per ticker)."""
    d = df
    r = {}
    # ── Dragon (Danny's reading) ──
    r["Dragon: all 7 panels"] = (d["dr_ss_confirm"] & (d["dr_n_bull"] >= 7), d["dr_n_bull"] <= 5)
    r["Dragon: 6 of 7"] = (d["dr_ss_confirm"] & (d["dr_n_bull"] >= 6), d["dr_n_bull"] <= 4)
    r["Dragon: 5 of 7"] = (d["dr_ss_confirm"] & (d["dr_n_bull"] >= 5), d["dr_n_bull"] <= 3)
    r["Dragon: red candle confirmed + ribbon red + whales≥50"] = (d["dr_ss_confirm"] & d["dr_ribbon_bull"] & (d["dr_whales"] >= 50), ~d["dr_ss_bull"] | ~d["dr_ribbon_bull"])
    r["Dragon: hole BROKE UP + whales≥50 + TE up"] = ((d["dr_hole_state"] == 1) & (d["dr_whales"] >= 50) & d["dr_te_bull"], (d["dr_hole_state"] == -1) | ~d["dr_te_bull"])
    r["Dragon: SS candle only (J>50)"] = (d["dr_ss_bull"], ~d["dr_ss_bull"])
    r["Dragon: whales ≥ 75 (surge zone) + candle red"] = ((d["dr_whales"] >= 75) & d["dr_ss_bull"], (d["dr_whales"] < 50) | ~d["dr_ss_bull"])
    # ── Diamond (founders' reading) ──
    r["Diamond: strict (3 confirmed blue, Mountain 4, above gold, not overheated)"] = ((d["di_n_blue_c"] == 3) & (d["di_mountain"] == 4) & d["di_above_gold"] & ~d["di_overheated"], (d["di_n_pink_c"] >= 2) | (d["di_mountain"] <= 2) | d["di_below_gold"])
    r["Diamond: 2 of 3 confirmed + Mountain ≥ 3 + above gold"] = ((d["di_n_blue_c"] >= 2) & (d["di_mountain"] >= 3) & d["di_above_gold"], (d["di_n_pink_c"] >= 2) | (d["di_mountain"] <= 2) | d["di_below_gold"])
    r["Diamond: 2 of 3 blue (last state, unconfirmed)"] = (d["di_n_blue"] >= 2, d["di_n_pink"] >= 2)
    r["Diamond: 3 of 3 blue + Mountain green (≥3)"] = ((d["di_n_blue"] == 3) & (d["di_mountain"] >= 3), (d["di_n_pink"] >= 1) | (d["di_mountain"] <= 2))
    r["Diamond: Echo blue diamond confirmed"] = (d["di_c_echo"] == 1, d["di_c_echo"] != 1)
    r["Diamond: Mountain maxed (4) + above gold"] = ((d["di_mountain"] == 4) & d["di_above_gold"], d["di_mountain"] <= 2)
    # ── the mined exit refinement: also leave when price is > 2 ATR above the gold band while the bubble is > 8.7 ──
    hot = (d["di_gold_dist"] > 2.0) & (d["di_bubble"] > 8.7)
    r["Dragon: hole BROKE UP + whales≥50 + TE up — exit flip OR overheated"] = ((d["dr_hole_state"] == 1) & (d["dr_whales"] >= 50) & d["dr_te_bull"], (d["dr_hole_state"] == -1) | ~d["dr_te_bull"] | hot)
    r["Dragon: hole BROKE UP + whales≥50 + TE up + NOT overheated at entry"] = ((d["dr_hole_state"] == 1) & (d["dr_whales"] >= 50) & d["dr_te_bull"] & ~hot, (d["dr_hole_state"] == -1) | ~d["dr_te_bull"])
    r["Diamond: strict — exit flip OR overheated"] = ((d["di_n_blue_c"] == 3) & (d["di_mountain"] == 4) & d["di_above_gold"] & ~d["di_overheated"], (d["di_n_pink_c"] >= 2) | (d["di_mountain"] <= 2) | d["di_below_gold"] | hot)
    r["Diamond: 2 of 3 confirmed + Mountain ≥ 3 + above gold — exit flip OR overheated"] = ((d["di_n_blue_c"] >= 2) & (d["di_mountain"] >= 3) & d["di_above_gold"], (d["di_n_pink_c"] >= 2) | (d["di_mountain"] <= 2) | d["di_below_gold"] | hot)
    # ── both worlds agreeing ──
    r["Dragon 5/7 AND Diamond 2/3 confirmed"] = (d["dr_ss_confirm"] & (d["dr_n_bull"] >= 5) & (d["di_n_blue_c"] >= 2) & d["di_above_gold"], (d["dr_n_bull"] <= 3) | (d["di_n_pink_c"] >= 2))
    # ── shorts (mirrors) ──
    r["SHORT Dragon: 5 of 7 bearish"] = (d["dr_ss_confirm_s"] & (d["dr_n_bear"] >= 5), d["dr_n_bear"] <= 3)
    r["SHORT Diamond: 2 of 3 pink confirmed + Mountain ≤ 1 + below gold"] = ((d["di_n_pink_c"] >= 2) & (d["di_mountain"] <= 1) & d["di_below_gold"], (d["di_n_blue_c"] >= 2) | (d["di_mountain"] >= 3))
    # ── controls: the same exits on entries that ignore the indicators ──
    rng = np.random.RandomState(7)
    coin = pd.Series(rng.rand(len(d)) < 0.12, index=d.index)          # ~1 random entry per 8 days per ticker
    r["BASELINE: random days (same exits; flip ≈ 10 bars)"] = (coin, ~coin.shift(10, fill_value=False) & ~coin.shift(9, fill_value=False) & (pd.Series(np.arange(len(d)) % 10 == 0, index=d.index)))
    r["BASELINE: every up day (close > close[1])"] = (d["fwd_1"].shift(1).fillna(0) > 0, d["fwd_1"].shift(1).fillna(0) <= 0)
    return r


def simulate(df: pd.DataFrame, px: dict[str, pd.DataFrame], entry: pd.Series, exit_state: pd.Series, short: bool = False) -> pd.DataFrame:
    """Event backtest across tickers. Returns one row per trade per exit style."""
    rows = []
    sign = -1.0 if short else 1.0
    for t, g in df.groupby("ticker", sort=False):
        p = px.get(t)
        if p is None:
            continue
        idx = g.index.values
        e = entry.loc[idx].values
        x = exit_state.loc[idx].values
        dates = g["date"].values
        regime = g["regime_bull"].values
        atrp = g["atr_pct"].values
        o, h, l, c = p["open"].values, p["high"].values, p["low"].values, p["close"].values
        n = len(o)
        rising = e & ~np.roll(e, 1)
        rising[0] = False
        in_trade_until = -1
        for i in np.where(rising)[0]:
            if i <= in_trade_until or i + 2 >= n:
                continue
            ent = o[i + 1]
            atr = ent * atrp[i] / 100.0
            res = {"ticker": t, "date": dates[i], "regime_bull": bool(regime[i]), "atr_pct": atrp[i]}
            # time exits
            for k in (5, 10):
                j = min(i + k, n - 1)
                res[f"time{k}"] = sign * (c[j] / ent - 1.0) * 100.0
            # flip
            j = i + 1
            while j < n - 1 and not x[j]:
                j += 1
            res["flip"] = sign * (c[j] / ent - 1.0) * 100.0
            res["flip_bars"] = j - i
            # bracket
            tgt = ent + sign * 3.0 * atr
            stp = ent - sign * 1.5 * atr
            out = None
            for j in range(i + 1, min(i + 41, n)):
                hit_t = h[j] >= tgt if not short else l[j] <= tgt
                hit_s = l[j] <= stp if not short else h[j] >= stp
                if hit_s:
                    out = sign * (stp / ent - 1.0) * 100.0
                    break
                if hit_t:
                    out = sign * (tgt / ent - 1.0) * 100.0
                    break
            res["bracket"] = out if out is not None else sign * (c[min(i + 40, n - 1)] / ent - 1.0) * 100.0
            # chandelier trail
            hh = -np.inf
            ll = np.inf
            out = None
            for j in range(i + 1, min(i + 61, n)):
                hh = max(hh, h[j])
                ll = min(ll, l[j])
                if not short and c[j] < hh - 3.0 * atr:
                    out = (c[j] / ent - 1.0) * 100.0
                    break
                if short and c[j] > ll + 3.0 * atr:
                    out = -(c[j] / ent - 1.0) * 100.0
                    break
            res["trail"] = out if out is not None else sign * (c[min(i + 60, n - 1)] / ent - 1.0) * 100.0
            in_trade_until = i + 1
            rows.append(res)
    return pd.DataFrame(rows)


def stats(tr: pd.DataFrame, col: str, weeks: float) -> dict:
    r = tr[col].dropna()
    if len(r) == 0:
        return {}
    wins = r[r > 0].sum()
    losses = -r[r <= 0].sum()
    per_ticker = tr.groupby("ticker")[col].mean()
    bear = tr.loc[~tr["regime_bull"], col]
    return {"n": int(len(r)), "total": round(float(r.sum()), 1), "win": round(float((r > 0).mean()), 3), "avg": round(float(r.mean()), 2), "med": round(float(r.median()), 2),
            "avg_atr": round(float((r / tr.loc[r.index, "atr_pct"]).mean()), 2), "pf": round(float(wins / losses), 2) if losses > 0 else None,
            "per_wk": round(len(r) / weeks, 2), "tick_pos": round(float((per_ticker > 0).mean()), 2),
            "bear_n": int(len(bear)), "bear_avg": round(float(bear.mean()), 2) if len(bear) else None}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--split", default="2024-01-01")
    args = ap.parse_args()
    df = features.load()
    df = df.sort_values(["ticker", "date"]).reset_index(drop=True)
    spy = data.fetch("SPY")
    if spy is not None:
        s = spy.set_index("date")["close"]
        reg = (s > s.rolling(200).mean())
        df["regime_bull"] = df["date"].map(reg).fillna(True).astype(bool)
    px = {t: data.fetch(t) for t in df["ticker"].unique()}
    px = {t: p.reset_index(drop=True) for t, p in px.items() if p is not None and len(p) == (df["ticker"] == t).sum()}
    out = [f"# Total-signal evaluation — {len(px)} tickers, entries at next open, {df['date'].min().date()} → {df['date'].max().date()}",
           "", f"Out-of-sample = trades signalled on/after {args.split}. Bear regime = SPY below its 200-day line ({(~df['regime_bull']).mean():.0%} of days).",
           "", "One fixed-size unit per trade, no compounding: TOTAL = sum of the trade returns (in % of one unit) — the 'stack up' number.",
           "Columns: n trades · TOTAL % · win rate · avg % · median % · avg in ATR units · profit factor · signals/week across the universe · share of tickers with positive avg · bear-regime n and avg %", ""]
    all_rows = []
    ledgers = []
    for name, (ent, ex) in rules(df).items():
        short = name.startswith("SHORT")
        tr = simulate(df, px, ent.fillna(False), ex.fillna(False), short)
        if len(tr) == 0:
            out.append(f"## {name}\n\nno trades\n")
            continue
        tr["date"] = pd.to_datetime(tr["date"])
        tr.insert(0, "rule", name)
        ledgers.append(tr)
        te = tr[tr["date"] >= args.split]
        tra = tr[tr["date"] < args.split]
        wk_te = max(1.0, (df["date"].max() - pd.Timestamp(args.split)).days / 7.0)
        wk_tr = max(1.0, (pd.Timestamp(args.split) - df["date"].min()).days / 7.0)
        out += [f"## {name}", "", f"trades: {len(tra)} in-sample, {len(te)} out-of-sample · mean bars held on flip exit: {tr['flip_bars'].mean():.1f}", "",
                "| exit | sample | n | TOTAL % | win | avg % | med % | avg ATR | PF | /wk | tickers+ | bear n | bear avg % |", "|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
        for col in ("time5", "time10", "flip", "bracket", "trail"):
            for label, part, wk in (("train", tra, wk_tr), ("TEST", te, wk_te)):
                s = stats(part, col, wk)
                if s:
                    out.append(f"| {col} | {label} | {s['n']} | {s['total']} | {s['win']} | {s['avg']} | {s['med']} | {s['avg_atr']} | {s['pf']} | {s['per_wk']} | {s['tick_pos']} | {s['bear_n']} | {s['bear_avg']} |")
                    all_rows.append({"rule": name, "exit": col, "sample": label, **s})
        out.append("")
    res = pd.DataFrame(all_rows)
    base = res[res["rule"].str.startswith("BASELINE: random")].set_index(["exit", "sample"])["avg"]
    res["excess_avg_vs_random"] = res.apply(lambda r: round(r["avg"] - base.get((r["exit"], r["sample"]), np.nan), 2), axis=1)
    res.to_csv(HERE / "evaluate.csv", index=False)
    pd.concat(ledgers, ignore_index=True).to_csv(HERE / "trades_ledger.csv", index=False)
    top = res[(res["sample"] == "TEST") & (~res["rule"].str.startswith("BASELINE")) & (res["n"] >= 100)].sort_values("excess_avg_vs_random", ascending=False).head(15)
    out += ["", "## Edge over random entries (out-of-sample, same exit): avg % per trade minus the random-day baseline", "",
            "| rule | exit | n | TOTAL % | avg % | random avg % | EXCESS % | PF | /wk |", "|---|---|---|---|---|---|---|---|---|"]
    for _, r in top.iterrows():
        out.append(f"| {r['rule']} | {r['exit']} | {r['n']} | {r['total']} | {r['avg']} | {round(base.get((r['exit'], 'TEST'), float('nan')), 2)} | {r['excess_avg_vs_random']} | {r['pf']} | {r['per_wk']} |")
    (HERE / "evaluate.md").write_text("\n".join(out), encoding="utf-8")
    print("\n".join(out[-20:]))


if __name__ == "__main__":
    main()
