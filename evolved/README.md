# evolved/ — the bred indicators

Three BASE-HIT indicators, each the winner of one family lane of the evolutionary program in `lab/bend/evo/`
(exhaustive combination search over ~680 movement measurements on 497,000 labelled bars, sequential training eras
2016–23 with survival required in bear and bull regimes, 2024+ never used to select), plus the RUNNER lane in
`dragon/DRAGON_RUNNER.pine`.

| lane | script | what it is | holdout 2024+ (vs random entries, same exit) |
|---|---|---|---|
| RUNNER | `dragon/DRAGON_RUNNER.pine` (+ strategy) | volatility hole breaks up → hold until it breaks down; volatile $300M–5B names | +0.7 to +5 % per trade over an in-force entry; live: 9/10 names profitable, median +199 % vs +22 % buy-and-hold |
| BASE-HIT | `super_structure_washout_at_the_low.pine` | 20-bar path dominated by drawdown, close at the day's low, at the last swing low, near the 50-bar low | +0.20 ATR, 64 % winners, n 1,433 (+1.49 % vs +0.89 %) |
| BASE-HIT | `super_force_inefficient_decline_below_the_gold_band.pine` | choppy 10-bar path, below the gold band, falling 20-bar slope, negative MACD histogram | +0.28 ATR, 66 % winners, n 1,249 |
| BASE-HIT | `super_energy_capitulation_gap_on_huge_volume.pine` | gap up after a huge-volume wide-range bar far below the SuperTrend line | +0.18 ATR, 65 % winners, n 552 |

Every BASE-HIT script uses the +2/−4 ATR bracket (20 bars). Each has a `strategy_*.pine` twin. All compile clean on
TradingView (Pine v5). Their decision logic is proven in Bend (`lab/bend/LAWS_SUPER.bend`, `PROOF_SUPER.bend`: the
signal implies every block, holds when all hold). Evidence tables, the payoff matrix and Jev's rankings live in
`lab/bend/data/`; the terminal that shows today's names is `lab/bend/evo/tree_of_life.py`.
