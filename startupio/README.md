# Startup.io Indicator Suite → open TradingView clone

Five Pine v5 scripts that rebuild the Startup.io Indicator Suite layout (startup.io, Wayne Liang & Sun Liao,
invite-only on TradingView, $99/mo on Whop). Install top → bottom to get their screen:

| # | File | Clones | Pane |
|---|------|--------|------|
| 1 | `1_startupio_bravo_price_overlay.pine` | **Bravo + Voila** – Bravo = teal/purple/white candles, price-pane diamonds (with ✕ invalidation), ranked short-term shapes, 4H/D/W confirmation table; Voila = Gold band, Silver percentile channels, Green/Purple pivot levels | price chart |
| 2 | `2_startupio_echo_hype_wave.pine` | **Echo** (ex-"Delta") – "Hype wave" (volume-driven momentum, ±50) with diamond lane, fastest diamonds | own pane |
| 3 | `3_startupio_tango_money_flow.pine` | **Tango** (ex-"Action") – "Smart money flow" (MFI-based, ±50) with diamond lane, slowest/safest diamonds | own pane |
| 4 | `4_startupio_mountain.pine` | **Mountain** (ex-"Flow", the "whale tracker") – 0–4 columns (purple / grey / green) | own pane |
| 5 | `5_startupio_bubble.pine` | **Bubble Detector** – overheat columns for major tops (D / W only) | own pane |

Official names and pane order come from Wayne Liang's V7 walkthrough (Sep 2025): Echo on top, price with Bravo +
Voila, then Tango, then Flow/Mountain — exactly the four panes in the IREN screenshot (the price pane's "2"
badge = Bravo + Voila). Wayne: use it on 4H or higher ("I personally use it on the weekly"); the other
invite-only panes (Phase, Rhythm, Cycle) are "noise" he doesn't use.

All five pass a Pine v5 syntax parser; they were not compiled on a live TradingView login. If "Add to
chart" shows a red error line, paste the line back — it will be a one-line fix.

## The official rules (from Sun Liao's guide videos and the founders' posts) and where they live

**Diamonds — "Blue diamonds are bullish (buy). Pink diamonds are bearish (sell)."** Three indicators each give one
confirmation: *Bravo, Echo, Tango* — "3 total confirmations possible, we need at least 2 to proceed". Wayne
Liang: "Buy blue diamonds. Sell pink diamonds. Rinse and repeat." Pink diamonds "can be invalidated".
→ Script 1 prints Bravo diamonds on the candles (blue below / pink above) and shows the 2-of-3 verdict plus the
last diamond of each indicator on 4H / D / W (Sun's "3/3 blue diamonds on 4H/1D/1W"). Scripts 2 and 3 print
their diamonds in the fixed lane at −30, exactly like the original panes.
Wayne, V7: each diamond replaced a *crossover* ("instead of crossovers, you're going to see blue diamonds"),
Echo's diamond "comes in much faster than Bravo… faster than Tango", "the diamond is confirmed on the next
candle", "buy when there's two or three blue diamonds, sell when there's two or three pink". Diamonds "can be
invalidated" when the diamond bar's high/low doesn't break right away → all three scripts mark such diamonds
with ✕ (input: bars allowed for the break, default 3) and raise an "INVALIDATED" alert.
Sun's entry checklist (Oct 2025): "1. Above gold support. 2. Fundamental moat. 3. Maxed out mountain.
4. Triple blue diamonds. 5. Weak resistance." A newer "9/9 bullish confirms" score exists in their 2026 posts;
its nine components are not public.

**Shapes — "Non-diamond shapes and order of importance: 1. any combo of shapes, 2. squares with X, 3. squares,
4. triangles, 5. circles. Bearish above candle, bullish below. Shapes mark short-term inflection points; they
show up faster than diamonds, by design. Diamonds mark longer-term inflection points."**
→ Script 1: circle = hype-wave slope turn, triangle = hype wave crosses its signal, square = money-flow wave
crosses its signal, X = price/hype-wave divergence; combos appear naturally when several fire on one bar.

**Support / resistance — "3 types: 1. Gold levels, 2. Green/Purple levels, 3. Silver levels. When levels stack,
focus. To breach a level we want candle closes, not wicks. Price bouncing at support is bullish (entry), price
rejecting at resistance is bearish (exit)."**
→ Script 1 (the Voila half): Gold = EMA 21/34/55 band (tan); Green = last confirmed pivot low below price,
Purple = last confirmed pivot high above price, both drawn as cross rows and dropped once a close breaches
them; Silver = nested 75/85/95-percentile channels of highs and lows over 100 bars. Wayne's V7 hierarchy:
grey upper/lower bands strongest ("the closer it is to the upper band, the more likely it is going to
reverse"), gold line second ("retests… could indicate a solid entry point"), green-teal/purple crosses weakest.

**Mountain — "validates the diamonds: no columns = lots of bears; purple = some bulls but mostly bears; grey =
roughly equal; green = lots of bulls. Prices continue up until green columns turn grey again."**
→ Script 4: count of {hype wave > 0, money flow > 0, close above the gold band, gold band rising}: 0 → no
column, 1 → purple, 2 → grey, 3–4 → green. In V7 this pane was "Flow — the whale tracker": no whale activity
= nothing at zero, green = whale/consumer buying pressure, "purple is retail and you don't want that", maxed
out = "a very good sign to enter anything". Their whale/retail volume classifier is private; the count is a
stand-in that reproduces the staircase and colour semantics.

**Bubble — "predicts major tops, daily and weekly timeframes only; brighter columns = more overheated."**
→ Script 5: distance above the 200-EMA in ATR units + bonus when both waves sit in their upper zones; the
column colour brightens toward pink as it approaches the overheated line.

**White candles** are our inference: the first close that breaches the gold band (either direction) — the
founders' examples (SLNH Nov-17-2025 and Apr-20-2026, ONDS Nov-24-2025) are all first closes through the gold
level.

## Calibration evidence

Replaying scripts 2–3 on real data against the founders' screenshots:

- IREN weekly Echo diamonds: script fires blue **Apr 21** and pink **Jul 21** 2025 — the same bars as the
  screenshot; the Sep-1 blue fires Aug 18 and the Nov-3 pink fires Nov 10 (±2 bars).
- IREN weekly Tango pane: MFI(14) − 50 tracks the cyan area within a few points (−4 May → +17/+29 June →
  +42…+48 Aug–Oct → +40 early Nov).
- ONDS daily Echo: pink Oct 10 vs ≈Oct 8, blue Nov 7 vs ≈Nov 7. SLNH weekly Bravo: blue Sep 22 vs Sep 15,
  pink Nov 10 vs the Nov 17 white candle.
- Mountain: the staircase shape and colours match, but our count reaches 4 earlier than theirs (Jun 16 vs
  Aug 25 on IREN) — their four private conditions are stricter. Tune the inputs per instrument.

## What is confirmed vs. reconstructed

- **Confirmed (their words):** diamond colours and the 2-of-3 Bravo/Echo/Tango rule; shape hierarchy and
  above/below meaning; the three level types and the close-not-wick rule; Mountain colour semantics; Bubble
  purpose; candle colours teal/purple.
- **Reconstructed:** every formula. Startup.io has never published one; the hype wave, money flow, gold/silver/
  green-purple constructions, diamond/shape triggers and the Mountain conditions are behavioural fits to
  their screenshots and real OHLCV data, documented in each script header.
