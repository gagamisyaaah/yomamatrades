"""Round-3 blocks: the top of Jev's 342-model longlist (bend/data/models_jev2.csv), own implementations. Merged atomically."""
import os, sys, time, numpy as np, pandas as pd
sys.path.insert(0, ".")
from mine import ta

RATIONALE = {
    "amihud20": "economics · Amihud illiquidity: |return| per dollar traded over 20 bars, relative to its 250-bar median",
    "win_mom20": "statistics · winsorised momentum: 20-bar return with the two largest daily moves removed, %",
    "capit5": "psychology · capitulation within the last 5 bars: a ≥3 ATR down day on ≥4× volume closing in the bottom quarter",
    "euph5": "psychology · euphoria within the last 5 bars: a ≥3 ATR up day on ≥4× volume closing in the top quarter",
    "dd_dur60": "stochastic · drawdown duration: bars since the 60-bar high",
    "du_dur60": "stochastic · drawup duration: bars since the 60-bar low",
    "bubble": "economics · bubble: log distance above the 250-bar trend line times the sign of acceleration",
    "deadband20": "control · dead band: bars of the last 20 within ±0.5 ATR of the 20-day mean",
    "cant20": "architecture · cantilever: close minus the 20-day VWAP, in ATR",
    "remission": "medicine · bars since the last ≥2 ATR down day during which no lower low was made",
    "retreat": "operations research · retreat: close below the 10-bar low on above-average volume (1/0)",
    "quantile250": "statistics · percentile of the close among the last 250 closes",
    "local_time50": "stochastic · local time: bars of the last 50 within ±0.25 ATR of the 50-day mean",
    "d_hi250": "psychology · anchoring: distance to the 250-bar high in ATR", "d_lo250": "psychology · anchoring: distance to the 250-bar low in ATR",
    "overshoot20": "control · overshoot: excursion beyond the 20-day mean since the last crossing, in ATR",
    "foundation60": "architecture · foundation: touches of the 60-bar low zone (within 0.5 ATR) in the last 60 bars",
    "env20": "signal · envelope: 20-bar range in ATR",
    "pid_i20": "control · PID integral: sum of the distance to the 50-day mean over 20 bars, in ATR",
    "pid_d5": "control · PID derivative: 5-bar change of the distance to the 50-day mean, in ATR",
    "kurt60": "signal · excess kurtosis of daily returns over 60 bars", "skew60": "signal · skewness of daily returns over 60 bars",
    "tail250": "extreme value · tail ratio: 95th-percentile |return| over the median |return| across 250 bars",
    "bandwagon": "social · consecutive up-days with rising volume",
    "kyle60": "economics · Kyle's lambda: slope of daily change (ATR) on signed volume ratio over 60 bars",
    "red_queen": "evolution · 20-bar return minus what the 250-bar trend would have given, in ATR",
    "inertia": "mechanics · size of the current same-direction run in ATR",
    "sg_slope11": "signal · Savitzky–Golay derivative over 11 bars, in ATR per bar",
    "price_disc10": "economics · price discovery: share of the 10-bar path made in opening gaps",
    "info_ratio20": "economics · information ratio: 20-bar return over the daily-return std",
    "work10": "mechanics · work: volume-weighted candle bodies over 10 bars, in ATR",
    "crit_mass": "social · critical mass: volume above 2× average for 3 consecutive bars (1/0)",
    "mad20": "statistics · median absolute deviation of daily changes over 20 bars, in ATR",
    "pot60": "extreme value · peaks over threshold: count of |daily change| > 2 ATR in 60 bars",
    "roll60": "economics · Roll spread: √(−cov(Δc_t, Δc_t−1)) over 60 bars, in ATR (0 when covariance is positive)",
    "anneal20": "materials · annealing: 20-bar slope of daily range (falling) times slope of volume (rising), sign-preserving",
    "siege20": "operations research · siege: the last 20 bars inside a 3-ATR range with volume declining (1/0)",
    "vratio20": "stochastic · variance ratio: var(20-day returns) ÷ 20·var(1-day) over 100 bars",
    "regime_age": "stochastic · hidden regime age: bars since ATR(5)/ATR(50) last crossed 1",
    "mi60": "information · mutual information of return sign and volume direction over 60 bars (bits)",
}


def longlist(df: pd.DataFrame) -> pd.DataFrame:
    c, o, h, l, v = df.close, df.open, df.high, df.low, df.volume.astype(float)
    atr = ta.atr(df, 14); dc = c.diff(); r = c.pct_change(); n = len(c); idx = pd.Series(np.arange(n), index=df.index, dtype=float); f = {}
    dv = (c * v).replace(0, np.nan); am = (r.abs() / dv).rolling(20).mean(); f["amihud20"] = am / am.rolling(250).median().replace(0, np.nan)
    f["win_mom20"] = (r.rolling(20).sum() - r.rolling(20).apply(lambda w: np.sort(np.abs(w))[-2:].sum() * np.sign(w[np.argsort(np.abs(w))[-2:]]).mean() if len(w) == 20 else 0, raw=True)) * 100
    dca = dc / atr; clv = (c - l) / (h - l).replace(0, np.nan); volr = v / v.rolling(20).mean().replace(0, np.nan)
    cap = (dca <= -3) & (volr >= 4) & (clv <= 0.25); eup = (dca >= 3) & (volr >= 4) & (clv >= 0.75)
    f["capit5"] = cap.astype(int).rolling(5).max(); f["euph5"] = eup.astype(int).rolling(5).max()
    f["dd_dur60"] = idx - idx.where(c == c.rolling(60).max()).ffill(); f["du_dur60"] = idx - idx.where(c == c.rolling(60).min()).ffill()
    lc = np.log(c); x = np.arange(250); xm = x.mean(); den = ((x - xm) ** 2).sum()
    sl = np.convolve(lc.values, (x - xm)[::-1], mode="valid") / den; sl = pd.Series(np.concatenate([np.full(249, np.nan), sl]), index=df.index)
    trend = lc.rolling(250).mean() + sl * (249 - xm); acc = (c - c.shift(5)) / 5 - (c - c.shift(20)) / 20
    f["bubble"] = (lc - trend) * np.sign(acc)
    m20 = c.rolling(20).mean(); f["deadband20"] = ((c - m20).abs() <= 0.5 * atr).astype(int).rolling(20).sum()
    tp = (h + l + c) / 3; vwap20 = (tp * v).rolling(20).sum() / v.rolling(20).sum().replace(0, np.nan); f["cant20"] = (c - vwap20) / atr
    shock = dca <= -2; shock_bar = idx.where(shock).ffill(); low_since = l.groupby(shock.cumsum()).cummin(); broke = (l < low_since.shift(1)).groupby(shock.cumsum()).cummax()
    f["remission"] = (idx - shock_bar).where(~broke.astype(bool), 0)
    f["retreat"] = ((c < l.shift(1).rolling(10).min()) & (volr > 1)).astype(float)
    f["quantile250"] = c.rolling(250).rank(pct=True)
    m50 = c.rolling(50).mean(); f["local_time50"] = ((c - m50).abs() <= 0.25 * atr).astype(int).rolling(50).sum()
    f["d_hi250"] = (c - h.rolling(250).max()) / atr; f["d_lo250"] = (c - l.rolling(250).min()) / atr
    side = np.sign(c - m20); cross = side != side.shift(1); f["overshoot20"] = ((c - m20).abs() / atr).groupby(cross.cumsum()).cummax()
    lo60 = l.rolling(60).min(); f["foundation60"] = ((l - lo60).abs() <= 0.5 * atr).astype(int).rolling(60).sum()
    f["env20"] = (h.rolling(20).max() - l.rolling(20).min()) / atr
    dist50 = (c - m50) / atr; f["pid_i20"] = dist50.rolling(20).sum(); f["pid_d5"] = dist50 - dist50.shift(5)
    f["kurt60"] = r.rolling(60).kurt(); f["skew60"] = r.rolling(60).skew()
    ar = r.abs(); f["tail250"] = ar.rolling(250).quantile(0.95) / ar.rolling(250).median().replace(0, np.nan)
    up = (dc > 0) & (v > v.shift(1)); f["bandwagon"] = up.astype(int).groupby((~up).cumsum()).cumsum()
    sv = np.sign(dc) * volr; f["kyle60"] = dca.rolling(60).cov(sv) / sv.rolling(60).var().replace(0, np.nan)
    f["red_queen"] = ((c - c.shift(20)) - (c.shift(20) * (np.exp(sl * 20) - 1))) / atr
    d = np.sign(dc).replace(0, np.nan).ffill(); run = (d != d.shift(1)).cumsum(); f["inertia"] = (dc / atr).groupby(run).cumsum()
    sg = np.array([-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5]) / 110.0; f["sg_slope11"] = pd.Series(np.concatenate([np.full(10, np.nan), np.convolve(c.values, sg[::-1], mode="valid")]), index=df.index) / atr
    gap = (o - c.shift(1)).abs(); f["price_disc10"] = gap.rolling(10).sum() / dc.abs().rolling(10).sum().replace(0, np.nan)
    f["info_ratio20"] = r.rolling(20).sum() / r.rolling(20).std().replace(0, np.nan)
    f["work10"] = ((c - o) * v).rolling(10).sum() / v.rolling(10).sum().replace(0, np.nan) / atr
    f["crit_mass"] = ((volr > 2).astype(int).rolling(3).sum() == 3).astype(float)
    f["mad20"] = (dc - dc.rolling(20).median()).abs().rolling(20).median() / atr
    f["pot60"] = (dca.abs() > 2).astype(int).rolling(60).sum()
    cov = dc.rolling(60).cov(dc.shift(1)); f["roll60"] = np.sqrt((-cov).clip(lower=0)) / atr
    x20 = np.arange(20); xm20 = x20.mean(); d20 = ((x20 - xm20) ** 2).sum()
    rs = pd.Series(np.concatenate([np.full(19, np.nan), np.convolve((h - l).values, (x20 - xm20)[::-1], mode="valid") / d20]), index=df.index) / atr
    vs = pd.Series(np.concatenate([np.full(19, np.nan), np.convolve(volr.fillna(1).values, (x20 - xm20)[::-1], mode="valid") / d20]), index=df.index)
    f["anneal20"] = -rs * vs * np.sign(vs) * np.sign(-rs)
    f["siege20"] = ((f["env20"] <= 3) & (vs < 0)).astype(float)
    f["vratio20"] = c.pct_change(20).rolling(100).var() / (20 * r.rolling(100).var()).replace(0, np.nan)
    ratio = ta.atr(df, 5) / ta.atr(df, 50); above = ratio > 1; f["regime_age"] = idx - idx.where(above != above.shift(1)).ffill()
    a = (dc > 0).astype(float); b = (v > v.shift(1)).astype(float)
    p11 = (a * b).rolling(60).mean() + 1e-9; p10 = (a * (1 - b)).rolling(60).mean() + 1e-9; p01 = ((1 - a) * b).rolling(60).mean() + 1e-9; p00 = ((1 - a) * (1 - b)).rolling(60).mean() + 1e-9
    pa1, pb1 = p11 + p10, p11 + p01; pa0, pb0 = 1 - pa1, 1 - pb1
    f["mi60"] = p11 * np.log2(p11 / (pa1 * pb1)) + p10 * np.log2(p10 / (pa1 * pb0)) + p01 * np.log2(p01 / (pa0 * pb1)) + p00 * np.log2(p00 / (pa0 * pb0))
    return pd.DataFrame(f)


if __name__ == "__main__":
    M = pd.read_pickle("bend/data/matrix_D.pkl"); t0 = time.time(); parts = []
    for i, (t, g) in enumerate(M.groupby("ticker")):
        try: df = pd.read_csv(f"mine/cache/{t}.csv", parse_dates=["date"])
        except Exception: continue
        b = longlist(df).iloc[g.bar.values]; b["ticker"] = t; b["bar"] = g.bar.values; parts.append(b)
        if i % 400 == 0: print(i, f"{time.time()-t0:.0f}s", flush=True)
    G = pd.concat(parts); M = M.drop(columns=[c for c in G.columns if c in M.columns and c not in ("ticker", "bar")]).merge(G, on=["ticker", "bar"])
    M.to_pickle("bend/data/matrix_D.tmp.pkl"); os.replace("bend/data/matrix_D.tmp.pkl", "bend/data/matrix_D.pkl"); print(M.shape, f"{time.time()-t0:.0f}s")
    import json; mean = json.load(open("bend/evo/meaning.json")); mean.update(RATIONALE); json.dump(mean, open("bend/evo/meaning.json", "w"), indent=1, ensure_ascii=False)
