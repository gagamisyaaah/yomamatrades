"""Python port of the Diamond (Startup.io) panels — same formulas as tv-indicator-clones/startupio and the twin.

states(df) → per-bar readings:
  hype, hype_sig, flow, flow_sig, echo (1 blue / -1 pink diamond this bar / 0), tango, bravo, st_echo/st_tango/st_bravo
  (last diamond colour, ±1), c_echo/c_tango/c_bravo (confirmed: diamond bar's high/low broken within 3 bars),
  n_blue, n_pink (last-state counts), n_blue_c, n_pink_c (confirmed counts), mountain (0-4), above_gold, below_gold,
  gold_dist (close vs band top in ATR), purple_dist (ATR to the purple pivot high above, NaN = none), green_dist,
  bubble, overheated, silver_pos (0-1 position inside the 100-bar 5th..95th percentile range)
"""
from __future__ import annotations

import numpy as np
import pandas as pd

from . import ta


def diamond_engine(w: pd.Series, s: pd.Series, min_move: float = 10.0):
    """Blue when the wave crosses above its signal after travelling ≥ min_move since the last diamond; pink mirror."""
    up = ta.crossover(w, s).values
    dn = ta.crossunder(w, s).values
    wv = w.values
    out = np.zeros(len(wv), dtype=int)
    hi = lo = np.nan
    for i in range(len(wv)):
        x = wv[i]
        if np.isnan(x):
            continue
        hi = x if np.isnan(hi) else max(hi, x)
        lo = x if np.isnan(lo) else min(lo, x)
        travel = hi - lo
        if up[i] and travel >= min_move:
            out[i] = 1
            hi = lo = x
        elif dn[i] and travel >= min_move:
            out[i] = -1
            hi = lo = x
    return pd.Series(out, index=w.index)


def last_state(d: pd.Series) -> pd.Series:
    return d.replace(0, np.nan).ffill().fillna(0).astype(int)


def confirmed(d: pd.Series, high: pd.Series, low: pd.Series, close: pd.Series, bars: int = 3) -> pd.Series:
    """±1 once the diamond bar's high (blue) / low (pink) is closed through within `bars` bars; 0 if invalidated / none."""
    dv, h, l, c = d.values, high.values, low.values, close.values
    out = np.zeros(len(dv), dtype=int)
    st = 0
    lvl = np.nan
    since = 0
    ok = False
    for i in range(len(dv)):
        if dv[i] != 0:
            st = dv[i]
            lvl = h[i] if st == 1 else l[i]
            since = 0
            ok = False
        else:
            since += 1
        if st == 1 and not ok and c[i] > lvl:
            ok = True
        if st == -1 and not ok and c[i] < lvl:
            ok = True
        if not ok and since > bars:
            st = 0
        out[i] = st if ok else 0
    return pd.Series(out, index=d.index)


def mountain_score(kind: str, hype: pd.Series, flow: pd.Series, above_top: pd.Series, band_up: pd.Series, volume: pd.Series) -> pd.Series:
    """0-4 Mountain. "a": today's count (hype>0, flow>0, close>gold top, band rising). "b": stricter — hype>10, flow>10, same
    other two, and 4 only when ≥2 conditions held on each of the last 3 bars (else capped at 3). "c": as "a" with the
    band-rising condition replaced by 20-day volume above its 50-day average."""
    if kind == "a":
        return (hype > 0).astype(int) + (flow > 0).astype(int) + above_top.astype(int) + band_up.astype(int)
    if kind == "b":
        m = (hype > 10).astype(int) + (flow > 10).astype(int) + above_top.astype(int) + band_up.astype(int)
        persist = (m >= 2) & (m.shift(1) >= 2) & (m.shift(2) >= 2)
        return m.where(persist | (m < 4), 3)
    if kind == "c":
        vol_up = ta.sma(volume.astype(float), 20) > ta.sma(volume.astype(float), 50)
        return (hype > 0).astype(int) + (flow > 0).astype(int) + above_top.astype(int) + vol_up.astype(int)
    raise ValueError(f"unknown mountain kind {kind!r}")


def states(df: pd.DataFrame, min_move: float = 10.0, confirm_bars: int = 3, mountain: str = "a") -> pd.DataFrame:
    """Defaults = the panel as shipped; min_move / confirm_bars / mountain ("a"|"b"|"c") are the calibration knobs (see diamond_sweep.py)."""
    out = pd.DataFrame(index=df.index)
    c, h, l, v = df["close"], df["high"], df["low"], df["volume"]
    atr14 = ta.atr(df, 14)
    signed = v.where(c > c.shift(1), 0.0) - v.where(c < c.shift(1), 0.0).fillna(0.0) * 0  # placeholder replaced below
    signed = np.where(c > c.shift(1), v, np.where(c < c.shift(1), -v, 0.0))
    signed = pd.Series(signed, index=df.index)
    vol_e = ta.ema(v.astype(float), 10)
    sv_e = ta.ema(signed, 10)
    hype = ta.ema((100.0 * sv_e / vol_e.replace(0, np.nan)).fillna(0.0) / 2.0, 3)
    hype_sig = ta.ema(hype, 5)
    flow = ta.ema(ta.mfi(df, 14) - 50.0, 3)
    flow_sig = ta.ema(flow, 5)
    e1, e2, e3 = ta.ema(c, 21), ta.ema(c, 34), ta.ema(c, 55)
    gold_top = pd.concat([e1, e2, e3], axis=1).max(axis=1)
    gold_bot = pd.concat([e1, e2, e3], axis=1).min(axis=1)
    out["hype"], out["hype_sig"], out["flow"], out["flow_sig"] = hype, hype_sig, flow, flow_sig
    echo = diamond_engine(hype, hype_sig, min_move)
    tango = diamond_engine(flow, flow_sig, min_move)
    bravo = pd.Series(np.where(ta.crossover(c, gold_top) & (hype > hype_sig), 1, np.where(ta.crossunder(c, gold_bot) & (hype < hype_sig), -1, 0)), index=df.index)
    out["echo"], out["tango"], out["bravo"] = echo, tango, bravo
    st_e, st_t, st_b = last_state(echo), last_state(tango), last_state(bravo)
    out["st_echo"], out["st_tango"], out["st_bravo"] = st_e, st_t, st_b
    out["n_blue"] = (st_e == 1).astype(int) + (st_t == 1).astype(int) + (st_b == 1).astype(int)
    out["n_pink"] = (st_e == -1).astype(int) + (st_t == -1).astype(int) + (st_b == -1).astype(int)
    c_e, c_t, c_b = confirmed(echo, h, l, c, confirm_bars), confirmed(tango, h, l, c, confirm_bars), confirmed(bravo, h, l, c, confirm_bars)
    out["c_echo"], out["c_tango"], out["c_bravo"] = c_e, c_t, c_b
    out["n_blue_c"] = (c_e == 1).astype(int) + (c_t == 1).astype(int) + (c_b == 1).astype(int)
    out["n_pink_c"] = (c_e == -1).astype(int) + (c_t == -1).astype(int) + (c_b == -1).astype(int)
    out["mountain"] = mountain_score(mountain, hype, flow, c > gold_top, (e1 > e3) & (e1 > e1.shift(1)), v)
    out["above_gold"] = c > gold_top
    out["below_gold"] = c < gold_bot
    out["gold_dist"] = (c - gold_top) / atr14
    ph = ta.pivothigh(h, 5, 5).ffill()
    pl = ta.pivotlow(l, 5, 5).ffill()
    purple = ph.where(ph > c)
    green = pl.where(pl < c)
    out["purple_dist"] = (purple - c) / atr14
    out["green_dist"] = (c - green) / atr14
    bubble = ((c - ta.ema(c, 200)) / atr14).clip(lower=0) + (hype > 25).astype(int) + (flow > 25).astype(int)
    out["bubble"] = bubble
    out["overheated"] = bubble >= 6.0
    hi95 = h.rolling(100).quantile(0.95)
    lo05 = l.rolling(100).quantile(0.05)
    out["silver_pos"] = (c - lo05) / (hi95 - lo05).replace(0, np.nan)
    return out
