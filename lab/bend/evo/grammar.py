"""A grammar of measurements: primitive × window × transform, generated systematically (thousands of blocks) so the
evolution searches the space of movement measurements instead of a hand-picked list.

primitives (per bar, from OHLCV): return, log-range, body, close-position-in-range, volume ratio, signed volume,
  path efficiency, velocity (ATR/bar), acceleration, distance to the window's mean / high / low (ATR), pivot distances,
  realised vol, volume-weighted return, up-day share, gap
windows: 3 5 8 13 21 34 55 89 144 233 (Fibonacci) — each primitive at every window it makes sense for
transforms: level · change over 5 · z-score vs 250 · percentile vs 250
Names: g_<primitive>_<window>_<transform>. Stored as float32.
"""
from __future__ import annotations

import numpy as np
import pandas as pd
import sys
sys.path.insert(0, ".")
from mine import ta  # noqa: E402

WINDOWS = [3, 5, 8, 13, 21, 34, 55, 89, 144, 233]


def base_series(df: pd.DataFrame) -> dict:
    c, o, h, l, v = df.close, df.open, df.high, df.low, df.volume.astype(float)
    atr = ta.atr(df, 14); r = c.pct_change(); dc = c.diff()
    out = {}
    for w in WINDOWS:
        out[f"ret_{w}"] = c.pct_change(w) * 100
        out[f"vel_{w}"] = (c - c.shift(w)) / w / atr
        out[f"eff_{w}"] = (c - c.shift(w)).abs() / dc.abs().rolling(w).sum().replace(0, np.nan)
        out[f"dmean_{w}"] = (c - c.rolling(w).mean()) / atr
        out[f"dhigh_{w}"] = (c - h.rolling(w).max()) / atr
        out[f"dlow_{w}"] = (c - l.rolling(w).min()) / atr
        out[f"pos_{w}"] = (c - l.rolling(w).min()) / (h.rolling(w).max() - l.rolling(w).min()).replace(0, np.nan)
        out[f"rvol_{w}"] = r.rolling(w).std() * 100
        out[f"rng_{w}"] = (h.rolling(w).max() - l.rolling(w).min()) / atr
        out[f"volr_{w}"] = v / v.rolling(w).mean().replace(0, np.nan)
        out[f"svol_{w}"] = (np.sign(dc) * v).rolling(w).sum() / v.rolling(w).sum().replace(0, np.nan)
        out[f"upsh_{w}"] = (dc > 0).astype(float).rolling(w).mean()
        out[f"clv_{w}"] = ((c - l) / (h - l).replace(0, np.nan)).rolling(w).mean()
        out[f"body_{w}"] = ((c - o) / atr).rolling(w).mean()
        out[f"vwret_{w}"] = (r * v).rolling(w).sum() / v.rolling(w).sum().replace(0, np.nan) * 100
        out[f"acc_{w}"] = out[f"vel_{w}"] - out[f"vel_{w}"].shift(w)
        out[f"gapsum_{w}"] = ((o - c.shift(1)) / atr).rolling(w).sum()
        out[f"atrr_{w}"] = ta.atr(df, max(2, w // 2)) / ta.atr(df, w * 2).replace(0, np.nan) if w >= 5 else atr / ta.atr(df, 50)
    return out


def grammar(df: pd.DataFrame) -> pd.DataFrame:
    base = base_series(df); f = {}
    for k, s in base.items():
        f[f"g_{k}"] = s
        f[f"g_{k}_chg5"] = s - s.shift(5)
        m, sd = s.rolling(250).mean(), s.rolling(250).std().replace(0, np.nan)
        f[f"g_{k}_z250"] = (s - m) / sd
        f[f"g_{k}_pct250"] = s.rolling(250).rank(pct=True)
    return pd.DataFrame(f).astype(np.float32)


if __name__ == "__main__":
    import time
    df = pd.read_csv("mine/cache/NVDA.csv", parse_dates=["date"]); t0 = time.time(); g = grammar(df)
    print(g.shape, f"{time.time()-t0:.2f}s per ticker;", g.memory_usage().sum() // 1024, "KB")
