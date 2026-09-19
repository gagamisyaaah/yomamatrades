# yomamatrades — Dragon & Diamond, read as total signals, tested on every stock

Open Pine v5 rebuilds of two proprietary chart suites, **plus** the two-indicator "total signal" readers built
from them, **plus** the lab that measured them (a browser-driven TradingView harness and a 94-ticker data miner).

| folder | what |
|---|---|
| [`dragon/`](dragon/) | **`DRAGON_TOTAL.pine`** — the five Dragon (Homily / DannyTrades) panels read together as one signal, with the reading that survived the tests as default. `strategy_dragon_twin.pine` is its Strategy-Tester twin. |
| [`diamond/`](diamond/) | **`DIAMOND_TOTAL.pine`** — the four Diamond (Startup.io) panels read together as one signal (confirmed diamonds, Mountain, gold band, purple resistance, bubble). `strategy_diamond_twin.pine` is its twin. |
| [`homily/`](homily/README.md) | the six single-panel Homily clones (Dragon one–five on your TradingView) and their reading rules |
| [`startupio/`](startupio/README.md) | the five single-panel Startup.io clones (Diamond one–four) and the official reading rules |
| [`vcp/`](vcp/) | a volatility-contraction overlay + pane (nine contraction engines) and its twin — a side experiment, not part of the Dragon/Diamond system |
| [`lab/`](lab/README.md) | the harness: compile lab, Strategy-report harvester, ranking, founder-post miner, library scout, and `mine/` (Python ports of every panel, 94-ticker feature table, lift tables, learned rules, event backtests) |

All eleven clones and both TOTAL indicators compiled clean on a live TradingView chart (`lab/compile_report.csv`).
Two active indicators are enough: **DRAGON TOTAL + DIAMOND TOTAL** replace nine panels.

## What the tests say (read this before trusting any number)

**Setup.** 94 US tickers (the nine you trade + momentum names + large caps), daily bars 2016-09 → 2026-09, every
panel ported to Python with the same formulas as the Pine clones (`lab/mine/dragon.py`, `diamond.py`). A trade is
one fixed-size unit entered at the **next open** after the signal bar and closed by the exit rule; **TOTAL** is the
sum of those trades (no compounding — the "stack up" number). Out-of-sample = trades signalled from 2024-01-01.
Controls = random-day entries and "every up day" entries with the *same* exits. Full tables: `lab/mine/evaluate.md`.

**Edge over random entries, out-of-sample (avg % per trade, same exit):**

| reading | exit | trades | TOTAL % | avg % | random avg % | edge | PF | signals / week (94 tickers) |
|---|---|---|---|---|---|---|---|---|
| Dragon: Volatility Hole BROKE UP + whales ≥ 50 % + TE line rising | flip (hole breaks down or TE turns) | 1509 | +11 550 | **+7.65** | +3.01 | **+4.6** | 2.22 | 10.7 |
| Diamond: 3 confirmed blue diamonds + Mountain 4 + above gold + bubble not hot | flip (2 pink / Mountain ≤ 2 / close below gold) | 338 | +1 945 | **+5.76** | +3.01 | **+2.8** | 2.11 | 2.4 |
| Diamond: Mountain maxed + above gold | flip | 2219 | +9 056 | +4.08 | +3.01 | +1.1 | 1.90 | 15.7 |
| Dragon: all 7 panels | 10-bar time exit | 2219 | +6 815 | +3.07 | +2.11 | +1.0 | 1.64 | 15.7 |
| any Dragon / Diamond reading | chandelier 3 ATR | — | — | +3.5 … +5.5 | +5.9 | **none** | — | — |
| any bearish mirror (shorts) | any | — | negative | −1 … −5 | — | **negative** | 0.5–0.9 | — |

So: the entries carry an edge only when the exit is the reading's own flip; with a chandelier trail every entry —
including random ones — earned the same, i.e. that profit is the exit plus the bull drift, not the indicators.
Shorting the mirrored readings lost money everywhere. The "overheated" exit that the miner suggested (price > 2 ATR
above the gold band with the bubble > 8.7) *hurt* the Dragon winner (+7.65 → +3.9), so it is offered but not default.

**On TradingView itself** (`lab/backtest_grid.csv`, 2018+, daily, $10 000 per trade, long only, 8 of your tickers):
Dragon total (5 of 7) with the chandelier exit had a median profit factor of 2.5 with all 8 tickers above 1.0 and a
median summed return of +337 % of one unit (e.g. SLNH +427 % over 38 trades while holding it lost 95 %, ONDS +221 %
vs −45 %, CLSK +483 % vs −9 %). Rankings: `python3.12 lab/rank_grid.py lab/backtest_grid.csv`.

**Caveats you must keep in mind.** The universe and the 2024–2026 window were strongly bullish; bear-regime samples
out-of-sample are small (dip-and-rip episodes) so the bear rows are not a bear-market proof. Fills are next-open with
0.05 % commission and no slippage. Every formula is a reconstruction of a closed product. Two of the readings were
chosen after looking at the same data they are scored on (the train/test split limits, not removes, that bias).

## How to use the two indicators for scanning

1. Add `DRAGON_TOTAL` and `DIAMOND_TOTAL` (defaults = the two readings above). Both print GO / OUT marks and a panel
   table, and expose alerts **DRAGON go / DRAGON out / DRAGON armed** and **DIAMOND go / out / armed**.
2. Enter at the next open after a GO; leave on OUT. Position size fixed per trade — the numbers above are per unit.
3. Across 94 names the Dragon reading fires ~11 times a week and the strict Diamond ~2–3; widen the universe (the lab
   downloads any list: `python3.12 -m lab.mine.data --universe broad`) and the counts scale with it.

## Reproduce

```bash
cd lab && make doctor && make tv-check            # compile every script live (browser-harness + your Chrome)
python3.12 -m mine.data --universe core            # prices (Nasdaq history API, Yahoo fallback)
python3.12 -m mine.features && python3.12 -m mine.evaluate && python3.12 -m mine.mine
make harvest                                       # Strategy-report grid on TradingView (slow, one twin at a time)
```
