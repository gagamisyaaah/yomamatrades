"""Building-block library: every indicator family at several parameterisations, computed at bar i from bars ≤ i.
blocks(df) → DataFrame of features for one ticker (all bars). Used at the labelled random bars for evolution and on
every bar for the finalists' replay. Each block name carries its Pine equivalent in PINE."""
import numpy as np, pandas as pd
import sys; sys.path.insert(0, ".")
from mine import ta

PINE = {}


def reg(name, pine):
    PINE[name] = pine


def blocks(df: pd.DataFrame) -> pd.DataFrame:
    c, o, h, l, v = df.close, df.open, df.high, df.low, df.volume.astype(float)
    atr = ta.atr(df, 14); f = {}
    for n in (2, 3, 5, 7, 14, 21):
        f[f"rsi{n}"] = ta.rsi(c, n); reg(f"rsi{n}", f"ta.rsi(close, {n})")
    for n in (1, 2, 3, 5, 10, 20, 60, 120, 252):
        f[f"roc{n}"] = c.pct_change(n) * 100; reg(f"roc{n}", f"ta.roc(close, {n})")
    f["mom12_1"] = (c.shift(21) / c.shift(252) - 1) * 100; reg("mom12_1", "(close[21] / close[252] - 1) * 100")
    for n in (5, 10, 20, 50, 100, 200):
        f[f"dsma{n}"] = (c - c.rolling(n).mean()) / atr; reg(f"dsma{n}", f"(close - ta.sma(close, {n})) / ta.atr(14)")
        f[f"dema{n}"] = (c - ta.ema(c, n)) / atr; reg(f"dema{n}", f"(close - ta.ema(close, {n})) / ta.atr(14)")
    for n in (20, 50, 126, 252):
        f[f"hi{n}"] = c / h.rolling(n).max() * 100; reg(f"hi{n}", f"close / ta.highest(high, {n}) * 100")
        f[f"lo{n}"] = c / l.rolling(n).min() * 100; reg(f"lo{n}", f"close / ta.lowest(low, {n}) * 100")
    for n in (10, 20):
        m, s = c.rolling(n).mean(), c.rolling(n).std()
        f[f"bbpct{n}"] = (c - (m - 2 * s)) / (4 * s); reg(f"bbpct{n}", f"ta.bbw... (close - ta.sma(close,{n}) + 2*ta.stdev(close,{n})) / (4*ta.stdev(close,{n}))")
        f[f"bbw{n}"] = 4 * s / m * 100; reg(f"bbw{n}", f"4 * ta.stdev(close, {n}) / ta.sma(close, {n}) * 100")
    for n in (5, 14):
        hh, ll = h.rolling(n).max(), l.rolling(n).min()
        f[f"stoch{n}"] = (c - ll) / (hh - ll).replace(0, np.nan) * 100; reg(f"stoch{n}", f"ta.stoch(close, high, low, {n})")
        f[f"willr{n}"] = f[f"stoch{n}"] - 100
    for n in (14, 20):
        tp = (h + l + c) / 3; f[f"cci{n}"] = (tp - tp.rolling(n).mean()) / (0.015 * tp.rolling(n).apply(lambda x: np.abs(x - x.mean()).mean(), raw=True)); reg(f"cci{n}", f"ta.cci(hlc3, {n})")
    for a, b in ((5, 20), (5, 50), (14, 50), (20, 100)):
        f[f"atrr{a}_{b}"] = ta.atr(df, a) / ta.atr(df, b); reg(f"atrr{a}_{b}", f"ta.atr({a}) / ta.atr({b})")
    for n in (5, 10, 20):
        f[f"rng{n}"] = (h.rolling(n).max() - l.rolling(n).min()) / (h.rolling(50).max() - l.rolling(50).min()); reg(f"rng{n}", f"(ta.highest(high,{n}) - ta.lowest(low,{n})) / (ta.highest(high,50) - ta.lowest(low,50))")
    f["atrpct"] = atr / c * 100; reg("atrpct", "ta.atr(14) / close * 100")
    for n in (5, 10, 20, 50):
        f[f"volr{n}"] = v / v.rolling(n).mean(); reg(f"volr{n}", f"volume / ta.sma(volume, {n})")
    f["dvol20"] = np.log10((c * v).rolling(20).mean().replace(0, np.nan)); reg("dvol20", "log10(ta.sma(close * volume, 20))")
    obv = (np.sign(c.diff()) * v).cumsum(); f["obv_slope20"] = (obv - obv.shift(20)) / v.rolling(20).sum(); reg("obv_slope20", "(ta.obv - ta.obv[20]) / math.sum(volume, 20)")
    f["gap"] = (o - c.shift(1)) / atr; reg("gap", "(open - close[1]) / ta.atr(14)")
    f["clv"] = (c - l) / (h - l).replace(0, np.nan); reg("clv", "(close - low) / (high - low)")
    f["body"] = (c - o) / atr; reg("body", "(close - open) / ta.atr(14)")
    f["range_atr"] = (h - l) / atr; reg("range_atr", "(high - low) / ta.atr(14)")
    dn = (c.diff() < 0).astype(int); f["dstreak"] = dn.groupby((dn == 0).cumsum()).cumsum(); reg("dstreak", "consecutive down closes")
    up = (c.diff() > 0).astype(int); f["ustreak"] = up.groupby((up == 0).cumsum()).cumsum(); reg("ustreak", "consecutive up closes")
    f["dow"] = df.date.dt.dayofweek; reg("dow", "dayofweek")
    f["dom"] = df.date.dt.day; reg("dom", "dayofmonth")
    # ADX
    pdm = (h.diff().clip(lower=0)).where(h.diff() > -l.diff(), 0.0); ndm = (-l.diff()).clip(lower=0).where(-l.diff() > h.diff(), 0.0)
    tr14 = ta.rma(ta.tr(df), 14); pdi = 100 * ta.rma(pdm, 14) / tr14; ndi = 100 * ta.rma(ndm, 14) / tr14
    f["adx14"] = ta.rma((pdi - ndi).abs() / (pdi + ndi).replace(0, np.nan) * 100, 14); reg("adx14", "ta.dmi(14, 14) adx")
    f["dmi_diff"] = pdi - ndi; reg("dmi_diff", "+DI − −DI")
    dif = ta.ema(c, 12) - ta.ema(c, 26); f["macd_hist"] = (dif - ta.ema(dif, 9)) / atr; reg("macd_hist", "ta.macd hist / atr")
    f["macd_dif"] = dif / atr
    f["ret_gap_5"] = (c - c.shift(5)) / atr; f["hl_pos20"] = (c - l.rolling(20).min()) / (h.rolling(20).max() - l.rolling(20).min()).replace(0, np.nan)
    f["vol_z20"] = (v - v.rolling(20).mean()) / v.rolling(20).std().replace(0, np.nan)
    f["corr_spy"] = np.nan  # filled by caller if wanted
    return pd.DataFrame(f)


if __name__ == "__main__":
    import time
    R = pd.read_pickle("bend/data/control_D.pkl")[["ticker", "bar", "date", "year", "band", "atr_pct", "r24", "r34", "s53", "t10", "flip", "mfe", "mae", "x_r24", "x_t10", "x_flip", "hole_up", "hole_dn", "whales50", "te_up", "mtn3", "gold", "hot", "ss", "ribbon", "poc", "macdrsi", "fresh", "panels6", "whales70", "whales_lt30", "hole_inforce"]]
    t0 = time.time(); parts = []
    for i, (t, g) in enumerate(R.groupby("ticker")):
        try: df = pd.read_csv(f"mine/cache/{t}.csv", parse_dates=["date"])
        except Exception: continue
        b = blocks(df).iloc[g.bar.values]; b["ticker"] = t; b["bar"] = g.bar.values; parts.append(b)
        if i % 400 == 0: print(i, f"{time.time()-t0:.0f}s", flush=True)
    B = pd.concat(parts); M = R.merge(B, on=["ticker", "bar"])
    for k in ["roc5", "roc20", "roc60", "mom12_1", "hi252", "hi52w" if False else "hi126", "volr20", "rsi2", "rsi14", "atrpct", "dvol20"]:
        M[f"xs_{k}"] = M.groupby("date")[k].rank(pct=True)
    M.to_pickle("bend/data/matrix_D.pkl"); import json; json.dump(PINE, open("bend/evo/pine_blocks.json", "w"), indent=1)
    print(M.shape, f"{time.time()-t0:.0f}s")
