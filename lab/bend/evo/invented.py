"""Self-written building blocks — measurements of price movement borrowed from physics, biology and other fields.
Each block: name → (formula from bars ≤ i, rationale). Appended to bend/data/matrix_D.pkl (merge on ticker, bar)."""
import sys, time, numpy as np, pandas as pd
sys.path.insert(0, ".")
from mine import ta

RATIONALE = {
    "kinetic20": "physics · kinetic energy of the move: 20-bar velocity squared, with its sign (ATR/bar)²",
    "p_mass20": "physics · momentum = mass × velocity: 20-bar velocity weighted by log dollar liquidity",
    "impulse10": "physics · impulse: net signed volume over 10 bars ÷ total volume (−1 all selling … +1 all buying)",
    "damping": "physics · damping ratio: size of the last swing ÷ the swing before it (<1 oscillation decaying, >1 expanding)",
    "spring20": "physics · spring displacement from the 20-day VWAP, in units of 20-day return noise",
    "phase": "physics · phase-space angle atan2(acceleration, velocity): 0 = steady rise, ±π = steady fall, +π/2 = turning up",
    "resonance20": "physics · lag-1 autocorrelation of daily returns over 20 bars (negative = choppy, positive = persistent)",
    "vratio50": "physics · variance ratio: var(5-day returns) ÷ 5·var(1-day) over 50 bars (>1 trending, <1 mean-reverting)",
    "fractal30": "geometry · Katz fractal dimension of the 30-bar path (1 = straight line, →2 = pure noise)",
    "impact20": "traffic flow · price movement per unit volume over 20 bars relative to its 100-bar norm (velocity ÷ density)",
    "strain": "geology · accumulated strain: bars in a squeeze × volatility compression (20-bar ATR ÷ 100-bar ATR inverted)",
    "predator_prey10": "biology · buyers ÷ sellers: up-volume ÷ down-volume over 10 bars (log)",
    "pp_drift": "biology · 5-bar drift of the predator–prey ratio (population cycle turning)",
    "recovery": "biology · bars since the last shock (a down day of ≥ 2 ATR)",
    "resilience": "biology · recovery since the last shock: (close − shock close) ÷ ATR",
    "stable20": "biology · homeostasis: bars of the last 20 spent within ±1 ATR of the 20-day median",
    "flatten": "biology · growth-curve flattening: 60-bar slope minus 5-bar slope (large = a rising curve losing pace)",
    "r0": "epidemiology · spreading ratio: up-days in the last 5 ÷ up-days in the 5 before (+1 each)",
    "metabolic": "biology · metabolic rate: volume ratio × today's range in ATR",
    "aggression10": "market microstructure · net aggression: close position in the range (−1..+1) weighted by volume over 10 bars",
    "snr20": "signal processing · signal-to-noise: 20-bar net move ÷ (daily-change std × √20)",
    "thrust5": "rocketry · thrust-to-weight: up-volume ÷ total volume over 5 bars",
    "asym20": "path shape · max drawup ÷ max drawdown over 20 bars (log)",
    "time_sym": "swing timing · bars since the swing low ÷ bars since the swing high (log)",
    "gap_fill10": "behaviour · share of the last 10 opening gaps that filled the same day",
    "ou_speed60": "stochastic processes · Ornstein–Uhlenbeck reversion speed over 60 bars (more negative = faster pull to the mean)",
    "vwap_dev50": "cost basis · distance of the close from the 50-day VWAP in ATR",
}


def invented(df: pd.DataFrame, piv: int = 4) -> pd.DataFrame:
    c, o, h, l, v = df.close, df.open, df.high, df.low, df.volume.astype(float)
    atr = ta.atr(df, 14); dc = c.diff(); n = len(c); f = {}
    vel20 = (c - c.shift(20)) / 20 / atr; vel5 = (c - c.shift(5)) / 5 / atr
    f["kinetic20"] = vel20 * vel20.abs()
    f["p_mass20"] = vel20 * np.log10((c * v).rolling(20).mean().replace(0, np.nan)) / 8
    sgn = np.sign(dc); f["impulse10"] = (sgn * v).rolling(10).sum() / v.rolling(10).sum().replace(0, np.nan)
    ph = ta.pivothigh(h, piv, piv); pl = ta.pivotlow(l, piv, piv)
    piv_px = ph.fillna(pl); seq = piv_px.dropna(); legs = seq.diff().abs()
    leg_now = legs.reindex(df.index).ffill(); leg_prev = legs.shift(1).reindex(df.index).ffill()
    f["damping"] = leg_now / leg_prev.replace(0, np.nan)
    tp = (h + l + c) / 3; vwap20 = (tp * v).rolling(20).sum() / v.rolling(20).sum().replace(0, np.nan)
    f["spring20"] = (c - vwap20) / (c.pct_change().rolling(20).std() * c).replace(0, np.nan)
    f["phase"] = np.arctan2(vel5 - vel20, vel20)
    r = c.pct_change(); f["resonance20"] = r.rolling(20).corr(r.shift(1))
    f["vratio50"] = c.pct_change(5).rolling(50).var() / (5 * r.rolling(50).var()).replace(0, np.nan)
    L = dc.abs().rolling(30).sum(); d = (h.rolling(30).max() - l.rolling(30).min())
    f["fractal30"] = np.log10(30) / (np.log10(30) + np.log10((d / L).replace(0, np.nan)))
    imp = (h - l).rolling(20).sum() / v.rolling(20).sum().replace(0, np.nan); f["impact20"] = imp / imp.rolling(100).mean().replace(0, np.nan)
    rng20 = (h.rolling(20).max() - l.rolling(20).min()) / (h.rolling(50).max() - l.rolling(50).min()); sq = (rng20 < 0.5).astype(int)
    f["strain"] = sq.groupby((sq == 0).cumsum()).cumsum() * (ta.atr(df, 100) / ta.atr(df, 20)).replace(0, np.nan)
    upv = v.where(dc > 0, 0.0); dnv = v.where(dc < 0, 0.0)
    pp = np.log((upv.rolling(10).sum() + 1) / (dnv.rolling(10).sum() + 1)); f["predator_prey10"] = pp; f["pp_drift"] = pp - pp.shift(5)
    shock = (dc / atr) <= -2.0; idx = pd.Series(np.arange(n), index=df.index, dtype=float)
    shock_bar = idx.where(shock).ffill(); shock_px = c.where(shock).ffill()
    f["recovery"] = idx - shock_bar; f["resilience"] = (c - shock_px) / atr
    med20 = c.rolling(20).median(); f["stable20"] = ((c - med20).abs() <= atr).astype(int).rolling(20).sum()
    x60 = np.arange(60); v60 = np.convolve(c.values, (x60 - x60.mean())[::-1], mode="valid") / ((x60 - x60.mean()) ** 2).sum()
    lr60 = pd.Series(np.concatenate([np.full(59, np.nan), v60]), index=df.index) / atr
    x5 = np.arange(5); v5 = np.convolve(c.values, (x5 - x5.mean())[::-1], mode="valid") / ((x5 - x5.mean()) ** 2).sum()
    lr5 = pd.Series(np.concatenate([np.full(4, np.nan), v5]), index=df.index) / atr
    f["flatten"] = lr60 - lr5
    up = (dc > 0).astype(int); f["r0"] = (up.rolling(5).sum() + 1) / (up.shift(5).rolling(5).sum() + 1)
    f["metabolic"] = (v / v.rolling(20).mean().replace(0, np.nan)) * (h - l) / atr
    pos = ((c - l) - (h - c)) / (h - l).replace(0, np.nan); f["aggression10"] = (pos * v).rolling(10).sum() / v.rolling(10).sum().replace(0, np.nan)
    f["snr20"] = (c - c.shift(20)) / (dc.rolling(20).std() * np.sqrt(20)).replace(0, np.nan)
    f["thrust5"] = upv.rolling(5).sum() / v.rolling(5).sum().replace(0, np.nan)
    cummax = c.rolling(20).max(); cummin = c.rolling(20).min()
    f["asym20"] = np.log(((c.rolling(20).apply(lambda w: (w / np.minimum.accumulate(w) - 1).max(), raw=True)) + 0.01) / ((c.rolling(20).apply(lambda w: (1 - w / np.maximum.accumulate(w)).max(), raw=True)) + 0.01))
    ph_bar = idx.where(ph.notna()).ffill() - piv; pl_bar = idx.where(pl.notna()).ffill() - piv
    f["time_sym"] = np.log((idx - pl_bar + 1) / (idx - ph_bar + 1))
    gap_up = o > c.shift(1); filled = np.where(gap_up, l <= c.shift(1), h >= c.shift(1)); f["gap_fill10"] = pd.Series(filled.astype(float), index=df.index).rolling(10).mean()
    dev = c.shift(1) - c.rolling(60).mean().shift(1); f["ou_speed60"] = dc.rolling(60).cov(dev) / dev.rolling(60).var().replace(0, np.nan)
    vwap50 = (tp * v).rolling(50).sum() / v.rolling(50).sum().replace(0, np.nan); f["vwap_dev50"] = (c - vwap50) / atr
    return pd.DataFrame(f)


if __name__ == "__main__":
    M = pd.read_pickle("bend/data/matrix_D.pkl"); t0 = time.time(); parts = []
    for i, (t, g) in enumerate(M.groupby("ticker")):
        try: df = pd.read_csv(f"mine/cache/{t}.csv", parse_dates=["date"])
        except Exception: continue
        b = invented(df).iloc[g.bar.values]; b["ticker"] = t; b["bar"] = g.bar.values; parts.append(b)
        if i % 400 == 0: print(i, f"{time.time()-t0:.0f}s", flush=True)
    G = pd.concat(parts); M = M.drop(columns=[c for c in G.columns if c in M.columns and c not in ("ticker", "bar")]).merge(G, on=["ticker", "bar"])
    M.to_pickle("bend/data/matrix_D.pkl"); print(M.shape, f"{time.time()-t0:.0f}s")
    import json; mean = json.load(open("bend/evo/meaning.json")); mean.update(RATIONALE); json.dump(mean, open("bend/evo/meaning.json", "w"), indent=1, ensure_ascii=False)
