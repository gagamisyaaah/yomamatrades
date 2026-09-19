# yomamatrades — Dragon & Diamond, read as total signals, tested on every stock

Open Pine v5 rebuilds of two proprietary chart suites, the two-indicator "total signal" readers built from them,
and the lab that measured them: a browser-driven TradingView harness plus a Python miner that runs the same panel
formulas over every US stock above $2B.

| folder | what |
|---|---|
| [`dragon/`](dragon/) | **`DRAGON_TOTAL.pine`** — the five Dragon (Homily / DannyTrades) panels read together as one signal. Default reading = the one that survived every test (hole break + whales + TE, Diamond vetoes, market guard); exit modes for 40 %, 65 % and 85 % win rates; tiers D/E/F; GO/OUT marks, panel table, alerts. `strategy_dragon_twin.pine` = its Strategy-Tester twin. |
| [`diamond/`](diamond/) | **`DIAMOND_TOTAL.pine`** — the four Diamond (Startup.io) panels read together (confirmed diamonds, Mountain, gold band, purple resistance, bubble), calibrated by a 54-setting sweep. `strategy_diamond_twin.pine` = its twin. |
| [`homily/`](homily/README.md), [`startupio/`](startupio/README.md) | the eleven single-panel clones (your "Dragon one–five" and "Diamond one–four") with the reading rules |
| [`lab/`](lab/README.md) | the harness (compile lab, Strategy-report harvester, ranker, X miner) and `lab/mine/` (Python ports of every panel, feature tables, event backtests vs controls, walk-forward, portfolio simulator, hit-rate frontier, Diamond sweep, daily scanner) |
| [`vcp/`](vcp/) | a volatility-contraction overlay/pane — side experiment, not part of the system |

Everything compiled clean on a live TradingView chart. Two active indicators are enough: DRAGON TOTAL + DIAMOND TOTAL.

## What the tests say — read in this order

Method everywhere: one fixed-size unit per trade, entered at the **next open** after the signal bar, closed by the
exit rule; **TOTAL** = the sum of trade returns (no compounding). Out-of-sample = signals from 2024-01-01. Controls =
random-day entries with the same exit. Two universes: the **94 tuning tickers** (your nine + momentum names, where the
rules were chosen) and a **546-ticker holdout** (S&P 500 and other ≥ $2B names never used to choose anything).
Full tables: `lab/mine/evaluate.md`, `walkforward.md`, `walkforward_holdout.md`, `frontier.md`, `portfolio*.md`,
`diamond_sweep.md`, `ground_truth.md`.

### 1. The reading that works: "Dragon C"

Volatility Hole **BROKE UP** + whales ≥ 50 % + TE line rising, vetoed by the Diamond panels (bubble not hot,
Mountain ≥ 3, close above the gold band), exit **only** when the hole breaks down. Requiring Diamond confirmations as
co-triggers diluted it; Diamond earns its place as the veto layer, not as a second trigger.

| universe (2024+) | trades | avg per trade | PF | winners | signals / week | random entries, same exit |
|---|---|---|---|---|---|---|
| 94 tuning tickers | 1,597 | **+11.8 %** | 2.46 | 41 % | 11 | +2.8 % |
| 546-ticker holdout, all | 7,630 | **+2.4 %** | 1.69 | 39 % | 54 | +0.7 % |
| holdout, only names with ATR ≥ 5 % (tier F) | 231 | **+25.3 %** | 4.99 | 49 % | 1.6 | — |
| holdout, ATR < 3 % (calm large caps) | ≈ 5,600 | ≈ +1 % | ≈ 1.5 | 38 % | — | +0.7 % |

The edge is real on names it was never tuned on, and its **size scales with volatility**: on calm large caps it is
a few tenths of a percent per trade, on volatile names it is the +11–25 % you see on your watch-list. That is the
quantitative form of "scan everything, trade only the ideal ones": the ideal ones are the volatile names where the
whole reading lines up.

Tiers (on the 94 set, 2024+): fresh break ≤ 3 bars → +12.9 %; + ≥ 6 of 7 panels → +14.2 %; ATR ≥ 5 % names → +17.6 %.

### 2. The year that breaks it, and the guard

Per signal-year, every Dragon variant lost in **2022** (≈ −10 % per trade on the tuning set, −2 % on the holdout,
PF 0.2–0.5) and was flat in 2018 and 2021; 2020 and 2024 carry the averages. It is a bull-regime breakout system.
**Market guard** (no entries while the S&P 500 is below its 200-day line, exit when it crosses below) cut 2022 on the
tuning set from 237 trades at −9.6 % to 52 at −4.3 %, made 2018 flat, and left 2024+ at +11 %. It is ON by default.

### 3. Win rate is set by the exit, the edge by the panels, the size by volatility

Same signals, different exit geometry (target / stop in ATR of the signal bar, first touched wins):

| exit | 94 tuning set: winners / avg | holdout, all: winners / avg | holdout, ATR ≥ 5 %: winners / avg (2024+) |
|---|---|---|---|
| Scalp +0.5 / −4 ATR, 20 bars | 88 % / +0.09 ATR | 88 % / +0.05 ATR | 87 % / +0.04 ATR |
| Scalp +0.5 / −3 ATR, 10 bars, tier E + ATR ≥ 5 % | 85 % / +0.09 ATR | — | 80 % / +0.03 ATR |
| Bracket +2 / −4 ATR, 20 bars | 64 % / +0.29 ATR | 62 % / +0.15 ATR | **68 % / +0.42 ATR (≈ +2 %)** |
| Bracket +3 / −4 ATR, 20 bars | 56 % / +0.43 ATR | 56 % / +0.18 ATR | 60 % / +0.47 ATR |
| Hole broke down (runner) | 41 % / ≈ +2.4 ATR | 39 % / ≈ +1 ATR | 49 % / ≈ +5 ATR |

So "80 % of trades profitable" exists and holds out of sample, but only as a scalp whose per-trade profit is at
commission level on calm names. The robust money-maker is the +2 / −4 bracket on volatile names: two thirds of
trades win, about +2 % each, with the guard on. A breakeven stop after +1 ATR was tested and rejected (it turns
losers into scratches and lowers both the win rate and the expectancy). Pullback-limit entries have zero expectancy
at small targets. All three exits are selectable in DRAGON TOTAL.

### 4. Diamond

Calibration sweep (54 settings): keep wave travel 10 (it puts our diamonds on the founders' own bars), strict entry
only; confirmation window 5 → +8.9 % per trade, 2.8 signals/week on the tuning set; window 2 + strict Mountain →
+11 %, PF > 3, 1.2/week. **But on the holdout Diamond strict makes +0.3 % per trade (PF 1.14), below the random
control.** Its edge does not generalise beyond momentum names; its Mountain / gold / bubble states do generalise as
Dragon's vetoes (C beat A on the holdout: +2.42 vs +2.10). DIAMOND TOTAL stays as the second indicator for reading the
Diamond panels; do not trade it alone on calm names.

### 5. Portfolio, not per-trade averages ($100k, 1-ATR move = 1 % of equity, ≤ 10 names, no leverage, 0.05 % each way)

| account (2016 → 2026) | final | CAGR | worst drawdown | 2022 | trades taken |
|---|---|---|---|---|---|
| Dragon C, tuning set | $11.8M | +61 % | −77 % | −59 % | 419 (0.8 / week) |
| Dragon C + market guard (G), tuning set | $4.4M | +46 % | −60 % | −17 % | 446 |
| random entries, tuning set | $6.0M | +51 % | −77 % | −64 % | 1,359 |

Read the random row first: this watch-list compounds on its own because it is made of names that already ran
(survivorship). The signal's own worth is the gap to that row, and the guard is what makes the account survivable.
Slot policy: when more names signal than the account can hold, taking the **best setups first** (fresh break,
≥ 6 panels, Mountain maxed, higher ATR) beat "biggest mover first" in every guarded configuration ($7.7M vs $4.4M at
1 % sizing). At 1 % sizing the no-leverage cap binds and five names fill the book; 0.5 % sizing with 20 slots takes
more of the signals. Broad-universe portfolio: `lab/mine/portfolio_broad.md`.

### 6. Ground truth, alerts, library

- Founders' posts: 104 mined from X (Danny mostly; Wayne's handle returns nothing). Directional calls agree with our
  bullish state 56 % of the time; Dragon C was ON at 24 % of Danny's bullish calls. Thin and recent-heavy; the
  pipeline (`lab/mine_x.py`, `lab/mine/ground_truth.py`) is in place for a longer pull.
- TradingView alerts: the Basic plan allows **0 technical alerts** (`lab/basic_plan_zero_alerts.png`). Until the
  plan changes, the notification path is the daily scanner below.
- Public library sweep: of the 60 most-liked open scripts across 15 themes, 9 were wrappable into strategies and the
  first two backtested worse than Dragon on your tickers; the sweep was stopped on your call (`lab/scout/`).

## Daily use

1. Keep `DRAGON_TOTAL` (default: reading C, market guard on, exit "Hole broke down") and `DIAMOND_TOTAL` on the chart.
   Switch DRAGON's exit to "Bracket +2 / −4 ATR" for ~65 % winners at ≈ +2 % per trade on volatile names, or
   "Scalp +0.5 / −3 ATR" for ~85 % winners at commission-thin profit.
2. Every evening: `cd lab && python3.12 -m mine.scan --refresh --top 40` → `lab/mine/candidates_<date>.md`, the
   ranked list over every cached ticker (tiers E > D > C, +F for ATR ≥ 5 %, TypeSafe quality score). Buy the top of
   the list at the next open, size by ATR, respect the guard.
3. Universe: `python3.12 -m mine.data --universe broad` downloads S&P 500 + every US common stock ≥ $2B (≈ 2,000
   names) via the Nasdaq history API; `python3.12 -m mine.features --out=features_broad` and
   `python3.12 -m mine.walkforward --features features_broad --holdout` re-run the holdout on the full set.

## Caveats you must keep in mind

The 2024–2026 window is bullish and the tuning watch-list is survivorship-biased; the holdout table is the honest
number. Fills are next-open with 0.05 % commission and no slippage; the scalp designs are the most sensitive to
that. Every formula is a reconstruction of a closed product. Rules were chosen on the 94-ticker set after looking at
its data; the holdout and the per-year tables limit, but do not remove, that bias. Nothing here is investment advice.

## Reproduce

```bash
cd lab && make doctor && make tv-check                       # compile every script live (browser-harness + your Chrome)
python3.12 -m mine.data --universe core                       # prices (Nasdaq history API, Yahoo fallback)
python3.12 -m mine.features && python3.12 -m mine.evaluate    # panel states, readings vs controls
python3.12 -m mine.walkforward --features features            # per-year, regime filters
python3.12 -m mine.frontier && python3.12 -m mine.portfolio   # exit frontier, portfolio + slot policies
python3.12 -m mine.diamond_sweep && python3.12 -m mine.scan   # Diamond calibration, tonight's candidates
make harvest                                                  # Strategy-report grid on TradingView (slow)
```
