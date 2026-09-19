"""Calibration sweep for the Diamond "strict" reading (3 confirmed blue diamonds + Mountain 4 + above gold + not overheated).

usage: python3.12 -m mine.diamond_sweep
Sweeps confirmation window × min wave travel × Mountain definition × entry strictness (3 × 3 × 3 × 2 = 54) on every
cached ticker except SPY/QQQ/IWM, daily bars, next-open entry, the rule's own flip exit (mine.evaluate.simulate).
Writes mine/diamond_sweep.md (ranked table, per-knob marginals, screenshot anchors, conclusion) and mine/diamond_sweep.csv.
"""
from __future__ import annotations

import time
from pathlib import Path

import numpy as np
import pandas as pd

from . import data, diamond, evaluate, features, ta

HERE = Path(__file__).parent
SPLIT = pd.Timestamp("2024-01-01")
CONFIRM = (2, 3, 5)
MIN_MOVE = (5.0, 10.0, 15.0)
MOUNTAIN = ("a", "b", "c")
LEVELS = ("strict", "2of3")
TODAY = (3, 10.0, "a", "strict")
# ticker, bar size, panel, founders' bars (Monday of the week for weekly charts), window to list our diamonds in
ANCHORS = [("IREN", "W", "echo", [("blue", "2025-04-21", "2025-04-21"), ("pink", "2025-07-21", "2025-07-21")], "2025-02-01", "2025-10-01"),
           ("ONDS", "D", "echo", [("pink", "2025-10-08", "2025-10-10"), ("blue", "2025-11-07", "2025-11-07")], "2025-09-01", "2025-12-31"),
           ("SLNH", "W", "bravo", [("blue", "2025-09-15", "2025-09-22"), ("pink", "2025-11-10", "2025-11-17")], "2025-07-01", "2026-01-31")]
T0 = time.time()


def log(msg: str):
    print(f"[{time.time() - T0:6.1f}s] {msg}", flush=True)


def tickers() -> list[str]:
    return sorted(p.stem for p in (HERE / "cache").glob("*.csv") if p.stem not in ("SPY", "QQQ", "IWM"))


def variants(df: pd.DataFrame):
    """One states() call, then only the cheap loops per knob. Returns {(min_move, confirm): [n_blue_c, n_pink_c]}, {kind: mountain}, fixed cols."""
    s = diamond.states(df)
    h, l, c, v = df["high"], df["low"], df["close"], df["volume"]
    hype, flow = s["hype"], s["flow"]
    band_up = (s["mountain"] - (hype > 0).astype(int) - (flow > 0).astype(int) - s["above_gold"].astype(int)).astype(bool)
    mount = {k: diamond.mountain_score(k, hype, flow, s["above_gold"], band_up, v).astype("int8") for k in MOUNTAIN}
    dia = {}
    for mm in MIN_MOVE:
        echo, tango = diamond.diamond_engine(hype, s["hype_sig"], mm), diamond.diamond_engine(flow, s["flow_sig"], mm)
        for cb in CONFIRM:
            cs = [diamond.confirmed(d, h, l, c, cb) for d in (echo, tango, s["bravo"])]
            dia[(mm, cb)] = pd.DataFrame({"n_blue_c": sum((x == 1).astype("int8") for x in cs), "n_pink_c": sum((x == -1).astype("int8") for x in cs)})
    return dia, mount, s[["above_gold", "below_gold", "overheated"]]


def self_check(df: pd.DataFrame):
    """The derived path above must equal states(df, **knobs) computed directly — a cross-check that can fail differently."""
    dia, mount, _ = variants(df)
    for mm in MIN_MOVE:
        for cb in CONFIRM:
            for k in MOUNTAIN:
                full = diamond.states(df, min_move=mm, confirm_bars=cb, mountain=k)
                assert (full["n_blue_c"].values == dia[(mm, cb)]["n_blue_c"].values).all(), (mm, cb)
                assert (full["n_pink_c"].values == dia[(mm, cb)]["n_pink_c"].values).all(), (mm, cb)
                assert (full["mountain"].values == mount[k].values).all(), k


def load_universe():
    reg = features.regime()
    px, frames, dia_parts, mount_parts, fixed_parts = {}, [], {}, {k: [] for k in MOUNTAIN}, []
    ticks = tickers()
    for i, t in enumerate(ticks):
        df = data.fetch(t)
        if df is None or len(df) < 300:
            continue
        df = df.reset_index(drop=True)
        if not px:
            self_check(df)
            log(f"self-check passed on {t}: derived states == states(df, min_move, confirm_bars, mountain) for all 27 knob sets")
        base = pd.DataFrame({"ticker": t, "date": df["date"], "atr_pct": ta.atr(df, 14) / df["close"] * 100.0})
        base["regime_bull"] = base["date"].map(reg).eq(True)           # NaN (no SPY bar) → False, same as features.build
        dia, mount, fixed = variants(df)
        px[t] = df
        frames.append(base)
        for key, d in dia.items():
            dia_parts.setdefault(key, []).append(d)
        for k in MOUNTAIN:
            mount_parts[k].append(mount[k])
        fixed_parts.append(fixed)
        if i % 10 == 0:
            log(f"states {i}/{len(ticks)} ({t}, {len(df)} bars)")
    cat = lambda parts: pd.concat(parts, ignore_index=True)  # noqa: E731
    return cat(frames), px, {k: cat(v) for k, v in dia_parts.items()}, {k: cat(v) for k, v in mount_parts.items()}, cat(fixed_parts)


def rule(d: pd.DataFrame, m: pd.Series, fixed: pd.DataFrame, level: str):
    nb, npk, ag, bg, hot = d["n_blue_c"], d["n_pink_c"], fixed["above_gold"], fixed["below_gold"], fixed["overheated"]
    entry = (nb == 3) & (m == 4) & ag & ~hot if level == "strict" else (nb >= 2) & (m >= 3) & ag
    return entry, (npk >= 2) | (m <= 2) | bg


def score(tr: pd.DataFrame, wk_is: float, wk_oos: float) -> dict:
    tr = tr.copy()
    tr["date"] = pd.to_datetime(tr["date"])
    oos, ins = tr[tr["date"] >= SPLIT], tr[tr["date"] < SPLIT]
    so, si = evaluate.stats(oos, "flip", wk_oos), evaluate.stats(ins, "flip", wk_is)
    g = lambda s, k: s.get(k, np.nan) if s.get(k) is not None else np.nan  # noqa: E731
    return {"n_is": len(ins), "n_oos": len(oos), "oos_avg": g(so, "avg"), "oos_med": g(so, "med"), "oos_win": g(so, "win"), "oos_pf": g(so, "pf"),
            "oos_per_wk": g(so, "per_wk"), "oos_tick_pos": g(so, "tick_pos"), "oos_total": g(so, "total"),
            "oos_bars": round(float(oos["flip_bars"].mean()), 1) if len(oos) else np.nan, "is_avg": g(si, "avg"), "is_pf": g(si, "pf")}


def weekly(df: pd.DataFrame) -> pd.DataFrame:
    w = df.set_index("date").resample("W-FRI").agg({"open": "first", "high": "max", "low": "min", "close": "last", "volume": "sum"})
    return w.dropna().reset_index()


def anchor_lines(best: tuple) -> list[str]:
    out = ["Diamond placement depends only on `min_move` (the confirmation window and Mountain never move a diamond; Bravo diamonds "
           "depend on none of the knobs, so SLNH is a fixed reference). Weekly bars are pandas W-FRI resamples of the daily cache; they "
           "are listed by the Monday that starts the week, which is how TradingView labels weekly bars. Offset = our nearest same-colour "
           "diamond vs the founders' bar, in calendar days (0 = same bar).", ""]
    for t, tf, panel, targets, lo, hi in ANCHORS:
        df = data.fetch(t).reset_index(drop=True)
        frame = weekly(df) if tf == "W" else df
        out += [f"### {t} {'weekly' if tf == 'W' else 'daily'} {panel.capitalize()} — founders: " + ", ".join(f"{c} {a}" + (f"..{b}" if b != a else "") for c, a, b in targets), "",
                "| min_move | our diamonds in window | " + " | ".join(f"offset to {c} {a}" for c, a, _ in targets) + " |", "|---|---|" + "---|" * len(targets)]
        for mm in MIN_MOVE:
            s = diamond.states(frame, min_move=mm)
            d = s[panel]
            lab = frame["date"] - pd.Timedelta(days=4) if tf == "W" else frame["date"]
            fired = [(lab.iloc[i], int(d.iloc[i])) for i in np.where(d.values != 0)[0]]
            listed = [f"{'blue' if col == 1 else 'pink'} {dt.date()}" for dt, col in fired if lo <= str(dt.date()) <= hi]
            offs = []
            for col, a, b in targets:
                want = 1 if col == "blue" else -1
                a_, b_ = pd.Timestamp(a), pd.Timestamp(b)
                cands = [dt for dt, c_ in fired if c_ == want and abs((dt - a_).days) <= 120]
                if not cands:
                    offs.append("none within 120d")
                else:
                    near = min(cands, key=lambda dt: 0 if a_ <= dt <= b_ else min(abs((dt - a_).days), abs((dt - b_).days)))
                    off = 0 if a_ <= near <= b_ else ((near - b_).days if near > b_ else (near - a_).days)
                    offs.append(f"{off:+d}d ({near.date()})" if off else f"0 (same bar, {near.date()})")
            tag = " (default)" if mm == 10.0 else ""
            tag += " (best)" if mm == best[1] else ""
            out.append(f"| {mm:g}{tag} | {', '.join(listed) or 'none'} | " + " | ".join(offs) + " |")
        out.append("")
    return out


def main():
    base, px, dia, mount, fixed = load_universe()
    wk_oos = max(1.0, (base["date"].max() - SPLIT).days / 7.0)
    wk_is = max(1.0, (SPLIT - base["date"].min()).days / 7.0)
    log(f"{len(px)} tickers, {len(base)} ticker-days, {base['date'].min().date()} → {base['date'].max().date()}; OOS weeks {wk_oos:.1f}, IS weeks {wk_is:.1f}")
    rows = []
    n = 0
    for cb in CONFIRM:
        for mm in MIN_MOVE:
            for k in MOUNTAIN:
                for level in LEVELS:
                    ent, ex = rule(dia[(mm, cb)], mount[k], fixed, level)
                    tr = evaluate.simulate(base, px, ent, ex)
                    rows.append({"confirm": cb, "min_move": mm, "mountain": k, "entry": level, **score(tr, wk_is, wk_oos)})
                    n += 1
            log(f"combo {n}/54 done (confirm={cb}, min_move={mm:g}): last OOS n={rows[-1]['n_oos']} avg={rows[-1]['oos_avg']}")
    rng = np.random.RandomState(7)
    coin = pd.Series(rng.rand(len(base)) < 0.12, index=base.index)
    every10 = pd.Series(np.arange(len(base)) % 10 == 0, index=base.index)
    tr = evaluate.simulate(base, px, coin, ~coin.shift(10, fill_value=False) & ~coin.shift(9, fill_value=False) & every10)
    ctrl = {"confirm": "-", "min_move": "-", "mountain": "-", "entry": "RANDOM CONTROL", **score(tr, wk_is, wk_oos)}
    log("random control done")
    res = pd.DataFrame(rows).sort_values("oos_avg", ascending=False).reset_index(drop=True)
    res["thin"] = res["n_oos"] < 100
    res.insert(0, "rank", np.arange(1, len(res) + 1))
    best_row = res[~res["thin"]].iloc[0]
    best = (int(best_row["confirm"]), float(best_row["min_move"]), best_row["mountain"], best_row["entry"])
    today = res[(res["confirm"] == TODAY[0]) & (res["min_move"] == TODAY[1]) & (res["mountain"] == TODAY[2]) & (res["entry"] == TODAY[3])].iloc[0]
    key = lambda r: (r["confirm"], r["min_move"], r["mountain"], r["entry"])  # noqa: E731
    more = res[~res["thin"] & (res["oos_per_wk"] >= today["oos_per_wk"]) & (res["oos_avg"] > today["oos_avg"])]
    more_row = more.iloc[0] if len(more) else None
    both = res[(res["oos_avg"] > today["oos_avg"]) & (res["is_avg"] > today["is_avg"]) & ~res["thin"]]
    pd.concat([res, pd.DataFrame([ctrl])], ignore_index=True).to_csv(HERE / "diamond_sweep.csv", index=False)

    f = lambda v, d=2: "-" if pd.isna(v) else (f"{v:.{d}f}" if isinstance(v, float) else str(v))  # noqa: E731
    hdr = ["| rank | confirm | min_move | Mountain | entry | n IS | n OOS | OOS avg % | OOS med % | OOS win | OOS PF | OOS /wk | tickers+ | IS avg % | IS PF | note |",
           "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
    def line(r, note=""):
        return (f"| {r.get('rank', '-')} | {r['confirm']} | {f(r['min_move'], 0) if r['min_move'] != '-' else '-'} | {r['mountain']} | {r['entry']} | {r['n_is']} | {r['n_oos']} | "
                f"{f(r['oos_avg'])} | {f(r['oos_med'])} | {f(r['oos_win'], 3)} | {f(r['oos_pf'])} | {f(r['oos_per_wk'])} | {f(r['oos_tick_pos'])} | {f(r['is_avg'])} | {f(r['is_pf'])} | {note} |")
    def note(r):
        tags = ["THIN (<100 OOS)"] if r["thin"] else []
        if key(r) == TODAY:
            tags.append("TODAY")
        if key(r) == best:
            tags.append("BEST (non-thin)")
        if more_row is not None and key(r) == key(more_row):
            tags.append("MORE-SIGNALS pick")
        return ", ".join(tags)
    out = [f"# Diamond strict — calibration sweep ({len(px)} tickers, daily bars, next-open entry, flip exit)", "",
           f"Generated {pd.Timestamp.now():%Y-%m-%d %H:%M}. Universe: every cached ticker except SPY/QQQ/IWM. Data {base['date'].min().date()} → {base['date'].max().date()}. "
           f"In-sample = trades signalled before {SPLIT.date()} ({wk_is:.0f} weeks); out-of-sample = on/after ({wk_oos:.0f} weeks). "
           "Signals per week = OOS trades ÷ OOS weeks, across the whole universe.", "",
           "Knobs: **confirm** = bars allowed for the diamond bar's high/low to be closed through (2/3/5). **min_move** = wave travel required since "
           "the last diamond before a new Echo/Tango diamond may fire (5/10/15). **Mountain** — (a) today's count: hype>0, flow>0, close>gold top, "
           "band rising (EMA21>EMA55 and EMA21 rising); (b) stricter: hype>10, flow>10, close>gold top, band rising, and the score may only reach 4 when "
           "at least 2 of the 4 held on each of the last 3 bars (otherwise capped at 3); (c) as (a) with the band-rising condition replaced by 20-day "
           "average volume above its 50-day average. **entry** — strict: 3 confirmed blue & Mountain 4 & above gold & not overheated; 2of3: ≥2 confirmed "
           "blue & Mountain ≥3 & above gold. Exit (both): ≥2 confirmed pink, or Mountain ≤2, or close below the gold band.", "",
           f"Rows with fewer than 100 OOS trades are flagged THIN; 'best' below means the top non-thin row. Random-entry control (same flip-style exit): "
           f"OOS avg {f(ctrl['oos_avg'])} %, median {f(ctrl['oos_med'])} %, win {f(ctrl['oos_win'], 3)}, PF {f(ctrl['oos_pf'])}, n {ctrl['n_oos']}.", "",
           "## Ranked by out-of-sample avg % per trade", ""] + hdr
    out += [line(r, note(r)) for _, r in res.iterrows()]
    out += [line(ctrl, "random entries, exit ≈ every 10th bar"), ""]
    out += ["## Per-knob marginals (mean over all combos sharing the value, OOS)", "", "| knob | value | mean OOS avg % | mean OOS PF | mean OOS /wk | mean n OOS |", "|---|---|---|---|---|---|"]
    for knob in ("confirm", "min_move", "mountain", "entry"):
        for val, g in res.groupby(knob, sort=True):
            out.append(f"| {knob} | {val if knob != 'min_move' else f'{val:g}'} | {g['oos_avg'].mean():.2f} | {g['oos_pf'].mean():.2f} | {g['oos_per_wk'].mean():.2f} | {g['n_oos'].mean():.0f} |")
    out += ["", "## Today vs best", "", *hdr, line(today, "TODAY"), line(best_row, "BEST (non-thin)")]
    out += [line(more_row, "MORE-SIGNALS pick")] if more_row is not None else []
    out += ["", "## Calibration anchors (founders' screenshots)", ""]
    out += anchor_lines(best)
    d_avg, d_wk = best_row["oos_avg"] - today["oos_avg"], best_row["oos_per_wk"] - today["oos_per_wk"]
    marg = {knob: res.groupby(knob)["oos_avg"].mean().idxmax() for knob in ("confirm", "min_move", "mountain", "entry")}
    out += ["## What to change (plain English)", "",
            f"Today's parameters (confirm {TODAY[0]}, min_move {TODAY[1]:g}, Mountain {TODAY[2]}, {TODAY[3]}) rank {today['rank']} of 54 out-of-sample: "
            f"{f(today['oos_avg'])} % per trade, PF {f(today['oos_pf'])}, {f(today['oos_per_wk'])} signals/week ({today['n_oos']} OOS trades). "
            f"The best non-thin row (confirm {best[0]}, min_move {best[1]:g}, Mountain {best[2]}, {best[3]}) makes {f(best_row['oos_avg'])} % per trade, "
            f"PF {f(best_row['oos_pf'])}, {f(best_row['oos_per_wk'])} signals/week ({best_row['n_oos']} OOS trades): {d_avg:+.2f} points per trade and "
            f"{d_wk:+.2f} signals/week versus today. In-sample the same row makes {f(best_row['is_avg'])} % (PF {f(best_row['is_pf'])}) against today's "
            f"{f(today['is_avg'])} % (PF {f(today['is_pf'])}).",
            "", (f"If the aim is more signals rather than bigger ones: the best row that keeps at least today's signal rate is confirm {more_row['confirm']}, "
                 f"min_move {more_row['min_move']:g}, Mountain {more_row['mountain']}, {more_row['entry']} — {f(more_row['oos_avg'])} % per trade, PF {f(more_row['oos_pf'])}, "
                 f"{f(more_row['oos_per_wk'])} signals/week ({more_row['n_oos']} OOS trades); in-sample {f(more_row['is_avg'])} % (PF {f(more_row['is_pf'])})."
                 if more_row is not None else "No row beats today on both signal rate and avg % per trade."),
            "", f"Knob by knob, averaged over everything else, the OOS-best values are: confirm {marg['confirm']}, min_move {marg['min_move']:g}, "
            f"Mountain {marg['mountain']}, entry {marg['entry']}. The 2-of-3 entry averages {res[res['entry'] == '2of3']['oos_avg'].mean():.2f} % across its 27 rows "
            f"against {res[res['entry'] == 'strict']['oos_avg'].mean():.2f} % for strict. The random control makes {f(ctrl['oos_avg'])} % per trade with the same exit style, "
            "so only rows well above that carry information; a row that beats it by a point or two on a few hundred trades is inside the noise. "
            f"{len(both)} of the 54 rows beat today on avg % in BOTH samples with ≥100 OOS trades: " + ", ".join(f"({r['confirm']}, {r['min_move']:g}, {r['mountain']}, {r['entry']})" for _, r in both.iterrows()) + ".",
            "", f"Recommended change: keep min_move {marg['min_move']:g} and the strict entry (both are clear winners on the marginals), adopt Mountain {marg['mountain']}, "
            f"and pick the confirmation window by what the rule is for — {best[0]} bars for fewer, larger trades (the top row) or "
            + (f"{more_row['confirm']} bars for more of them (the more-signals pick)" if more_row is not None else "today's 3 bars") + ". The anchors below show whether "
            "the chosen min_move keeps the diamonds on the founders' bars; the other three knobs never move a diamond, only whether a trade is taken.", "",
            "Caveat: the 2024 split limits selection bias but does not remove it. 54 rows were scored on the same out-of-sample window and the best one was "
            "picked by that score, so its OOS number is optimistic; the in-sample column and the per-knob marginals (which average away single lucky cells) "
            "are the honest guides. Treat a change as real only if it wins in both samples and on the marginals, and re-check it on the next quarter's bars "
            "before trusting it. The anchors section shows separately whether a change moves the diamonds toward the founders' bars — that is a different "
            "question from return, and both matter.", ""]
    (HERE / "diamond_sweep.md").write_text("\n".join(out), encoding="utf-8")
    log(f"wrote {HERE / 'diamond_sweep.md'} and {HERE / 'diamond_sweep.csv'}")
    print("\n".join(hdr + [line(r, note(r)) for _, r in res.head(8).iterrows()] + [line(ctrl, "random control")]))


if __name__ == "__main__":
    main()
