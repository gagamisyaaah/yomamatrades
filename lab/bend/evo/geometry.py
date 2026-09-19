"""Geometric blocks: the shape of the move, computed from confirmed swing pivots and path shape, from bars ≤ i.
Appends to bend/data/matrix_D.pkl (merge on ticker, bar)."""
import sys, time, numpy as np, pandas as pd
sys.path.insert(0, ".")
from mine import ta


def geometry(df: pd.DataFrame, piv: int = 4) -> pd.DataFrame:
    c, h, l = df.close, df.high, df.low; atr = ta.atr(df, 14); n = len(c)
    ph = ta.pivothigh(h, piv, piv); pl = ta.pivotlow(l, piv, piv)              # confirmed at i (pivot bar = i-4)
    idx = pd.Series(np.arange(n), index=df.index, dtype=float)
    ph_px = ph.ffill(); pl_px = pl.ffill()
    ph_bar = idx.where(ph.notna()).ffill() - piv; pl_bar = idx.where(pl.notna()).ffill() - piv
    ph_prev = ph.dropna().shift(1).reindex(df.index).ffill(); pl_prev = pl.dropna().shift(1).reindex(df.index).ffill()
    hh = (ph_px > ph_prev).astype(float).where(ph_prev.notna()); hl = (pl_px > pl_prev).astype(float).where(pl_prev.notna())
    # last three swings on each side → structure score −3..+3 (higher highs and higher lows count up)
    ph_seq = ph.dropna(); hh_seq = (ph_seq > ph_seq.shift(1)).astype(float).replace(0, -1.0).rolling(3).sum()
    pl_seq = pl.dropna(); hl_seq = (pl_seq > pl_seq.shift(1)).astype(float).replace(0, -1.0).rolling(3).sum()
    struct = (hh_seq.reindex(df.index).ffill().fillna(0) + hl_seq.reindex(df.index).ffill().fillna(0))
    up_leg = ph_bar > pl_bar                      # last swing was up (high is more recent than low)
    leg = (ph_px - pl_px) / atr
    retrace = np.where(up_leg, (ph_px - c) / (ph_px - pl_px).replace(0, np.nan), (c - pl_px) / (ph_px - pl_px).replace(0, np.nan))
    f = {"d_ph": (c - ph_px) / atr, "d_pl": (c - pl_px) / atr, "bars_ph": idx - ph_bar, "bars_pl": idx - pl_bar, "hh": hh, "hl": hl, "struct": struct,
         "leg_atr": leg, "leg_bars": (ph_bar - pl_bar).abs(), "retrace": pd.Series(retrace, index=df.index), "up_leg": up_leg.astype(float),
         "bars_52w_hi": idx - idx.where(h == h.rolling(252).max()).ffill()}
    d = c.diff().abs()
    for m in (5, 10, 20, 50):
        f[f"eff{m}"] = (c - c.shift(m)).abs() / d.rolling(m).sum().replace(0, np.nan)                       # path efficiency
        f[f"slope{m}"] = (c - c.shift(m)) / m / atr                                                          # velocity, ATR per bar
    for m in (5, 20, 60):                                                                                # regression slope per bar in ATR (closed form)
        x = np.arange(m); xm = x.mean(); den = ((x - xm) ** 2).sum()
        v = np.convolve(c.values, (x - xm)[::-1], mode="valid") / den
        f[f"lr{m}"] = pd.Series(np.concatenate([np.full(m - 1, np.nan), v]), index=df.index) / atr
    f["accel"] = f["lr5"] - f["lr20"]
    rng20 = (h.rolling(20).max() - l.rolling(20).min()) / (h.rolling(50).max() - l.rolling(50).min())
    sq = (rng20 < 0.5).astype(int); f["squeeze_bars"] = sq.groupby((sq == 0).cumsum()).cumsum()
    f["vel_ratio"] = f["slope5"] / f["slope20"].replace(0, np.nan)
    return pd.DataFrame(f)


if __name__ == "__main__":
    M = pd.read_pickle("bend/data/matrix_D.pkl"); t0 = time.time(); parts = []
    for i, (t, g) in enumerate(M.groupby("ticker")):
        try: df = pd.read_csv(f"mine/cache/{t}.csv", parse_dates=["date"])
        except Exception: continue
        b = geometry(df).iloc[g.bar.values]; b["ticker"] = t; b["bar"] = g.bar.values; parts.append(b)
        if i % 400 == 0: print(i, f"{time.time()-t0:.0f}s", flush=True)
    G = pd.concat(parts); M = M.drop(columns=[c for c in G.columns if c in M.columns and c not in ("ticker", "bar")]).merge(G, on=["ticker", "bar"])
    M.to_pickle("bend/data/matrix_D.pkl"); print(M.shape, f"{time.time()-t0:.0f}s")
