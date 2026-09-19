"""Round-4 blocks: the top of Jev's professional-lineage ranking (bend/data/models_jev3.csv) that were not yet blocks.
Own implementations; merged atomically into the matrix."""
import os, sys, time, numpy as np, pandas as pd
sys.path.insert(0, ".")
from mine import ta

RATIONALE = {
    "donch20": "Turtles · close at or above the prior 20-bar high (1/0)", "donch55": "Turtles · close at or above the prior 55-bar high (1/0)",
    "donch_lo20": "Turtles · close at or below the prior 20-bar low (1/0)",
    "tsmom": "Moskowitz · sign of the 252-bar return (1 up / 0 down)", "vsmom120": "Barroso · 120-bar return divided by realised volatility",
    "mom_consist": "Grinblatt–Moskowitz · positive 21-bar windows among the last 12", "fip": "Da–Gurun–Warachka · share of the last 252 days with the sign of the 252-bar return",
    "minervini": "Minervini · trend template: close > SMA50 > SMA150 > SMA200, SMA200 rising, ≥ 25 % above the 52-week low, within 25 % of the 52-week high (1/0)",
    "vol_hi50": "Gervais–Kaniel–Mingelgrin · volume at a 50-bar high (1/0)", "rv5": "Corsi HAR · 5-day realised volatility, %", "rv22": "Corsi HAR · 22-day realised volatility, %", "har_ratio": "Corsi HAR · 5-day over 22-day realised volatility",
    "pivot_buy": "O'Neil · close above the prior 20-bar high on volume ≥ 1.5× average (1/0)", "on_mom20": "Lou–Polk–Skouras · cumulative overnight (close-to-open) return over 20 bars, %",
    "mad21_200": "Avramov et al. · 21-day SMA over the 200-day SMA minus 1", "max20": "Bali–Cakici–Whitelaw · largest daily return in 20 bars, %", "min20": "MIN effect · most negative daily return in 20 bars, %",
    "mfi14": "Quong–Soudack · money flow index (14)", "ravi": "Chande · RAVI: (SMA7 − SMA65) / SMA65, %", "kelt_pos": "Keltner · position of the close inside the 20-EMA ± 1.5 ATR channel",
    "vstop": "Wilder · close minus (20-bar high − 3 ATR), in ATR", "crsi2": "Connors · cumulative RSI(2) of the last two bars", "liq_shock": "Bali et al. · 20-bar Amihud over its 250-bar mean",
    "cmo14": "Chande · momentum oscillator (14)", "effort": "Wyckoff · effort vs result: volume ratio over |daily change| in ATR", "obv_div": "Granville · OBV 20-bar slope minus price 20-bar slope (both standardised)",
    "td_setup": "DeMark · consecutive closes above the close 4 bars earlier", "td_setup_dn": "DeMark · consecutive closes below the close 4 bars earlier",
    "spring": "Wyckoff · low below the 20-bar range low with a close back inside (1/0)", "upthrust": "Wyckoff · high above the 20-bar range high with a close back inside (1/0)",
    "stage_slope": "Weinstein · slope of the 150-day SMA over 20 bars, in ATR", "nr7": "Toby Crabel · narrowest range of the last 7 bars (1/0)", "inside_n": "consecutive inside bars",
    "ulcer14": "Martin · Ulcer index: RMS drawdown from the 14-bar high, %", "sharpe60": "rolling 60-bar Sharpe (mean/std of daily returns)",
    "gk20": "Garman–Klass · 20-bar OHLC volatility, %", "yz20": "Yang–Zhang · 20-bar volatility with overnight variance, %", "jump20": "Barndorff-Nielsen · realised variance minus bipower variation over 20 bars (jump share)",
    "cusum": "López de Prado · symmetric CUSUM of daily changes in ATR since the last reset at ±3", "tscan": "López de Prado · best t-statistic of the linear trend over horizons 5 to 20",
    "cs_spread": "Corwin–Schultz · high-low spread estimator over 2-day windows, averaged 20 bars, %", "vpin20": "Easley–López de Prado–O'Hara · VPIN proxy: mean |up − down volume| ÷ volume over 20 bars",
    "fracdiff": "López de Prado · fractionally differenced close (d = 0.5, 50 lags), in ATR", "cci_sig": "Lambert · CCI(20) crossing above −100 within 3 bars (1/0)",
    "stoch_pull": "Raschke · stochastic(7) below 30 while ADX(14) > 30 (1/0)", "double7": "Connors · close at a 7-bar low above the 200-SMA (1/0)",
    "ha_run": "Vervoort · consecutive Heikin-Ashi candles of one colour (signed)", "bop14": "Levine · balance of power, 14-bar SMA", "eom14": "Arms · ease of movement (14), standardised",
}


def pro(df: pd.DataFrame) -> pd.DataFrame:
    c, o, h, l, v = df.close, df.open, df.high, df.low, df.volume.astype(float)
    atr = ta.atr(df, 14); dc = c.diff(); r = c.pct_change(); n = len(c); f = {}
    f["donch20"] = (c >= h.shift(1).rolling(20).max()).astype(float); f["donch55"] = (c >= h.shift(1).rolling(55).max()).astype(float); f["donch_lo20"] = (c <= l.shift(1).rolling(20).min()).astype(float)
    f["tsmom"] = (c > c.shift(252)).astype(float); f["vsmom120"] = c.pct_change(120) / r.rolling(120).std().replace(0, np.nan) / np.sqrt(120)
    w21 = c.pct_change(21); f["mom_consist"] = sum((w21.shift(21 * k) > 0).astype(int) for k in range(12)).astype(float)
    sgn252 = np.sign(c.pct_change(252)); f["fip"] = (np.sign(r) == sgn252).astype(float).rolling(252).mean()
    s50, s150, s200 = c.rolling(50).mean(), c.rolling(150).mean(), c.rolling(200).mean()
    f["minervini"] = ((c > s50) & (s50 > s150) & (s150 > s200) & (s200 > s200.shift(20)) & (c >= 1.25 * l.rolling(252).min()) & (c >= 0.75 * h.rolling(252).max())).astype(float)
    f["vol_hi50"] = (v >= v.rolling(50).max()).astype(float)
    f["rv5"] = np.sqrt((r ** 2).rolling(5).sum()) * 100; f["rv22"] = np.sqrt((r ** 2).rolling(22).sum() * 5 / 22) * 100; f["har_ratio"] = f["rv5"] / f["rv22"].replace(0, np.nan)
    volr = v / v.rolling(20).mean().replace(0, np.nan); f["pivot_buy"] = ((c > h.shift(1).rolling(20).max()) & (volr >= 1.5)).astype(float)
    f["on_mom20"] = (o / c.shift(1) - 1).rolling(20).sum() * 100
    f["mad21_200"] = c.rolling(21).mean() / s200 - 1; f["max20"] = r.rolling(20).max() * 100; f["min20"] = r.rolling(20).min() * 100
    tp = (h + l + c) / 3; mf = tp * v; pos = mf.where(tp > tp.shift(1), 0.0).rolling(14).sum(); neg = mf.where(tp < tp.shift(1), 0.0).rolling(14).sum()
    f["mfi14"] = 100 - 100 / (1 + pos / neg.replace(0, np.nan))
    f["ravi"] = (c.rolling(7).mean() - c.rolling(65).mean()) / c.rolling(65).mean() * 100
    e20 = ta.ema(c, 20); k = ta.atr(df, 20) * 1.5; f["kelt_pos"] = (c - (e20 - k)) / (2 * k)
    f["vstop"] = (c - (h.rolling(20).max() - 3 * atr)) / atr
    rsi2 = ta.rsi(c, 2); f["crsi2"] = rsi2 + rsi2.shift(1)
    am = (r.abs() / (c * v).replace(0, np.nan)).rolling(20).mean(); f["liq_shock"] = am / am.rolling(250).mean().replace(0, np.nan)
    up = dc.clip(lower=0).rolling(14).sum(); dn = (-dc).clip(lower=0).rolling(14).sum(); f["cmo14"] = (up - dn) / (up + dn).replace(0, np.nan) * 100
    f["effort"] = volr / (dc.abs() / atr).replace(0, np.nan)
    obv = (np.sign(dc) * v).cumsum(); f["obv_div"] = (obv - obv.shift(20)) / v.rolling(20).sum().replace(0, np.nan) - (c - c.shift(20)) / (20 * atr)
    upc = (c > c.shift(4)).astype(int); f["td_setup"] = upc.groupby((upc == 0).cumsum()).cumsum(); dnc = (c < c.shift(4)).astype(int); f["td_setup_dn"] = dnc.groupby((dnc == 0).cumsum()).cumsum()
    rlo = l.shift(1).rolling(20).min(); rhi = h.shift(1).rolling(20).max(); f["spring"] = ((l < rlo) & (c > rlo)).astype(float); f["upthrust"] = ((h > rhi) & (c < rhi)).astype(float)
    f["stage_slope"] = (s150 - s150.shift(20)) / atr
    rng = h - l; f["nr7"] = (rng <= rng.rolling(7).min()).astype(float)
    ins = ((h <= h.shift(1)) & (l >= l.shift(1))).astype(int); f["inside_n"] = ins.groupby((ins == 0).cumsum()).cumsum()
    ddp = (c / c.rolling(14).max() - 1) * 100; f["ulcer14"] = np.sqrt((ddp ** 2).rolling(14).mean())
    f["sharpe60"] = r.rolling(60).mean() / r.rolling(60).std().replace(0, np.nan) * np.sqrt(252)
    lhl = np.log(h / l); lco = np.log(c / o); f["gk20"] = np.sqrt((0.5 * lhl ** 2 - (2 * np.log(2) - 1) * lco ** 2).rolling(20).mean() * 252) * 100
    loc = np.log(o / c.shift(1)); rs = np.log(h / o) * np.log(h / c) + np.log(l / o) * np.log(l / c)
    f["yz20"] = np.sqrt((loc.rolling(20).var() + 0.34 * lco.rolling(20).var() + 0.66 * rs.rolling(20).mean()) * 252) * 100
    rv = (r ** 2).rolling(20).sum(); bpv = (np.pi / 2) * (r.abs() * r.abs().shift(1)).rolling(20).sum(); f["jump20"] = ((rv - bpv) / rv.replace(0, np.nan)).clip(lower=0)
    x = (dc / atr).fillna(0).values; sp = np.zeros(n); sm = np.zeros(n); cs = np.zeros(n)
    for i in range(1, n):
        sp[i] = max(0.0, sp[i - 1] + x[i]); sm[i] = min(0.0, sm[i - 1] + x[i])
        if sp[i] > 3: sp[i] = 0.0
        if sm[i] < -3: sm[i] = 0.0
        cs[i] = sp[i] + sm[i]
    f["cusum"] = pd.Series(cs, index=df.index)
    best = None
    for m in (5, 10, 15, 20):
        xx = np.arange(m); xm = xx.mean(); den = ((xx - xm) ** 2).sum()
        slope = pd.Series(np.concatenate([np.full(m - 1, np.nan), np.convolve(c.values, (xx - xm)[::-1], mode="valid") / den]), index=df.index)
        fit = slope * (xx[-1] - xm) + c.rolling(m).mean(); resid = (c - fit).rolling(m).std().replace(0, np.nan)
        t = slope / (resid / np.sqrt(den))
        best = t if best is None else best.where(best.abs() >= t.abs(), t)
    f["tscan"] = best
    beta = (np.log(h / l) ** 2).rolling(2).sum(); hh2 = h.rolling(2).max(); ll2 = l.rolling(2).min(); gamma = np.log(hh2 / ll2) ** 2
    alpha = (np.sqrt(2 * beta) - np.sqrt(beta)) / (3 - 2 * np.sqrt(2)) - np.sqrt(gamma / (3 - 2 * np.sqrt(2)))
    spread = 2 * (np.exp(alpha) - 1) / (1 + np.exp(alpha)); f["cs_spread"] = spread.clip(lower=0).rolling(20).mean() * 100
    upv = v.where(dc > 0, 0.0); dnv = v.where(dc < 0, 0.0); f["vpin20"] = ((upv - dnv).abs().rolling(20).sum()) / v.rolling(20).sum().replace(0, np.nan)
    wts = [1.0]
    for k in range(1, 50):
        wts.append(-wts[-1] * (0.5 - k + 1) / k)
    wts = np.array(wts); f["fracdiff"] = pd.Series(np.concatenate([np.full(49, np.nan), np.convolve(c.values, wts[::-1], mode="valid")]), index=df.index) / atr
    cci = (tp - tp.rolling(20).mean()) / (0.015 * tp.rolling(20).apply(lambda w: np.abs(w - w.mean()).mean(), raw=True))
    f["cci_sig"] = ((cci > -100) & (cci.shift(1) <= -100)).astype(int).rolling(3).max()
    hh7, ll7 = h.rolling(7).max(), l.rolling(7).min(); st7 = (c - ll7) / (hh7 - ll7).replace(0, np.nan) * 100
    pdm = (h.diff().clip(lower=0)).where(h.diff() > -l.diff(), 0.0); ndm = (-l.diff()).clip(lower=0).where(-l.diff() > h.diff(), 0.0); tr14 = ta.rma(ta.tr(df), 14)
    pdi = 100 * ta.rma(pdm, 14) / tr14; ndi = 100 * ta.rma(ndm, 14) / tr14; adx = ta.rma((pdi - ndi).abs() / (pdi + ndi).replace(0, np.nan) * 100, 14)
    f["stoch_pull"] = ((st7 < 30) & (adx > 30)).astype(float); f["double7"] = ((c <= c.rolling(7).min()) & (c > s200)).astype(float)
    ha_c = (o + h + l + c) / 4; ha_o = ((o + c) / 2).shift(1); col = np.sign(ha_c - ha_o); run = (col != col.shift(1)).cumsum(); f["ha_run"] = col.groupby(run).cumsum()
    f["bop14"] = ((c - o) / (h - l).replace(0, np.nan)).rolling(14).mean()
    mid = (h + l) / 2; eom = mid.diff() / ((v / 1e8) / (h - l).replace(0, np.nan)).replace(0, np.nan); e14 = eom.rolling(14).mean(); f["eom14"] = e14 / e14.rolling(100).std().replace(0, np.nan)
    return pd.DataFrame(f)


if __name__ == "__main__":
    M = pd.read_pickle("bend/data/matrix_D.pkl"); t0 = time.time(); parts = []
    for i, (t, g) in enumerate(M.groupby("ticker")):
        try: df = pd.read_csv(f"mine/cache/{t}.csv", parse_dates=["date"])
        except Exception: continue
        b = pro(df).iloc[g.bar.values]; b["ticker"] = t; b["bar"] = g.bar.values; parts.append(b)
        if i % 400 == 0: print(i, f"{time.time()-t0:.0f}s", flush=True)
    G = pd.concat(parts); M = M.drop(columns=[c for c in G.columns if c in M.columns and c not in ("ticker", "bar")]).merge(G, on=["ticker", "bar"])
    M.to_pickle("bend/data/matrix_D.tmp.pkl"); os.replace("bend/data/matrix_D.tmp.pkl", "bend/data/matrix_D.pkl"); print(M.shape, f"{time.time()-t0:.0f}s")
    import json; mean = json.load(open("bend/evo/meaning.json")); mean.update(RATIONALE); json.dump(mean, open("bend/evo/meaning.json", "w"), indent=1, ensure_ascii=False)
