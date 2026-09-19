"""Python port of the Dragon (Homily / DannyTrades) panels — the same formulas as tv-indicator-clones/homily and
the strategy twin, so what is mined here is what the TradingView scripts show.

states(df) → DataFrame of per-bar panel readings:
  j, ss_bull, ss_fresh, ss_confirm, ribbon_bull, hole_state (0 in force / 1 broke up / -1 broke down), hole_age,
  poc_ratio (close/POC - 1), poc_bull, te_slope (v7 - v7[1] in ATR), te_bull, whales, retail, retail_shrinking,
  macd_dif (in ATR), macd_bull, rsi9/14/24, rsi_up, n_bull (0-7), n_bear (0-7)
"""
from __future__ import annotations

import math

import numpy as np
import pandas as pd

from . import ta


def kdj(df: pd.DataFrame):
    hh9 = df["high"].rolling(9).max()
    ll9 = df["low"].rolling(9).min()
    rsv = ((df["close"] - ll9) / (hh9 - ll9).replace(0, np.nan) * 100.0).fillna(50.0)
    k = ta.sma_cn(rsv, 3)
    d = ta.sma_cn(k, 3)
    return 3.0 * k - 2.0 * d


def hole(df: pd.DataFrame, k: float = 1.4, piv: int = 4):
    """Volatility hole state machine (twin version): MA10 ± k·ATR14 at each confirmed pivot; 2 closes to break."""
    atr14 = ta.atr(df, 14).values
    ma10 = ta.sma(df["close"], 10).values
    ph = ta.pivothigh(df["high"], piv, piv).values
    pl = ta.pivotlow(df["low"], piv, piv).values
    c = df["close"].values
    state = np.zeros(len(c), dtype=int)
    age = np.zeros(len(c), dtype=int)
    up = dn = np.nan
    st = 0
    a = 0
    for i in range(len(c)):
        if (not np.isnan(ph[i]) or not np.isnan(pl[i])) and i - piv >= 0:
            up = ma10[i - piv] + k * atr14[i - piv]
            dn = ma10[i - piv] - k * atr14[i - piv]
            st = 0
            a = 0
        if st == 0 and not np.isnan(up) and i > 0:
            if c[i] > up and c[i - 1] > up:
                st = 1
            elif c[i] < dn and c[i - 1] < dn:
                st = -1
        a += 1
        state[i] = st
        age[i] = a
    return pd.Series(state, index=df.index), pd.Series(age, index=df.index)


def chips(df: pd.DataFrame, win: int = 120, bin_pct: float = 0.5):
    """Rolling cost distribution (MCDX / CD): whales % (chips ≥ 4 % below close), retail % (chips ≥ 4 % above),
    POC price (bin with most chips). O(bars × bins) with a dict of log-price bins."""
    ln_step = math.log(1.0 + bin_pct / 100.0)
    lo = df["low"].values
    hi = df["high"].values
    vol = df["volume"].values.astype(float)
    c = df["close"].values
    n = len(c)
    bins: dict[int, float] = {}
    whales = np.full(n, np.nan)
    retail = np.full(n, np.nan)
    poc = np.full(n, np.nan)

    def add(i, sign):
        if vol[i] <= 0 or lo[i] <= 0 or hi[i] < lo[i]:
            return
        bl = int(math.floor(math.log(lo[i]) / ln_step))
        bh = int(math.floor(math.log(hi[i]) / ln_step))
        per = sign * vol[i] / (bh - bl + 1)
        for b in range(bl, bh + 1):
            bins[b] = bins.get(b, 0.0) + per

    for i in range(n):
        add(i, +1)
        if i >= win:
            add(i - win, -1)
        tot = below = above = 0.0
        vmax = 0.0
        pmax = np.nan
        lo_t = c[i] * 0.96
        hi_t = c[i] * 1.04
        for b, v in bins.items():
            if v <= 0:
                continue
            tot += v
            px = math.exp((b + 0.5) * ln_step)
            if px < lo_t:
                below += v
            if px > hi_t:
                above += v
            if v > vmax:
                vmax = v
                pmax = px
        if tot > 0:
            whales[i] = below / tot * 100.0
            retail[i] = above / tot * 100.0
            poc[i] = pmax
    w = pd.Series(whales, index=df.index).rolling(3).mean()
    r = pd.Series(retail, index=df.index).rolling(3).mean()
    return w, r, pd.Series(poc, index=df.index)


def states(df: pd.DataFrame) -> pd.DataFrame:
    out = pd.DataFrame(index=df.index)
    c = df["close"]
    atr14 = ta.atr(df, 14)
    j = kdj(df)
    out["j"] = j
    out["ss_bull"] = j > 50
    fresh_red = ta.crossover(j, pd.Series(50.0, index=df.index))
    fresh_yel = ta.crossunder(j, pd.Series(50.0, index=df.index))
    out["ss_fresh"] = fresh_red
    red_hi = df["high"].where(fresh_red).ffill()
    red_lo = df["low"].where(fresh_red).ffill()
    yel_lo = df["low"].where(fresh_yel).ffill()
    yel_hi = df["high"].where(fresh_yel).ffill()
    out["ss_confirm"] = out["ss_bull"] & (c > red_hi) & ~(c < red_lo)
    out["ss_confirm_s"] = ~out["ss_bull"] & (c < yel_lo) & ~(c > yel_hi)
    src = (c + df["low"]) / 2.0
    out["ribbon_bull"] = ta.ema(src, 8) > ta.ema(ta.ema(src, 20), 8)
    hs, ha = hole(df)
    out["hole_state"] = hs
    out["hole_age"] = ha
    w, r, poc = chips(df)
    out["whales"] = w
    out["retail"] = r
    out["retail_shrinking"] = r < r.shift(3)
    out["poc_ratio"] = c / poc - 1.0
    out["poc_bull"] = (c > poc) & (c.shift(1) > poc.shift(1))
    out["poc_bear"] = (c < poc) & (c.shift(1) < poc.shift(1))
    ohlc4 = (df["open"] + df["high"] + df["low"] + c) / 4.0
    trip = (ta.ema(ohlc4, 21) + ta.ema(ohlc4, 34) + ta.ema(ohlc4, 68)) / 3.0
    v7 = ta.linreg(trip, 6)
    out["te_slope"] = (v7 - v7.shift(1)) / atr14
    out["te_bull"] = v7 > v7.shift(1)
    dif = ta.ema(c, 12) - ta.ema(c, 26)
    out["macd_dif"] = dif / atr14
    out["macd_bull"] = dif > 0
    r1, r2, r3 = ta.rsi(c, 9), ta.rsi(c, 14), ta.rsi(c, 24)
    out["rsi9"], out["rsi14"], out["rsi24"] = r1, r2, r3
    out["rsi_up"] = (r1 > r1.shift(1)) & (r2 > r2.shift(1)) & (r3 > r3.shift(1)) & (r2 > 50)
    out["rsi_dn"] = (r1 < r1.shift(1)) & (r2 < r2.shift(1)) & (r3 < r3.shift(1)) & (r2 < 50)
    out["n_bull"] = (out["ss_confirm"].astype(int) + out["ribbon_bull"].astype(int) + (hs == 1).astype(int) + out["poc_bull"].astype(int)
                     + out["te_bull"].astype(int) + ((w >= 50) & out["retail_shrinking"]).astype(int) + (out["macd_bull"] & out["rsi_up"]).astype(int))
    out["n_bear"] = (out["ss_confirm_s"].astype(int) + (~out["ribbon_bull"]).astype(int) + (hs == -1).astype(int) + out["poc_bear"].astype(int)
                     + (~out["te_bull"]).astype(int) + (w < 50).astype(int) + (~out["macd_bull"] & out["rsi_dn"]).astype(int))
    return out
