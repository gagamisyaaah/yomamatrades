"""Portfolio-level simulation of the Dragon C signal and its D/E/F tiers: one account, real slots, vol-targeted sizing.

usage: python3.12 -m mine.portfolio [--capital 100000] [--f 0.01] [--n 10] [--commission 0.0005] [--split 2024-01-01]
writes mine/portfolio.md (founder-readable) and mine/portfolio.csv (every grid row).

Rules of the game (identical for every run):
  entry     the signal is read at the close of bar i; the fill is the NEXT open. Sizing uses the ATR of the signal bar
            and the account's equity at the previous close — nothing from the future.
  exit      close of the first bar after entry whose hole state is −1 (the ledger's `flip` return, next open → exit close)
  sizing    notional = equity × f / (atr_pct / 100), capped at 20 % of equity   (f = 0.01: a 1-ATR move ≈ 1 % of equity)
  slots     at most N open positions; a position holds its slot from its entry day through its exit day inclusive, so a
            slot freed by today's exit is usable tomorrow. Several signals on one day → highest ATR first.
            One position per ticker at a time (a repeat signal while the name is held is skipped).
  leverage  none: the sum of open notionals may not exceed equity, else the signal is skipped
  costs     0.05 % of notional per side
"""
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd

from . import data, features
from .evaluate import simulate

HERE = Path(__file__).parent
CAP_SHARE = 0.20                      # no single position above 20 % of equity
GRID_F = (0.005, 0.01, 0.02)
GRID_N = (5, 10, 20)
TIER_TEXT = {"C": "hole broke up + whales ≥ 50 + TE up + not overheated + Mountain ≥ 3 + above gold",
             "D": "C, and the hole broke up within the last 3 bars (fresh)",
             "E": "D, and at least 6 of the 7 Dragon panels are bullish",
             "F": "C, only on names whose ATR is ≥ 5 % of price",
             "G": "C with the market guard: no entries while SPY is below its 200-day line, exit when it crosses below"}


def tiers(d: pd.DataFrame) -> dict[str, tuple[pd.Series, pd.Series]]:
    """tier → (entry_state, exit_state) on the feature table."""
    entry = (d["dr_hole_state"] == 1) & (d["dr_whales"] >= 50) & d["dr_te_bull"] & ~d["di_overheated"] & (d["di_mountain"] >= 3) & d["di_above_gold"]
    exit_state = d["dr_hole_state"] == -1
    fresh = (d["dr_hole_state"] == 1) & (d.groupby("ticker")["dr_hole_state"].shift(3) != 1)
    return {"C": (entry, exit_state),
            "D": (entry & fresh, exit_state),
            "E": (entry & fresh & (d["dr_n_bull"] >= 6), exit_state),
            "F": (entry & (d["atr_pct"] >= 5), exit_state),
            "G": (entry & d["regime_bull"], exit_state | ~d["regime_bull"])}


def load(name: str = "features") -> tuple[pd.DataFrame, dict[str, pd.DataFrame]]:
    """Feature table + price frames, built the way evaluate.main() does (only tickers whose bars match the feature rows)."""
    df = features.load(name).sort_values(["ticker", "date"]).reset_index(drop=True)
    df["date"] = pd.to_datetime(df["date"])
    spy = data.fetch("SPY")
    if spy is not None:                                   # the stored regime column was built before SPY was available
        s_ = spy.set_index("date")["close"]
        df["regime_bull"] = df["date"].map(s_ > s_.rolling(200).mean()).fillna(True).astype(bool)
    px = {t: data.fetch(t) for t in df["ticker"].unique()}
    px = {t: p.reset_index(drop=True) for t, p in px.items() if p is not None and len(p) == (df["ticker"] == t).sum()}
    return df, px


def trades(df: pd.DataFrame, px: dict[str, pd.DataFrame], entry: pd.Series, exit_state: pd.Series) -> pd.DataFrame:
    """simulate() ledger → one row per signalled trade with real entry/exit dates (bar offsets mapped on the ticker's own bars)."""
    tr = simulate(df, px, entry.fillna(False), exit_state.fillna(False))
    tr["date"] = pd.to_datetime(tr["date"])
    fresh = (df["dr_hole_state"] == 1) & (df.groupby("ticker")["dr_hole_state"].shift(3) != 1)
    q = pd.DataFrame({"ticker": df["ticker"], "date": pd.to_datetime(df["date"]),
                      "quality": 2.0 * fresh.astype(float) + (df["dr_n_bull"] >= 6).astype(float) + 0.5 * (df["di_mountain"] == 4).astype(float)
                                 + df["atr_pct"].clip(upper=10) / 10.0 + 0.25 * (df["dr_whales"] >= 60).astype(float)})
    tr = tr.merge(q, on=["ticker", "date"], how="left")
    tr["quality"] = tr["quality"].fillna(0.0)
    parts = []
    for t, g in tr.groupby("ticker", sort=False):
        p = px[t]
        dates, opn = p["date"].values, p["open"].values
        i = np.searchsorted(dates, g["date"].values)                          # signal bar
        assert (dates[i] == g["date"].values).all(), f"{t}: signal date not on a bar"
        j = i + g["flip_bars"].values                                          # exit bar (close)
        parts.append(pd.DataFrame({"ticker": t, "signal": g["date"].values, "entry": dates[i + 1], "exit": dates[j],
                                   "entry_px": opn[i + 1], "atr_pct": g["atr_pct"].values, "ret": g["flip"].values / 100.0, "quality": g["quality"].values}))
    return pd.concat(parts, ignore_index=True)


def run(tr: pd.DataFrame, closes: pd.DataFrame, capital: float, f: float, n_max: int, commission: float, start: pd.Timestamp | None = None, policy: str = "atr") -> dict:
    """Day-by-day account over the trade list. closes = calendar × ticker close matrix (forward-filled)."""
    cal = closes.index
    if start is not None:
        cal = cal[cal >= start]
        tr = tr[tr["entry"] >= start]
    C = closes.loc[cal].values
    col = {t: k for k, t in enumerate(closes.columns)}
    tr = tr.assign(entry_k=cal.get_indexer(tr["entry"]), exit_k=cal.get_indexer(tr["exit"]))
    if policy == "quality":
        tr = tr.sort_values(["entry_k", "quality", "atr_pct"], ascending=[True, False, False])
    elif policy == "random":
        tr = tr.assign(_r=np.random.RandomState(11).rand(len(tr))).sort_values(["entry_k", "_r"])
    else:
        tr = tr.sort_values(["entry_k", "atr_pct"], ascending=[True, False])
    by_day: dict[int, list] = {}
    for s in tr.itertuples(index=False):
        by_day.setdefault(s.entry_k, []).append(s)                             # highest ATR first within a day
    cash = equity = float(capital)
    open_pos: list[dict] = []
    eq = np.empty(len(cal))
    n_open = np.zeros(len(cal), dtype=int)
    taken = 0
    skip = {"no_slot": 0, "held": 0, "no_headroom": 0, "atr_zero": 0}
    for k in range(len(cal)):
        # at the open: new entries, sized on last close's equity; today's exits still hold their slot
        for s in by_day.get(k, ()):
            if s.atr_pct <= 0:
                skip["atr_zero"] += 1
                continue
            if any(p["ticker"] == s.ticker for p in open_pos):
                skip["held"] += 1
                continue
            if len(open_pos) >= n_max:
                skip["no_slot"] += 1
                continue
            notional = min(equity * f / (s.atr_pct / 100.0), CAP_SHARE * equity)
            if sum(p["notional"] for p in open_pos) + notional > equity:
                skip["no_headroom"] += 1
                continue
            cash -= notional * commission
            open_pos.append({"ticker": s.ticker, "col": col[s.ticker], "entry_px": s.entry_px, "notional": notional, "ret": s.ret, "exit_k": s.exit_k})
            taken += 1
        n_open[k] = len(open_pos)
        # at the close: settle today's exits at the ledger return, mark the rest at today's close
        still, mtm = [], 0.0
        for p in open_pos:
            if p["exit_k"] == k:
                cash += p["notional"] * p["ret"] - p["notional"] * (1.0 + p["ret"]) * commission
            else:
                still.append(p)
                mtm += p["notional"] * (C[k, p["col"]] / p["entry_px"] - 1.0)
        open_pos = still
        equity = cash + mtm
        eq[k] = equity
    e = pd.Series(eq, index=cal)
    span_days = (cal[-1] - cal[0]).days
    year_end = e.groupby(e.index.year).last()
    yearly = year_end / year_end.shift(1).fillna(capital) - 1.0
    return {"final": float(e.iloc[-1]), "cagr": (e.iloc[-1] / capital) ** (365.25 / span_days) - 1.0, "max_dd": float((1.0 - e / e.cummax()).max()),
            "taken": taken, "signalled": int(len(tr)), "per_week": taken / (span_days / 7.0), "avg_open": float(n_open.mean()),
            **{f"skip_{k}": v for k, v in skip.items()}, **{f"y{y}": float(v) for y, v in yearly.items()}}


def money(x: float) -> str:
    return f"${x:,.0f}"


def pct(x: float, signed: bool = True) -> str:
    if x is None or (isinstance(x, float) and np.isnan(x)):
        return "—"
    return f"{x * 100:+.1f} %" if signed else f"{x * 100:.1f} %"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--capital", type=float, default=100000)
    ap.add_argument("--f", type=float, default=0.01, help="target: a 1-ATR move = f × equity")
    ap.add_argument("--n", type=int, default=10, help="max concurrent positions")
    ap.add_argument("--commission", type=float, default=0.0005, help="per side, as a fraction of notional")
    ap.add_argument("--split", default="2024-01-01", help="out-of-sample start")
    ap.add_argument("--features", default="features", help="feature table name (features | features_broad)")
    ap.add_argument("--tag", default="", help="suffix for the output files")
    args = ap.parse_args()
    split = pd.Timestamp(args.split)

    df, px = load(args.features)
    closes = pd.concat({t: p.set_index("date")["close"] for t, p in px.items()}, axis=1).sort_index().ffill()
    ledgers = {name: trades(df, px, ent, ex) for name, (ent, ex) in tiers(df).items()}
    rng = np.random.RandomState(7)
    ledgers["random"] = trades(df, px, pd.Series(rng.rand(len(df)) < 0.1, index=df.index), pd.Series(np.arange(len(df)) % 25 == 0, index=df.index))

    grid = sorted({(f, n) for f in GRID_F for n in GRID_N} | {(args.f, args.n)})
    rows = []
    for tier in ("C", "D", "E", "F", "G"):
        for f, n in grid:
            for sample, start in (("full", None), ("oos", split)):
                rows.append({"tier": tier, "f": f, "N": n, "sample": sample, **run(ledgers[tier], closes, args.capital, f, n, args.commission, start)})
    for sample, start in (("full", None), ("oos", split)):
        rows.append({"tier": "random", "f": 0.01, "N": 10, "sample": sample, **run(ledgers["random"], closes, args.capital, 0.01, 10, args.commission, start)})
    pol_rows = []
    for tier in ("C", "G"):
        for f, n in ((0.005, 10), (0.01, 10), (0.005, 20)):
            for policy in ("quality", "atr", "random"):
                for sample, start in (("full", None), ("oos", split)):
                    pol_rows.append({"tier": tier, "f": f, "N": n, "policy": policy, "sample": sample, **run(ledgers[tier], closes, args.capital, f, n, args.commission, start, policy)})
    pol = pd.DataFrame(pol_rows)
    pol.to_csv(HERE / f"portfolio_policy{args.tag}.csv", index=False)
    res = pd.DataFrame(rows)
    res.to_csv(HERE / f"portfolio{args.tag}.csv", index=False)

    years = sorted(int(c[1:]) for c in res.columns if c.startswith("y"))
    first, last = closes.index[0].date(), closes.index[-1].date()
    head = res[(res["tier"] == "C") & (res["f"] == args.f) & (res["N"] == args.n)].set_index("sample")
    hf, ho = head.loc["full"], head.loc["oos"]
    out = [f"# Dragon C as a portfolio — {money(args.capital)}, {len(px)} tickers, {first} → {last}", "",
           "One account, traded by the rules below. The signal is read at a day's close, the position is bought at the next morning's open, "
           f"and it is sold at the close of the first day the hole flips to −1. Size: a 1-ATR move is worth {args.f:.1%} of the account (never more than "
           f"{CAP_SHARE:.0%} of the account in one name). At most {args.n} names at once; extra signals are simply not taken, biggest-ATR first. "
           f"No leverage: the open positions never add up to more than the account. {args.commission:.2%} commission each way. Profits compound.", "",
           f"Tiers: **C** = {TIER_TEXT['C']}. **D** = {TIER_TEXT['D']}. **E** = {TIER_TEXT['E']}. **F** = {TIER_TEXT['F']}. **G** = {TIER_TEXT['G']}.", "",
           f"## The headline account: tier C, f = {args.f:.1%}, max {args.n} positions", "",
           "| year | account at year end | return in the year |", "|---|---|---|"]
    eq = args.capital
    for y in years:
        r = hf[f"y{y}"]
        eq *= 1 + r
        note = " (from Sep 19)" if y == years[0] else (f" (to {last})" if y == years[-1] else "")
        out.append(f"| {y}{note} | {money(eq)} | {pct(r)} |")
    out += ["", f"- Ended at **{money(hf['final'])}** — {pct(hf['cagr'])} a year compounded.",
            f"- Worst drawdown from a peak: **{pct(-hf['max_dd'])}**.",
            f"- Trades actually taken: **{hf['taken']:,} of {hf['signalled']:,} signalled** — {hf['per_week']:.1f} per week; skipped {hf['skip_no_slot']:,} for no free slot, "
            f"{hf['skip_held']:,} because the name was already held, {hf['skip_no_headroom']:,} for the no-leverage cap, {hf['skip_atr_zero']:,} with a zero ATR.",
            f"- On an average day {hf['avg_open']:.1f} positions were open (of {args.n} slots). Note the binding limit at f = {args.f:.1%}: a typical signal has an ATR near 5 %, "
            f"so almost every position is sized at the {CAP_SHARE:.0%} cap and {int(1 / CAP_SHARE)} names fill the account before the slots run out — that is why the "
            f"N = 10 and N = 20 rows below barely differ (more slots only help when f is small enough that positions are under the cap).",
            f"- **Starting fresh on {split.date()} with {money(args.capital)}**: {money(ho['final'])} by {last} ({pct(ho['cagr'])} a year), worst drawdown {pct(-ho['max_dd'])}, "
            f"{ho['taken']:,} of {ho['signalled']:,} signals taken ({ho['per_week']:.1f} per week), {ho['avg_open']:.1f} open on an average day.", "",
            "## Every tier × sizing × slot count", "",
            f"Full history {first} → {last}; the OOS columns restart the same account with {money(args.capital)} on {split.date()}. f = the share of the account a 1-ATR move is worth.", "",
            "| tier | f | N | final | CAGR | max DD | taken / signalled | per week | avg open | 2022 | OOS final | OOS CAGR | OOS max DD | OOS per week |",
            "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
    full = res[res["sample"] == "full"].set_index(["tier", "f", "N"])
    oos = res[res["sample"] == "oos"].set_index(["tier", "f", "N"])
    for key, r in full.iterrows():
        o = oos.loc[key]
        tier, f, n = key
        out.append(f"| {tier} | {f:.1%} | {n} | {money(r['final'])} | {pct(r['cagr'])} | {pct(-r['max_dd'])} | {r['taken']:,} / {r['signalled']:,} | {r['per_week']:.1f} | {r['avg_open']:.1f} | "
                   f"{pct(r.get('y2022'))} | {money(o['final'])} | {pct(o['cagr'])} | {pct(-o['max_dd'])} | {o['per_week']:.1f} |")
    out += ["", "The **random** row is the control: entries on random days (10 % of all ticker-days), held to a fixed 25-bar clock, same account rules, f = 1.0 %, N = 10. "
            "A tier only means something if it beats this row on the same columns.", "",
            "## Return by calendar year, every tier (f = 1.0 %, N = 10)", "",
            "| tier | " + " | ".join(str(y) for y in years) + " |", "|---|" + "---|" * len(years)]
    for tier in ("C", "D", "E", "F", "G", "random"):
        r = full.loc[(tier, 0.01, 10)]
        out.append(f"| {tier} | " + " | ".join(pct(r.get(f"y{y}")) for y in years) + " |")
    out += ["", "## Which signals get the slots — the founder's thesis test", "",
            "When more names signal than the account can hold, the policy decides. **quality** = take the best setups first (fresh hole break, ≥ 6 of 7 panels, "
            "Mountain maxed, higher ATR, whales ≥ 60); **atr** = biggest mover first; **random** = whatever comes. Same account rules otherwise.", "",
            "| tier | f | N | policy | final | CAGR | max DD | taken | per week | 2022 | OOS final | OOS CAGR | OOS max DD |", "|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
    pf = pol[pol["sample"] == "full"].set_index(["tier", "f", "N", "policy"])
    po = pol[pol["sample"] == "oos"].set_index(["tier", "f", "N", "policy"])
    for key, r in pf.iterrows():
        o = po.loc[key]
        tier, f, n, policy = key
        out.append(f"| {tier} | {f:.1%} | {n} | {policy} | {money(r['final'])} | {pct(r['cagr'])} | {pct(-r['max_dd'])} | {r['taken']:,} | {r['per_week']:.1f} | {pct(r.get('y2022'))} | {money(o['final'])} | {pct(o['cagr'])} | {pct(-o['max_dd'])} |")
    out += ["", "## Caveats", "",
            "- **Read the random row first.** Buying these names on random days already compounds at a high rate with a deep drawdown, because the universe is today's "
            "watch-list of names that have already run (crypto miners, quantum, AI). A big part of every tier's return is the universe, not the signal; the signal's own "
            "worth is the gap between its row and the random row, and that gap is modest next to the size of the swings.",
            "- The trade list comes from `mine.evaluate.simulate`: entries on the rising edge of the rule, one ledger row per signal. A name can re-signal while it is still held "
            "(the rule flickers inside one hole cycle); the account takes one position per name, so those repeats are counted as *skipped: held*, not as slots.",
            "- Sizing divides by the signal bar's ATR; bars with ATR = 0 (bad or flat data) are skipped and counted.",
            "- Exits are the ledger's flip exit only — no stop-loss. A position that never flips is closed at the last bar in the data, at that close.",
            "- Daily equity marks open positions at each day's close; a name that is halted on a day carries its last close forward.",
            "- Names that listed late (e.g. CRWV 2025, RDDT 2024) contribute nothing before their first bar; the early years are traded on fewer names than the later ones.",
            "- Yahoo/Nasdaq daily bars, adjusted as the source delivers them; no survivorship correction — the universe is the founder's current watch-list."]
    (HERE / f"portfolio{args.tag}.md").write_text("\n".join(out), encoding="utf-8")
    print("\n".join(out))


if __name__ == "__main__":
    main()
