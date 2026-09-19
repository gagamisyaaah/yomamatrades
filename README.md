# TradingView clones of two proprietary indicator suites

Open Pine v5 rebuilds of two paid/invite-only chart platforms, written from public docs, videos,
leaked formulas and calibration against real OHLCV. All scripts parse as valid Pine v5. They were
**not** compiled on a live TradingView account — if "Add to chart" shows a red error line, paste
it back and it's a one-line fix.

## [`homily/`](homily/README.md) — Homily Chart (弘历软件) · DannyTrades' layout

Six scripts covering DannyTrades' (@dannycheng2022) daily chart: BBE 4-colour candles + ribbon,
CD/DC "momentum bars" + Profit / Hung Up readouts, TE Trend Expert, MCDX Six-Color Dragon on a real
cost-distribution engine, CN-style MACD, and RSI ×3. Includes the **Volatility Hole** (Homily TBH)
calibrated to reproduce Danny's own boundaries within a few tenths of a percent.

## [`startupio/`](startupio/README.md) — Startup.io Indicator Suite

Five scripts covering Wayne Liang / Sun Liao's suite (invite-only on TradingView, $99/mo on Whop):
**Bravo + Voila** on the price pane (diamonds, ranked shapes, gold / silver / green-purple levels),
**Echo** hype-wave pane, **Tango** money-flow pane, **Mountain** (ex-Flow) 0–4 whale-tracker, and
**Bubble Detector** for major-top overheat. Diamond ✕-invalidation and the 2-of-3 confirmation rule
are wired in, along with the 4H/D/W confirmation table.

## Install order per suite

Add scripts top-to-bottom to reproduce the original screen layout. Each folder's README has the full
reading rules, calibration evidence, and what is confirmed vs. reconstructed.
