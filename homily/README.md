# Homily Chart → TradingView (DannyTrades layout)

Six Pine v5 scripts that rebuild every panel of Danny Cheng's (@dannycheng2022) Homily Chart screen.
Install each one: TradingView → Pine Editor → paste → "Add to chart". Add them top to bottom in this order
to get his layout.

| # | File | Homily panel it clones | Pane |
|---|------|------------------------|------|
| 1 | `1_homily_p1_candles_ribbon_wave_hole.pine` | **BBE / GCAND / SS / TBH** – 4-colour candles (Strong Signal), red/blue ribbon (Bull & Bear Expert), yellow wave line, **Volatility Hole** (Time Black Hole) | on the price chart |
| 2 | `2_homily_cd_dc_momentum_bars.pine` | **CD** blue volume-at-price bars = Danny's "momentum bars" (+ POC), **DC** yellow chip histogram with **Profit % / Hung Up %** | on the price chart |
| 3 | `3_homily_te_trend_expert.pine` | **TE** Trend Expert (趋势王): red/green tower bars + 7 dotted purple/green lines | own pane |
| 4 | `4_homily_mcdx_six_color_dragon.pine` | **MCDX** Six-Color Dragon (六彩神龙): whales (red) / day-traders (yellow) / retail (green) + MA lines | own pane |
| 5 | `5_homily_macd_cn.pine` | **MACD** Chinese convention (bar = 2×(DIF−DEA)) | own pane |
| 6 | `6_homily_rsi_x3.pine` | **RSI** 9 / 14 / 24 with 80 / 50 / 20 lines | own pane |

All six parse as valid Pine v5. They were **not** compiled on a live TradingView account (that needs a login);
if "Add to chart" shows a red error line, copy the line and it is a one-line fix.

## Reading rules (Danny's, mapped onto the scripts)

**Script 1 – candles.** Red = fresh short-term bullish (KDJ J-line crosses above 50). Dark blue = uptrend
persists. Yellow = fresh short-term bearish (J crosses below 50). Light blue = downtrend persists.
Danny: "the price must surpass the high point of the red candle; a close below its low negates it".

**Script 1 – ribbon.** Red fill = mid-term uptrend, blue fill = mid-term downtrend. Two lines = the
ribbon's upper/lower boundary. "Red candle hugging the blue ribbon" = bottoming setup.

**Script 1 – Wave.** The yellow zigzag now turns only when price reverses ≥ max(8 %, 2.5 × ATR14) from
the last extreme, so a stock that moves 8 % a day no longer flips every 2-3 bars. `m × ATR14` is the
knob: 2.5 (default) ≈ Danny's turn count on MRNA; use 2.0 on very volatile names if you want more turns.

**Script 1 – Volatility Hole.** One hole per confirmed turn of the Wave (v2: no longer one per swing).
Centre = midpoint of the swing from the extreme to the confirmation bar; boundaries = centre ± 1.33 ×
ATR14. Calibrated on real daily data: MRNA's Aug-2026 hole computes as ▲165.56 / ▼124.42 vs Danny's
▲165.3 / ▼125.2. A circle with cross-hair marks the hole; its two horizontal lines are the boundaries.
Label says IN FORCE while price stays inside. `Consecutive closes to break a boundary` (default 2) above
the top → "BROKE UP" (surge watch); below the bottom → "BROKE DOWN" (plummet watch); resolved holes shrink
to a tiny tag. Holes on a red ribbon = topping watch, on a blue ribbon = bottoming watch. Two turns closer
than `minGap` bars (default 5, e.g. a V-spike) keep only the later hole; only the last `Holes kept on
chart` (default 4) are drawn.

**Script 2 – momentum bars.** The longest blue bars (default top 3, labelled with price; the longest is
"POC") are his support / resistance "momentum bars". "Closing consistently above the longest momentum bar
triggers a surge". Profit % / Hung Up % = Homily's DC readouts (share of chips in profit / trapped).

**Script 3 – TE.** Red bars = bullish sentiment, green = bearish. Purple lines = mid-term uptrend,
green = mid-term downtrend. Scan signal: main line turns green → purple.

**Script 4 – MCDX.** Whales (red) ≥ 50 % → stock can run, ≥ 75 % → surge zone. Green (retail) bars
shrinking / disappearing = bullish; long green bars = drop risk. Yellow = day traders (ignore). Purple /
cyan / magenta lines = MA of each group (the "(8)" in Homily's header). Blue squares at the bottom = whales
dropped > 30 % from their 15-bar high (shake-out / distribution flag from the leaked formula).

**Scripts 5–6.** MACD above zero = bullish momentum; golden cross curling up = bullish. RSI 20–30 oversold,
80–95 sell zone; all three RSIs curling up together = bullish.

## What is proprietary vs. reconstructed

- **Exact (leaked formulas):** TE / 趋势王, MCDX / 六彩神龙 structure (profit / floating / locked chips on
  a cost distribution), Strong Signal (KDJ J vs 50), MACD, RSI.
- **Behavioural clones (formula undisclosed):** BBE ribbon line pair (default: Homily's leaked main-chart
  pair EMA((C+L)/2,8) vs EMA(EMA((C+L)/2,20),8)), the Volatility Hole trigger and boundary width, the
  chip-decay parameters of DC/MCDX (window 120 bars, ±4 % floating band).
- **Danny's own hand-drawn objects:** the yellow trend line (script 1 replaces it with an automatic
  zigzag) and the horizontal boundary lines / labels he adds around each hole.
