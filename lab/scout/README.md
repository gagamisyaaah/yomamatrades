# scout — community-library scout + strategy twins

## What was scouted

Search endpoint: `https://www.tradingview.com/pubscripts-suggest-json/?search=<term>` (the JSON the site's own search box uses; 50 cards per page, page 2 via its `next` offset cursor). The HTML listing `/scripts/?text=<term>` was fetched too but ignored: it embeds the same 24 hot-feed cards for every term and for `&page=2`, so the term is only applied client-side.

| term | cards (2 pages) |
|---|---|
| breakout | 100 |
| squeeze | 100 |
| volatility contraction | 8 |
| momentum | 100 |
| trend | 100 |
| reversal | 99 |
| volume | 100 |
| smart money | 100 |
| whale | 100 |
| chip distribution | 1 |
| diamond | 69 |
| swing | 100 |
| buy sell signal | 100 |
| supertrend | 100 |
| trend following | 100 |

Cards found: **1277**, unique ids: **1217**, open-source: **979**, sources fetched (top 60 open-source by likes): **60** → `scout/src/<id>.pine`, facade JSON cached in `scout/raw/<id>.json` (file names use `PUB_…`, the `;` of the PUB id is replaced by `_`).

## How wrapping works (`scout_wrap.py`)

1. Keep only `//@version=5` / `6` scripts that declare `indicator()` and contain `alertcondition()`; scripts that are already a `strategy()` (or a `library()`) are skipped.
2. Each `alertcondition(condition, title=…, message=…)` (keyword or positional) is parsed with a string/comment-aware tokenizer; the condition expression text is kept verbatim.
3. TypeSafe (jev) answers one choice question per alert — enter_long / exit_long / enter_short / exit_short / none — from the script name, alert title, message, condition snippet and the other alert titles in the script. Answers + confidence are cached in `scout/wrapped/<id>.json`.
4. `indicator(...)` becomes `strategy(title=…, shorttitle=…, overlay=…, initial_capital=10000, default_qty_type=strategy.cash, default_qty_value=10000, commission 0.05 %, process_orders_on_close=false, calc_on_every_tick=false)`; every other indicator() argument is dropped.
5. Appended at the end: `__enterL` / `__exitL` (OR of the classified conditions, `false` when none), long-only `strategy.entry("L")` / `strategy.close("L")`, and the lab's verbatim metrics block (BH, Alpha, TPW, Bear, Bull, Short, Exp status-line plots). Shorts are never traded; a script with no enter_long alert is skipped.
6. `pynescript` parses every wrapped file (column `syntax`); the original is parsed too when the wrapped one fails, so a parser limitation is not blamed on the wrapper.

## Limits

- alertcondition conditions are re-used verbatim at global scope; a condition that references a variable declared inside a local block (`if`, function, method) will not compile on TradingView.
- The metrics block declares `c0 t0 bhPct netPct weeks tpw`; a script that already uses one of these names fails to compile (flagged in `note`).
- Dropping `max_lines_count` / `max_labels_count` / `max_boxes_count` from the declaration only changes how many drawings stay on the chart, not the signals.
- The alert classification is a judgment from title/message text; an alert that fires on both directions ("any signal") is `none` and contributes nothing.
- `pynescript` is a parser only: a `pass` means the file lexes/parses, not that it compiles; a `fail` on a file whose original also fails is a parser gap, not a wrapper bug.
- Search results are TradingView's relevance ranking, two pages per term; likes are the `agreeCount` at scouting time.

## Wrapped scripts (sorted by likes)

| likes | script | author | alerts | enter_long | exit_long | syntax | file |
|---|---|---|---|---|---|---|---|
| 167524 | Smart Money Concepts (SMC) [LuxAlgo] | LuxAlgo | 16 | 8 | 7 | timeout | `PUB_6daafb2cabe6419d98ae25229d2327f8.pine` |
| 52119 | Trendlines with Breaks [LuxAlgo] | LuxAlgo | 2 | 1 | 1 | pass | `PUB_1a32fae03299466690dcdde76d812c78.pine` |
| 30579 | Machine Learning Adaptive SuperTrend [AlgoAlpha] | AlgoAlpha | 6 | 1 | 1 | timeout | `PUB_84c58fb9947d4713a23d145d97e74d28.pine` |
| 30508 | Smart Money Breakout Channels [AlgoAlpha] | AlgoAlpha | 3 | 1 | 1 | timeout | `PUB_8c2d234156044effa75d531d82b247b3.pine` |
| 18460 | Self-Aware Trend System [WillyAlgoTrader] | WillyAlgoTrader | 22 | 8 | 14 | timeout | `PUB_0f80bcf05d544d4c98fde06faab1c976.pine` |
| 16609 | Zero Lag Trend Signals (MTF) [AlgoAlpha] | AlgoAlpha | 22 | 11 | 11 | fail | `PUB_d7eefaf9a1ea4811bb0cbc0c1d9a7334.pine` |
| 15537 | Trend Targets [AlgoAlpha] | AlgoAlpha | 7 | 2 | 5 | fail | `PUB_92ff5628d7c643419a350022e0e3866c.pine` |
| 14513 | Buy Sell Signal | kelfry98 | 5 | 1 | 4 | timeout | `PUB_92a018ea21ef4b9886e810a7fea6968d.pine` |
| 13405 | SuperTrend [everget] | everget | 3 | 1 | 2 | fail | `PUB_nlyrgnVJ8Zrisan2g01Sfv5x9RBKvpPL.pine` |

## Skipped

| script | reason |
|---|---|
| Zero-Lag MA Trend Levels [ChartPrime] | no alertcondition() |
| Volume Profile with Node Detection [LuxAlgo] | no alertcondition() |
| Indicator: OBV Oscillator | pine vNone |
| Volume Flow Indicator [LazyBear] | pine vNone |
| Pivot Points High Low & Missed Reversal Levels [LuxAlgo] | no alertcondition() |
| Squeeze Momentum Indicator [LazyBear] | pine vNone |
| Deviation Trend Profile [BigBeluga] | no alertcondition() |
| Indicator: Weis Wave Volume [LazyBear] | pine vNone |
| Auto Chart Patterns [Trendoscope®] | no alertcondition() |
| Smart Money Concepts Probability (Expo) | no alertcondition() |
| Momentum-based ZigZag (incl. QQE) NON-REPAINTING | no alertcondition() |
| Smart Money Concept [TradingFinder] Major OB + FVG + Liquidity | no alertcondition() |
| Volume Profile, Pivot Anchored by DGT | no alertcondition() |
| Cumulative Delta Volume | pine v4 |
| Opening Range with Breakouts & Targets [LuxAlgo] | no alertcondition() |
| CM EMA Trend Bars | pine vNone |
| Trendline Breakouts With Targets [ChartPrime] | no alertcondition() |
| Trend Type Indicator by BobRivera990 | pine v4 |
| Reversal Signals [LuxAlgo] | no alertcondition() |
| Liquidity Swings [LuxAlgo] | no alertcondition() |
| Fibonacci Trend [ChartPrime] | no alertcondition() |
| SuperTrend AI (Clustering) [LuxAlgo] | no alertcondition() |
| Stochastic OTT | pine v4 |
| Candlestick Trend Indicator v0.5 by JustUncleL | pine vNone |
| Pivot Point Supertrend | pine v4 |
| Trend Lines v2 | pine v4 |
| Optimized Trend Tracker | pine v4 |
| Volume Profile [LuxAlgo] | pine v4 |
| Volume Profile Free Ultra SLI (100 Levels Value Area VWAP) - RRB | pine v4 |
| SuperTrend EXPLORER / SCREENER | pine v4 |
| SuperTrend | pine v4 |
| Volume Profile | no alertcondition() |
| Trend Direction Helper (ZigZag and S/R and HH/LL labels) | pine v4 |
| Smart Money Structure | GainzAlgo | no alertcondition() |
| Support and Resistance (High Volume Boxes) [ChartPrime] | no alertcondition() |
| Volume-based Support & Resistance Zones | no alertcondition() |
| Volume Profile / Fixed Range | no alertcondition() |
| Volume Delta Candles [LuxAlgo] | no alertcondition() |
| Breakout Probability (Expo) | no alertcondition() |
| Adaptive Trend Finder (log) | no alertcondition() |
| Volume Profile and Volume Indicator by DGT | no alertcondition() |
| Market sessions and Volume profile - By Leviathan | no alertcondition() |
| Volume Orderbook (Expo) | no alertcondition() |
| Price Action Smart Money Concepts [BigBeluga] | no alertcondition() |
| Dynamic Swing Anchored VWAP (Zeiierman) | no alertcondition() |
| VuManChu Swing Free | pine v4 |
| Volume / Open Interest "Footprint" - By Leviathan | no alertcondition() |
| Swing Profile [BigBeluga] | no alertcondition() |
| Swing Highs/Lows & Candle Patterns [LuxAlgo] | no alertcondition() |
| SuperTrend STRATEGY | pine v4 |
| Breakout Finder | pine v4 |
