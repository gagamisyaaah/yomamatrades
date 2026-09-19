"""Write a finalist rule as a Pine v5 indicator (+ strategy twin) from the block → Pine map.
usage: python3.12 bend/evo/pine.py --rule "rsi2≤p10(9.215) & hi252≥p75(90.1)" --name "SUPER A pullback" --out ../tv-indicator-clones/evolved"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
COND = re.compile(r"^(?P<b>[a-z0-9_]+)(?P<op>≤|≥|=)(?:p\d+\()?(?P<thr>-?[0-9.e+-]+)\)?$")

# Pine v5 expressions for every chart-computable block (the evolution uses exactly these definitions)
PINE = {
    **{f"rsi{n}": f"ta.rsi(close, {n})" for n in (2, 3, 5, 7, 14, 21)},
    **{f"roc{n}": f"ta.roc(close, {n})" for n in (1, 2, 3, 5, 10, 20, 60, 120, 252)},
    "mom12_1": "(close[21] / close[252] - 1) * 100",
    **{f"dsma{n}": f"(close - ta.sma(close, {n})) / atr" for n in (5, 10, 20, 50, 100, 200)},
    **{f"dema{n}": f"(close - ta.ema(close, {n})) / atr" for n in (5, 10, 20, 50, 100, 200)},
    **{f"hi{n}": f"close / ta.highest(high, {n}) * 100" for n in (20, 50, 126, 252)},
    **{f"lo{n}": f"close / ta.lowest(low, {n}) * 100" for n in (20, 50, 126, 252)},
    **{f"bbpct{n}": f"(close - (ta.sma(close, {n}) - 2 * ta.stdev(close, {n}))) / (4 * ta.stdev(close, {n}))" for n in (10, 20)},
    **{f"bbw{n}": f"4 * ta.stdev(close, {n}) / ta.sma(close, {n}) * 100" for n in (10, 20)},
    **{f"stoch{n}": f"ta.stoch(close, high, low, {n})" for n in (5, 14)},
    **{f"willr{n}": f"ta.stoch(close, high, low, {n}) - 100" for n in (5, 14)},
    **{f"cci{n}": f"ta.cci(hlc3, {n})" for n in (14, 20)},
    **{f"atrr{a}_{b}": f"ta.atr({a}) / ta.atr({b})" for a, b in ((5, 20), (5, 50), (14, 50), (20, 100))},
    **{f"rng{n}": f"(ta.highest(high, {n}) - ta.lowest(low, {n})) / (ta.highest(high, 50) - ta.lowest(low, 50))" for n in (5, 10, 20)},
    "atrpct": "atr / close * 100",
    **{f"volr{n}": f"volume / ta.sma(volume, {n})" for n in (5, 10, 20, 50)},
    "dvol20": "math.log10(ta.sma(close * volume, 20))", "obv_slope20": "(ta.obv - ta.obv[20]) / math.sum(volume, 20)",
    "gap": "(open - close[1]) / atr", "clv": "(close - low) / (high - low)", "body": "(close - open) / atr", "range_atr": "(high - low) / atr",
    "dstreak": "dstreak", "ustreak": "ustreak", "dow": "dayofweek - 2", "dom": "dayofmonth",
    "adx14": "adx14", "dmi_diff": "diplus - diminus", "macd_hist": "(macdLine - signalLine) / atr", "macd_dif": "macdLine / atr",
    "ret_gap_5": "(close - close[5]) / atr", "hl_pos20": "(close - ta.lowest(low, 20)) / (ta.highest(high, 20) - ta.lowest(low, 20))",
    "vol_z20": "(volume - ta.sma(volume, 20)) / ta.stdev(volume, 20)",
    # geometry (swing pivots of strength 4, confirmed 4 bars later, like the Dragon hole)
    "d_ph": "(close - ph_px) / atr", "d_pl": "(close - pl_px) / atr", "bars_ph": "bar_index - ph_bar", "bars_pl": "bar_index - pl_bar",
    "hh": "hh", "hl": "hl", "struct": "swing_structure", "leg_atr": "(ph_px - pl_px) / atr", "leg_bars": "math.abs(ph_bar - pl_bar)",
    "retrace": "ph_bar > pl_bar ? (ph_px - close) / (ph_px - pl_px) : (close - pl_px) / (ph_px - pl_px)", "up_leg": "ph_bar > pl_bar ? 1 : 0",
    "bars_52w_hi": "bar_index - ta.valuewhen(high == ta.highest(high, 252), bar_index, 0)",
    **{f"eff{m}": f"math.abs(close - close[{m}]) / math.sum(math.abs(ta.change(close)), {m})" for m in (5, 10, 20, 50)},
    **{f"slope{m}": f"(close - close[{m}]) / {m} / atr" for m in (5, 10, 20, 50)},
    **{f"lr{m}": f"(ta.linreg(close, {m}, 0) - ta.linreg(close, {m}, 1)) / atr" for m in (5, 20, 60)},
    "accel": "((ta.linreg(close, 5, 0) - ta.linreg(close, 5, 1)) - (ta.linreg(close, 20, 0) - ta.linreg(close, 20, 1))) / atr",
    "squeeze_bars": "squeeze_bars", "vel_ratio": "((close - close[5]) / 5) / ((close - close[20]) / 20)",
    # self-written blocks (physics / biology / other fields)
    "kinetic20": "vel20 * math.abs(vel20)", "p_mass20": "vel20 * math.log10(ta.sma(close * volume, 20)) / 8",
    "impulse10": "math.sum(math.sign(ta.change(close)) * volume, 10) / math.sum(volume, 10)",
    "damping": "leg_now / leg_prev", "spring20": "(close - vwap20) / (ta.stdev(ta.roc(close, 1) / 100, 20) * close)",
    "phase": "atan2(vel5 - vel20, vel20)", "resonance20": "ta.correlation(ta.roc(close, 1), ta.roc(close, 1)[1], 20)",
    "vratio50": "ta.variance(ta.roc(close, 5), 50) / (5 * ta.variance(ta.roc(close, 1), 50))",
    "fractal30": "math.log10(30) / (math.log10(30) + math.log10((ta.highest(high, 30) - ta.lowest(low, 30)) / math.sum(math.abs(ta.change(close)), 30)))",
    "impact20": "(math.sum(high - low, 20) / math.sum(volume, 20)) / ta.sma(math.sum(high - low, 20) / math.sum(volume, 20), 100)",
    "strain": "squeeze_bars * ta.atr(100) / ta.atr(20)", "predator_prey10": "math.log((upv10 + 1) / (dnv10 + 1))", "pp_drift": "math.log((upv10 + 1) / (dnv10 + 1)) - math.log((upv10[5] + 1) / (dnv10[5] + 1))",
    "recovery": "bar_index - shock_bar", "resilience": "(close - shock_px) / atr", "stable20": "math.sum(math.abs(close - ta.median(close, 20)) <= atr ? 1 : 0, 20)",
    "flatten": "(ta.linreg(close, 60, 0) - ta.linreg(close, 60, 1)) / atr - (ta.linreg(close, 5, 0) - ta.linreg(close, 5, 1)) / atr",
    "r0": "(math.sum(close > close[1] ? 1 : 0, 5) + 1) / (math.sum(close[5] > close[6] ? 1 : 0, 5) + 1)", "metabolic": "volume / ta.sma(volume, 20) * (high - low) / atr",
    "aggression10": "math.sum(((close - low) - (high - close)) / (high - low) * volume, 10) / math.sum(volume, 10)",
    "snr20": "(close - close[20]) / (ta.stdev(ta.change(close), 20) * math.sqrt(20))", "thrust5": "math.sum(close > close[1] ? volume : 0, 5) / math.sum(volume, 5)",
    "asym20": "math.log((drawup20 + 0.01) / (drawdown20 + 0.01))", "time_sym": "math.log((bar_index - pl_bar + 1) / (bar_index - ph_bar + 1))",
    "gap_fill10": "math.sum(open > close[1] ? (low <= close[1] ? 1 : 0) : (high >= close[1] ? 1 : 0), 10) / 10",
    "ou_speed60": "ou_speed60", "vwap_dev50": "(close - math.sum(hlc3 * volume, 50) / math.sum(volume, 50)) / atr",
    # scouted blocks
    "vfi20": "vfi20", "obv_osc20": "(ta.obv - ta.ema(ta.obv, 20)) / math.sum(volume, 20)", "weis_ratio": "weis_ratio",
    "sqz_on": "sqz_on ? 1 : 0", "sqz_mom20": "ta.linreg(close - ((ta.highest(high, 20) + ta.lowest(low, 20)) / 2 + ta.sma(close, 20)) / 2, 20, 0) * 20 / atr",
    "st_dist": "(close - st_line) / atr", "st_dir": "st_dirn < 0 ? 1 : 0", "st_age": "bar_index - ta.valuewhen(ta.change(st_dirn) != 0, bar_index, 0)",
    "avwap_pl": "(close - avwap_pl) / atr", "avwap_ph": "(close - avwap_ph) / atr", "bo_bars20": "bar_index - ta.valuewhen(close > ta.highest(close[1], 20) and volume > ta.sma(volume, 20), bar_index, 0)",
    # round-3 blocks (Jev's longlist)
    "amihud20": "ta.sma(math.abs(ta.roc(close, 1) / 100) / (close * volume), 20) / ta.median(ta.sma(math.abs(ta.roc(close, 1) / 100) / (close * volume), 20), 250)",
    "win_mom20": "win_mom20", "capit5": "math.max(capit ? 1 : 0, capit[1] ? 1 : 0, capit[2] ? 1 : 0, capit[3] ? 1 : 0, capit[4] ? 1 : 0)", "euph5": "math.max(euph ? 1 : 0, euph[1] ? 1 : 0, euph[2] ? 1 : 0, euph[3] ? 1 : 0, euph[4] ? 1 : 0)",
    "dd_dur60": "bar_index - ta.valuewhen(close == ta.highest(close, 60), bar_index, 0)", "du_dur60": "bar_index - ta.valuewhen(close == ta.lowest(close, 60), bar_index, 0)",
    "bubble": "(math.log(close) - ta.linreg(math.log(close), 250, 0)) * math.sign((close - close[5]) / 5 - (close - close[20]) / 20)",
    "deadband20": "math.sum(math.abs(close - ta.sma(close, 20)) <= 0.5 * atr ? 1 : 0, 20)", "cant20": "(close - vwap20) / atr",
    "remission": "remission", "retreat": "close < ta.lowest(low[1], 10) and volume > ta.sma(volume, 20) ? 1 : 0", "quantile250": "ta.percentrank(close, 250) / 100",
    "local_time50": "math.sum(math.abs(close - ta.sma(close, 50)) <= 0.25 * atr ? 1 : 0, 50)", "d_hi250": "(close - ta.highest(high, 250)) / atr", "d_lo250": "(close - ta.lowest(low, 250)) / atr",
    "overshoot20": "overshoot20", "foundation60": "math.sum(math.abs(low - ta.lowest(low, 60)) <= 0.5 * atr ? 1 : 0, 60)", "env20": "(ta.highest(high, 20) - ta.lowest(low, 20)) / atr",
    "pid_i20": "math.sum((close - ta.sma(close, 50)) / atr, 20)", "pid_d5": "(close - ta.sma(close, 50)) / atr - (close[5] - ta.sma(close, 50)[5]) / atr[5]",
    "kurt60": "kurt60", "skew60": "skew60", "tail250": "ta.percentile_nearest_rank(math.abs(ta.roc(close, 1)), 250, 95) / ta.median(math.abs(ta.roc(close, 1)), 250)",
    "bandwagon": "bandwagon", "kyle60": "ta.correlation(ta.change(close) / atr, math.sign(ta.change(close)) * volume / ta.sma(volume, 20), 60) * ta.stdev(ta.change(close) / atr, 60) / ta.stdev(math.sign(ta.change(close)) * volume / ta.sma(volume, 20), 60)",
    "red_queen": "((close - close[20]) - close[20] * (math.exp((ta.linreg(math.log(close), 250, 0) - ta.linreg(math.log(close), 250, 1)) * 20) - 1)) / atr",
    "inertia": "inertia", "sg_slope11": "(5 * (close - close[10]) + 4 * (close[1] - close[9]) + 3 * (close[2] - close[8]) + 2 * (close[3] - close[7]) + (close[4] - close[6])) / 110 / atr",
    "price_disc10": "math.sum(math.abs(open - close[1]), 10) / math.sum(math.abs(ta.change(close)), 10)", "info_ratio20": "math.sum(ta.roc(close, 1), 20) / ta.stdev(ta.roc(close, 1), 20)",
    "work10": "math.sum((close - open) * volume, 10) / math.sum(volume, 10) / atr", "crit_mass": "math.sum(volume > 2 * ta.sma(volume, 20) ? 1 : 0, 3) == 3 ? 1 : 0",
    "mad20": "ta.median(math.abs(ta.change(close) - ta.median(ta.change(close), 20)), 20) / atr", "pot60": "math.sum(math.abs(ta.change(close)) / atr > 2 ? 1 : 0, 60)",
    "roll60": "math.sqrt(math.max(0, -ta.correlation(ta.change(close), ta.change(close)[1], 60) * ta.stdev(ta.change(close), 60) * ta.stdev(ta.change(close)[1], 60))) / atr",
    "anneal20": "anneal20", "siege20": "(ta.highest(high, 20) - ta.lowest(low, 20)) / atr <= 3 and vol_slope20 < 0 ? 1 : 0",
    "vratio20": "ta.variance(ta.roc(close, 20), 100) / (20 * ta.variance(ta.roc(close, 1), 100))", "regime_age": "bar_index - ta.valuewhen(ta.cross(ta.atr(5) / ta.atr(50), 1), bar_index, 0)",
    "mi60": "mi60",
    # Diamond / Dragon blocks a single script can compute (chips, hole and panel counts cannot — banned from breeding)
    "gold": "close > math.max(ta.ema(close, 21), ta.ema(close, 34), ta.ema(close, 55)) ? 1 : 0", "mtn3": "mountain >= 3 ? 1 : 0", "hot": "bubble >= 6 ? 1 : 0",
    "te_up": "te_v7 > te_v7[1] ? 1 : 0", "ribbon": "ta.ema((close + low) / 2, 8) > ta.ema(ta.ema((close + low) / 2, 20), 8) ? 1 : 0",
    "ss": "ss_confirm ? 1 : 0", "macdrsi": "macdLine > 0 and ta.rsi(close, 9) > ta.rsi(close, 9)[1] and ta.rsi(close, 14) > ta.rsi(close, 14)[1] and ta.rsi(close, 24) > ta.rsi(close, 24)[1] and ta.rsi(close, 14) > 50 ? 1 : 0",
}
PRELUDE = """atan2(y, x) => x > 0 ? math.atan(y / x) : x < 0 and y >= 0 ? math.atan(y / x) + math.pi : x < 0 and y < 0 ? math.atan(y / x) - math.pi : y > 0 ? math.pi / 2 : -math.pi / 2
atr = ta.atr(14)
ph = ta.pivothigh(high, 4, 4)
pl = ta.pivotlow(low, 4, 4)
var float ph_px = na
var float pl_px = na
var int ph_bar = na
var int pl_bar = na
var float ph_prev = na
var float pl_prev = na
var float hh = na
var float hl = na
if not na(ph)
    hh := ph > ph_px ? 1 : 0
    ph_prev := ph_px
    ph_px := ph
    ph_bar := bar_index - 4
if not na(pl)
    hl := pl > pl_px ? 1 : 0
    pl_prev := pl_px
    pl_px := pl
    pl_bar := bar_index - 4
// structure: higher highs / higher lows over the last three swings on each side
var float[] hhs = array.new_float()
var float[] hls = array.new_float()
if not na(ph) and not na(ph_prev)
    array.push(hhs, ph > ph_prev ? 1 : -1)
    if array.size(hhs) > 3
        array.shift(hhs)
if not na(pl) and not na(pl_prev)
    array.push(hls, pl > pl_prev ? 1 : -1)
    if array.size(hls) > 3
        array.shift(hls)
swing_structure = (array.size(hhs) > 0 ? array.sum(hhs) : 0) + (array.size(hls) > 0 ? array.sum(hls) : 0)
rng20 = (ta.highest(high, 20) - ta.lowest(low, 20)) / (ta.highest(high, 50) - ta.lowest(low, 50))
var int squeeze_bars = 0
squeeze_bars := rng20 < 0.5 ? squeeze_bars + 1 : 0
vel20 = (close - close[20]) / 20 / atr
vel5 = (close - close[5]) / 5 / atr
vwap20 = math.sum(hlc3 * volume, 20) / math.sum(volume, 20)
upv10 = math.sum(close > close[1] ? volume : 0, 10)
dnv10 = math.sum(close < close[1] ? volume : 0, 10)
shock = ta.change(close) / atr <= -2
shock_bar = ta.valuewhen(shock, bar_index, 0)
shock_px = ta.valuewhen(shock, close, 0)
drawup20 = ta.highest(close, 20) / ta.lowest(close, 20) - 1
drawdown20 = 1 - ta.lowest(close, 20) / ta.highest(close, 20)
dev60 = close[1] - ta.sma(close, 60)[1]
ou_speed60 = ta.correlation(ta.change(close), dev60, 60) * ta.stdev(ta.change(close), 60) / ta.stdev(dev60, 60)
// last two swing sizes (for the damping ratio)
var float leg_now = na
var float leg_prev = na
var float last_piv = na
if not na(ph) or not na(pl)
    piv_px = not na(ph) ? ph : pl
    if not na(last_piv)
        leg_prev := leg_now
        leg_now := math.abs(piv_px - last_piv)
    last_piv := piv_px
// volume flow (VFI 20)
inter = math.log(hlc3) - math.log(hlc3[1])
vinter = ta.stdev(inter, 30)
cutoff = 0.2 * vinter * close
vave = ta.sma(volume, 50)[1]
vc = math.min(volume, vave * 2.5)
mf = hlc3 - hlc3[1]
vcp = mf > cutoff ? vc : mf < -cutoff ? -vc : 0
vfi20 = ta.sma(math.sum(vcp, 20) / vave, 3)
// Weis wave volume ratio
var float wave_vol = 0
var float prev_wave = 0
var int wave_dir = 0
d_ = close > close[1] ? 1 : close < close[1] ? -1 : wave_dir
if d_ != wave_dir
    prev_wave := wave_vol
    wave_vol := 0
    wave_dir := d_
wave_vol += volume
weis_ratio = math.log((wave_vol + 1) / (prev_wave + 1))
// squeeze (BB inside Keltner) and SuperTrend(10, 3)
sqz_on = (ta.sma(close, 20) - 2 * ta.stdev(close, 20) > ta.sma(close, 20) - 1.5 * ta.atr(20)) and (ta.sma(close, 20) + 2 * ta.stdev(close, 20) < ta.sma(close, 20) + 1.5 * ta.atr(20))
[st_line, st_dirn] = ta.supertrend(3, 10)
// anchored VWAPs from the last swing low / high
var float apv_l = 0
var float av_l = 0
var float apv_h = 0
var float av_h = 0
if not na(pl)
    apv_l := 0
    av_l := 0
if not na(ph)
    apv_h := 0
    av_h := 0
apv_l += hlc3 * volume
av_l += volume
apv_h += hlc3 * volume
av_h += volume
avwap_pl = apv_l / av_l
avwap_ph = apv_h / av_h
// round-3 helpers
capit = ta.change(close) / atr <= -3 and volume >= 4 * ta.sma(volume, 20) and (close - low) / (high - low) <= 0.25
euph = ta.change(close) / atr >= 3 and volume >= 4 * ta.sma(volume, 20) and (close - low) / (high - low) >= 0.75
r1 = ta.roc(close, 1)
a1 = math.abs(r1)
big1 = ta.highest(a1, 20)
big2 = ta.highest(a1 == big1 ? 0 : a1, 20)
win_mom20 = math.sum(r1, 20) - (ta.valuewhen(a1 == big1, r1, 0) + ta.valuewhen(a1 == big2, r1, 0))
var int shock_b = na
var float low_since = na
var bool broke = false
if ta.change(close) / atr <= -2
    shock_b := bar_index
    low_since := low
    broke := false
else
    if not na(low_since)
        broke := broke or low < low_since
        low_since := math.min(low_since, low)
remission = na(shock_b) or broke ? 0 : bar_index - shock_b
side20 = math.sign(close - ta.sma(close, 20))
var float overshoot20 = 0
overshoot20 := side20 != side20[1] ? math.abs(close - ta.sma(close, 20)) / atr : math.max(overshoot20, math.abs(close - ta.sma(close, 20)) / atr)
m60 = ta.sma(r1, 60)
s60 = ta.stdev(r1, 60)
kurt60 = ta.sma(math.pow((r1 - m60) / s60, 4), 60) - 3
skew60 = ta.sma(math.pow((r1 - m60) / s60, 3), 60)
var int bandwagon = 0
bandwagon := close > close[1] and volume > volume[1] ? bandwagon + 1 : 0
var float inertia = 0
inertia := math.sign(ta.change(close)) == math.sign(ta.change(close)[1]) ? inertia + ta.change(close) / atr : ta.change(close) / atr
rng_slope20 = (ta.linreg(high - low, 20, 0) - ta.linreg(high - low, 20, 1)) / atr
vol_slope20 = ta.linreg(volume / ta.sma(volume, 20), 20, 0) - ta.linreg(volume / ta.sma(volume, 20), 20, 1)
anneal20 = -rng_slope20 * vol_slope20 * math.sign(vol_slope20) * math.sign(-rng_slope20)
au = close > close[1] ? 1.0 : 0.0
bu = volume > volume[1] ? 1.0 : 0.0
p11 = ta.sma(au * bu, 60) + 1e-9
p10 = ta.sma(au * (1 - bu), 60) + 1e-9
p01 = ta.sma((1 - au) * bu, 60) + 1e-9
p00 = ta.sma((1 - au) * (1 - bu), 60) + 1e-9
pa1 = p11 + p10
pb1 = p11 + p01
mi60 = (p11 * math.log(p11 / (pa1 * pb1)) + p10 * math.log(p10 / (pa1 * (1 - pb1))) + p01 * math.log(p01 / ((1 - pa1) * pb1)) + p00 * math.log(p00 / ((1 - pa1) * (1 - pb1)))) / math.log(2)
// Diamond vetoes: hype, money flow, gold band, mountain, bubble (same formulas as DIAMOND_TOTAL)
signed_v = close > close[1] ? volume : close < close[1] ? -volume : 0
hype = ta.ema(100 * ta.ema(signed_v, 10) / ta.ema(volume, 10) / 2, 3)
mfi_raw = ta.mfi(hlc3, 14)
flow = ta.ema(mfi_raw - 50, 3)
gold_top = math.max(ta.ema(close, 21), ta.ema(close, 34), ta.ema(close, 55))
band_up = ta.ema(close, 21) > ta.ema(close, 55) and ta.ema(close, 21) > ta.ema(close, 21)[1]
mountain = (hype > 0 ? 1 : 0) + (flow > 0 ? 1 : 0) + (close > gold_top ? 1 : 0) + (band_up ? 1 : 0)
bubble = math.max(0, (close - ta.ema(close, 200)) / atr) + (hype > 25 ? 1 : 0) + (flow > 25 ? 1 : 0)
// Dragon: TE line and the SS candle
te_trip = (ta.ema(ohlc4, 21) + ta.ema(ohlc4, 34) + ta.ema(ohlc4, 68)) / 3
te_v7 = ta.linreg(te_trip, 6, 0)
kdj_rsv = (close - ta.lowest(low, 9)) / (ta.highest(high, 9) - ta.lowest(low, 9)) * 100
var float kdj_k = 50.0
var float kdj_d = 50.0
kdj_k := (nz(kdj_rsv, 50) + 2 * kdj_k) / 3
kdj_d := (kdj_k + 2 * kdj_d) / 3
kdj_j = 3 * kdj_k - 2 * kdj_d
var float red_hi = na
var float red_lo = na
if kdj_j > 50 and kdj_j[1] <= 50
    red_hi := high
    red_lo := low
ss_confirm = kdj_j > 50 and not na(red_hi) and close > red_hi and not (close < red_lo)
var int dstreak = 0
var int ustreak = 0
dstreak := close < close[1] ? dstreak + 1 : 0
ustreak := close > close[1] ? ustreak + 1 : 0
[diplus, diminus, adx14] = ta.dmi(14, 14)
[macdLine, signalLine, _h] = ta.macd(close, 12, 26, 9)
"""


def build(rule: str, name: str, strategy: bool) -> str:
    conds = []
    inputs = []
    for i, c in enumerate(rule.split(" & ")):
        m = COND.match(c.strip()); b, op, thr = m["b"], m["op"], float(m["thr"])
        pine_op = {"≤": "<=", "≥": ">=", "=": "=="}[op]
        inputs.append(f"thr{i} = input.float({thr:.6g}, \"{b} {op}\")")
        conds.append(f"c{i} = ({PINE[b]}) {pine_op} thr{i}")
    on = " and ".join(f"c{i}" for i in range(len(conds)))
    head = (f'//@version=5\nstrategy("{name}", overlay=true, default_qty_type=strategy.cash, default_qty_value=10000, commission_type=strategy.commission.percent, commission_value=0.05, initial_capital=100000, pyramiding=0)\n'
            if strategy else f'//@version=5\nindicator("{name}", overlay=true)\n')
    body = ("// Evolved on 496,799 labelled random-entry bars across 2,174 US stocks (2016-23 sequential eras, 2024+ holdout).\n"
            f"// Rule: {rule}\n" + PRELUDE + "\n".join(inputs) + "\n" + "\n".join(conds) + f"\non = {on}\nsignal = on and not on[1]\n"
            "target_mult = input.float(2.0, \"target (ATR)\")\nstop_mult = input.float(4.0, \"stop (ATR)\")\nbars_max = input.int(20, \"time stop (bars)\")\n"
            "plotshape(signal, style=shape.triangleup, location=location.belowbar, color=color.new(color.lime, 0), size=size.small, title=\"entry\")\n"
            "bgcolor(on ? color.new(color.lime, 92) : na)\nalertcondition(signal, \"evolved entry\", \"{{ticker}} evolved entry\")\n")
    if strategy:
        body += ("var float ent = na\nvar int ent_bar = na\nif signal and strategy.position_size == 0\n    strategy.entry(\"L\", strategy.long)\n"
                 "if strategy.position_size > 0 and na(ent)\n    ent := strategy.position_avg_price\n    ent_bar := bar_index\n"
                 "if strategy.position_size > 0\n    strategy.exit(\"X\", \"L\", limit=ent + target_mult * atr[bar_index - ent_bar], stop=ent - stop_mult * atr[bar_index - ent_bar])\n"
                 "    if bar_index - ent_bar >= bars_max\n        strategy.close(\"L\")\nif strategy.position_size == 0\n    ent := na\n    ent_bar := na\n")
    return head + body


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--rule", required=True); ap.add_argument("--name", required=True); ap.add_argument("--out", required=True)
    a = ap.parse_args(); out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    slug = re.sub(r"[^a-z0-9]+", "_", a.name.lower()).strip("_")
    (out / f"{slug}.pine").write_text(build(a.rule, a.name, False), encoding="utf-8")
    (out / f"strategy_{slug}.pine").write_text(build(a.rule, a.name + " (strategy)", True), encoding="utf-8")
    print(out / f"{slug}.pine")


if __name__ == "__main__":
    main()
