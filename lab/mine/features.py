"""Per-bar feature table for the mining pass: every Dragon and Diamond panel reading + forward outcomes.

build(tickers) → DataFrame (one row per ticker-day) with
  outcomes (entered at the NEXT open, the way the founder trades): fwd_1/3/5/10/20 (% close h bars later vs next open),
  mfe_10 / mae_10 (best / worst excursion % over 10 bars), big_up (≥ +10 % within 10 bars before −5 %), big_dn (mirror),
  regime_bull (SPY above its 200-day line), and the panel states from mine.dragon / mine.diamond.
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pandas as pd

from . import data, diamond, dragon, ta

HERE = Path(__file__).parent


def outcomes(df: pd.DataFrame) -> pd.DataFrame:
    o = pd.DataFrame(index=df.index)
    entry = df["open"].shift(-1)                      # fill at the next open
    for h in (1, 3, 5, 10, 20):
        o[f"fwd_{h}"] = (df["close"].shift(-h) / entry - 1.0) * 100.0
    hi10 = df["high"].shift(-1).rolling(10).max().shift(-9)   # highest high over bars t+1..t+10
    lo10 = df["low"].shift(-1).rolling(10).min().shift(-9)
    o["mfe_10"] = (hi10 / entry - 1.0) * 100.0
    o["mae_10"] = (lo10 / entry - 1.0) * 100.0
    # "spike": +10 % reached within 10 bars before a −5 % drawdown from entry (path-dependent, computed per bar)
    c = df["close"].values
    hgh = df["high"].values
    lw = df["low"].values
    op = df["open"].values
    n = len(c)
    big_up = np.zeros(n, dtype=bool)
    big_dn = np.zeros(n, dtype=bool)
    for i in range(n - 11):
        e = op[i + 1]
        up_hit = dn_hit = False
        for k in range(i + 1, i + 11):
            if not up_hit and not dn_hit:
                if lw[k] <= e * 0.95:
                    dn_hit = True
                if hgh[k] >= e * 1.10:
                    up_hit = True
        big_up[i] = up_hit and not dn_hit
        up2 = dn2 = False
        for k in range(i + 1, i + 11):
            if not up2 and not dn2:
                if hgh[k] >= e * 1.05:
                    up2 = True
                if lw[k] <= e * 0.90:
                    dn2 = True
        big_dn[i] = dn2 and not up2
    o["big_up"] = big_up
    o["big_dn"] = big_dn
    return o


def regime() -> pd.Series:
    spy = data.fetch("SPY")
    if spy is None:
        return pd.Series(dtype=bool)
    s = spy.set_index("date")["close"]
    return (s > s.rolling(200).mean()).rename("regime_bull")


def build(tickers: list[str], min_bars: int = 300) -> pd.DataFrame:
    reg = regime()
    frames = []
    for i, t in enumerate(tickers):
        df = data.fetch(t)
        if df is None or len(df) < min_bars:
            continue
        df = df.reset_index(drop=True)
        try:
            f = pd.concat([outcomes(df), dragon.states(df).add_prefix("dr_"), diamond.states(df).add_prefix("di_")], axis=1)
        except Exception as e:  # noqa: BLE001
            print(f"  {t}: {type(e).__name__}: {str(e)[:100]}", file=sys.stderr)
            continue
        f.insert(0, "date", df["date"])
        f.insert(0, "ticker", t)
        f["atr_pct"] = (ta.atr(df, 14) / df["close"] * 100.0)
        f["regime_bull"] = f["date"].map(reg).fillna(False).astype(bool) if len(reg) else True
        frames.append(f)
        if i % 10 == 0:
            print(f"  features {i}/{len(tickers)} ({t}, {len(df)} bars)", flush=True)
    out = pd.concat(frames, ignore_index=True)
    out.to_parquet(HERE / "features.parquet") if _has_parquet() else out.to_csv(HERE / "features.csv", index=False)
    return out


def _has_parquet() -> bool:
    try:
        import pyarrow  # noqa: F401
        return True
    except ImportError:
        return False


def load() -> pd.DataFrame:
    p = HERE / "features.parquet"
    if p.exists():
        return pd.read_parquet(p)
    return pd.read_csv(HERE / "features.csv", parse_dates=["date"])


if __name__ == "__main__":
    ticks = sys.argv[1:] or [p.stem for p in (HERE / "cache").glob("*.csv")]
    f = build(ticks)
    print(f"{len(f)} ticker-days, {f['ticker'].nunique()} tickers → mine/features.*")
