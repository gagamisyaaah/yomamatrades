# tv_lab — TradingView indicator lab (browser-harness + jev + TypeSafe)

Drives **your real, logged-in Chrome** through Chrome DevTools (browser-harness, the layer jev-ultrafast sits on),
so TradingView's UI becomes an API: compile Pine, run the Strategy Tester, sweep inputs, export data, scout
the community library, create alerts. TypeSafe (jev model) turns the messy text that comes back into typed
judgments. The chart canvas is never read — everything is DOM.

## One-time setup
1. Chrome → `chrome://inspect/#remote-debugging` → tick **Allow remote debugging for this browser instance**.
   On macOS, if Chrome asks for permission, run `browser-harness mac-approve` once.
2. `.env` in this folder holds `TYPESAFE_API_KEY` (done). Add an OpenRouter key as `TEXT_MODEL_API_KEY`
   only if you want jev's natural-language goals (the deterministic lab below needs no text model).
3. `make doctor` must show `daemon alive` and `active browser connections — 1`.

## The eight ideas → commands
| # | Idea | Command | Output |
|---|------|---------|--------|
| 1 | Compile lab | `make tv-check` | `compile_report.csv` — per script: added to chart?, console text, TypeSafe verdict |
| 2 | Strategy Tester grid | `make smoke` then `make harvest` | `backtest_grid.csv` — 14 signals × 9 tickers × 4H/D/W with PF, net %, win %, max DD, trades, TypeSafe robustness/overfit |
| 3 | Parameter sweep | `make sweep` (edit knob/values) | `sweep_<knob>.csv` |
| 4 | Ground-truth mining | `make mine` | `x_posts.jsonl`, `x_claims.csv` — every founders' post typed as signal/has-numbers/conviction |
| 5 | Export + offline eval | `python3.12 export_eval.py export …` then `eval ~/Downloads/*.csv` | forward-return table per event column |
| 6 | Community scout | `make scout` | `corpus/<term>/*.pine` + `manifest.json` |
| 7 | Regression | `make tv-check` after every edit (same as 1) | fails loudly on any red line |
| 8 | Alerts | `python3.12 alerts.py --title "SU Echo"` | scaffold; finishes once the alert dialog DOM is confirmed live |

Strategy twins live in `strategies/` (one per suite; the `Signal` input picks the family: Homily = SS candle,
BBE ribbon, Volatility hole, MCDX whales, TE main line, MACD zero, RSI×3; Startup.io = Echo/Tango/Bravo
diamonds, 2-of-3 diamonds, Mountain green, hype-wave zero, gold band). Long-only, 100 % equity, 0.05 %
commission, orders on close.

## Your TradingView plan sets the rules (Basic)
- **One layout.** "Create new layout…" opens the upgrade modal, so the lab runs inside your single "Unnamed"
  layout (tab id in `.lab_tab`). It only ever adds to, and removes from, that chart.
- **Indicator cap.** Two active at once, about five selected (active + hidden) before the upgrade popup.
  The lab therefore keeps **one** twin/clone on the chart at a time and removes it before the next one
  (`tvh.remove_indicator`, which selects the legend title and presses Delete).
- **Never your saved scripts.** Every paste goes into a fresh *Untitled* script (`Create new → Indicator/Strategy`).
  Unsaved Untitled scripts add to the chart with no "Save this script before adding?" prompt; the saved ones
  ("Dragon one–five", "Diamond one–four", "Homily Flow …") are never opened or overwritten.
- The tab must be the **active** Chrome tab while the lab runs (a hidden tab drops clicks and pastes).

## Where the DOM might bite
`tvh.py` targets TradingView's `data-name` hooks (`scripteditor`, `backtesting`, `legend-source-title`,
`legend-delete-action`, `legend-settings-action`) with text fallbacks. If a step fails, run
`python3.12 -c "import tvh; tvh.connect(); tvh.open_chart(); print(tvh.probe())"` and paste the output —
selectors are adjusted in one place.

## `mine/` — the data-mining side (no browser needed)

| step | command | output |
|---|---|---|
| prices | `python3.12 -m mine.data --universe core` (or `broad` = + S&P 500 + NASDAQ-100) | `mine/cache/<TICKER>.csv` (Nasdaq history API; Yahoo with cookie+crumb as fallback) |
| panel states | `python3.12 -m mine.features` | one row per ticker-day: every Dragon and Diamond panel reading (`mine/dragon.py`, `mine/diamond.py` = the Pine formulas in pandas) + forward outcomes |
| the readings as written | `python3.12 -m mine.evaluate` | `evaluate.md` / `evaluate.csv` / `trades_ledger.csv.gz`: event backtests of each total-signal reading × 5 exits, in/out-of-sample, vs random-entry controls |
| what the panels showed at profit points | `python3.12 -m mine.mine` | `report.md` / `rules.json`: within-ticker lift of every state for +3 ATR spikes, −3 ATR spikes and tops-while-long; depth-4 trees → conjunction rules scored out-of-sample |

Rule of the lab: a reading only counts if it beats the random-entry control **with the same exit**.
