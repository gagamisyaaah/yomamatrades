"""Blocks ported from the measurements Jev ranked highest in the public-script corpus (own implementations, not copies):
volume flow (VFI), OBV oscillator, Weis-wave volume ratio, squeeze + squeeze momentum, SuperTrend distance/direction/age,
anchored VWAP from the last swing low and high, bars since a volume-confirmed breakout. Appended to the matrix atomically."""
import os, sys, time, numpy as np, pandas as pd
sys.path.insert(0, ".")
from mine import ta

RATIONALE = {
    "vfi20": "volume flow · Katsanos VFI: volume that moved price beyond a noise cutoff, capped, summed over 20 bars ÷ average volume",
    "obv_osc20": "volume flow · OBV minus its 20-day EMA, in units of 20-day volume",
    "weis_ratio": "volume waves · volume of the current same-direction wave ÷ volume of the previous wave (log)",
    "sqz_on": "squeeze · Bollinger(20,2) inside Keltner(20,1.5): 1 when compressed",
    "sqz_mom20": "squeeze momentum · regression of close minus the mid of (20-bar range mid, 20-day SMA), in ATR",
    "st_dist": "SuperTrend(10,3) · distance of the close from the trailing line in ATR (positive = above)",
    "st_dir": "SuperTrend(10,3) · direction (1 up, 0 down)",
    "st_age": "SuperTrend(10,3) · bars since the last flip",
    "avwap_pl": "anchored VWAP · distance of the close from the VWAP anchored at the last swing low, in ATR",
    "avwap_ph": "anchored VWAP · distance of the close from the VWAP anchored at the last swing high, in ATR",
    "bo_bars20": "breakout · bars since the close exceeded the prior 20-bar high on above-average volume",
}


def scouted(df: pd.DataFrame, piv: int = 4) -> pd.DataFrame:
    c, o, h, l, v = df.close, df.open, df.high, df.low, df.volume.astype(float)
    atr = ta.atr(df, 14); n = len(c); f = {}
    tp = (h + l + c) / 3
    inter = np.log(tp) - np.log(tp.shift(1)); vinter = inter.rolling(30).std(); cutoff = 0.2 * vinter * c
    vave = v.rolling(50).mean().shift(1); vc = np.minimum(v, vave * 2.5); mf = tp - tp.shift(1)
    vcp = pd.Series(np.where(mf > cutoff, vc, np.where(mf < -cutoff, -vc, 0.0)), index=df.index)
    f["vfi20"] = (vcp.rolling(20).sum() / vave.replace(0, np.nan)).rolling(3).mean()
    obv = (np.sign(c.diff()) * v).cumsum(); f["obv_osc20"] = (obv - ta.ema(obv, 20)) / v.rolling(20).sum().replace(0, np.nan)
    d = np.sign(c.diff()).replace(0, np.nan).ffill(); wave_id = (d != d.shift(1)).cumsum()
    wv = v.groupby(wave_id).cumsum(); wave_tot = v.groupby(wave_id).transform("sum"); prev_tot = wave_tot.groupby(wave_id).first().shift(1).reindex(wave_id.values).values
    f["weis_ratio"] = np.log((wv + 1) / (pd.Series(prev_tot, index=df.index) + 1))
    m20, s20 = c.rolling(20).mean(), c.rolling(20).std(); kc = ta.atr(df, 20) * 1.5
    f["sqz_on"] = ((m20 - 2 * s20 > m20 - kc) & (m20 + 2 * s20 < m20 + kc)).astype(float)
    mid = ((h.rolling(20).max() + l.rolling(20).min()) / 2 + m20) / 2; dev = c - mid
    x = np.arange(20); xm = x.mean(); den = ((x - xm) ** 2).sum(); slope = np.convolve(dev.fillna(0).values, (x - xm)[::-1], mode="valid") / den
    f["sqz_mom20"] = pd.Series(np.concatenate([np.full(19, np.nan), slope]), index=df.index) * 20 / atr
    # SuperTrend(10, 3)
    a10 = ta.atr(df, 10).values; hl2 = ((h + l) / 2).values; cv = c.values
    up = hl2 - 3 * a10; dn = hl2 + 3 * a10; st = np.full(n, np.nan); dirn = np.ones(n); fu = up.copy(); fd = dn.copy()
    for i in range(1, n):
        fu[i] = max(up[i], fu[i - 1]) if cv[i - 1] > fu[i - 1] else up[i]
        fd[i] = min(dn[i], fd[i - 1]) if cv[i - 1] < fd[i - 1] else dn[i]
        if dirn[i - 1] == 1:
            dirn[i] = 1 if cv[i] > fd[i - 1] or cv[i] > fu[i] else (-1 if cv[i] < fu[i] else 1)
            dirn[i] = -1 if cv[i] < fu[i] else 1
        else:
            dirn[i] = 1 if cv[i] > fd[i] else -1
        st[i] = fu[i] if dirn[i] == 1 else fd[i]
    stS = pd.Series(st, index=df.index); f["st_dist"] = (c - stS) / atr; f["st_dir"] = (pd.Series(dirn, index=df.index) > 0).astype(float)
    flip = pd.Series(dirn, index=df.index).diff().ne(0); idx = pd.Series(np.arange(n), index=df.index, dtype=float); f["st_age"] = idx - idx.where(flip).ffill()
    ph = ta.pivothigh(h, piv, piv); pl = ta.pivotlow(l, piv, piv)
    for name, pv in (("avwap_pl", pl), ("avwap_ph", ph)):
        anchor = (pv.notna()).cumsum(); pvsum = (tp * v).groupby(anchor).cumsum(); vsum = v.groupby(anchor).cumsum()
        f[name] = (c - pvsum / vsum.replace(0, np.nan)) / atr
    bo = (c > c.shift(1).rolling(20).max()) & (v > v.rolling(20).mean()); f["bo_bars20"] = idx - idx.where(bo).ffill()
    return pd.DataFrame(f)


if __name__ == "__main__":
    M = pd.read_pickle("bend/data/matrix_D.pkl"); t0 = time.time(); parts = []
    for i, (t, g) in enumerate(M.groupby("ticker")):
        try: df = pd.read_csv(f"mine/cache/{t}.csv", parse_dates=["date"])
        except Exception: continue
        b = scouted(df).iloc[g.bar.values]; b["ticker"] = t; b["bar"] = g.bar.values; parts.append(b)
        if i % 400 == 0: print(i, f"{time.time()-t0:.0f}s", flush=True)
    G = pd.concat(parts); M = M.drop(columns=[c for c in G.columns if c in M.columns and c not in ("ticker", "bar")]).merge(G, on=["ticker", "bar"])
    M.to_pickle("bend/data/matrix_D.tmp.pkl"); os.replace("bend/data/matrix_D.tmp.pkl", "bend/data/matrix_D.pkl"); print(M.shape, f"{time.time()-t0:.0f}s")
    import json; mean = json.load(open("bend/evo/meaning.json")); mean.update(RATIONALE); json.dump(mean, open("bend/evo/meaning.json", "w"), indent=1, ensure_ascii=False)
