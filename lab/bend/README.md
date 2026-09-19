# The Bend 2 engine — every Dragon + Diamond panel, every ticker, every bar, proven where it can be

`dragon.bend` is the whole reading in one Bend 2 program: per ticker a fold over the packed bars computes the KDJ candle,
ribbon, ATR, the volatility hole (pivot levels, two closes to break), the 120-bar chip distribution (whales, retail,
POC), the TE line (triple-EMA regression), MACD and the three RSIs, and the Diamond vetoes (hype, money flow, gold
band, Mountain, bubble). It then scores every rising edge of the C reading — and a hashed random entry on ~12 % of
bars as the control — under the same exits: hole broke down, +2/−4 and +3/−4 ATR brackets (20 bars), +0.5/−3 scalp
(10 bars), +0.5/−4 (20 bars), the 10-bar close, and the 20-bar best and worst excursions. Tickers run in parallel
(fork-join over the list); 2,174 tickers × 10 years take about a minute on this Mac.

| file | what |
|---|---|
| `pack.py` | cached bars → `data/<D|W>/<T>.bin` (6 U32 words per bar: yyyymmdd, o/h/l/c × 10000, volume / 100) + `list.txt` |
| `dragon.bend` | the engine; `bend dragon.bend -o dragon`, run `./dragon data/D --threads 8` → `data/D/states.bin` |
| `order.py` | sorts a Bend file callee-first (the compiler needs it) |
| `validate.py` | every flag bit and ATR % of the engine against the Python ports (`mine/dragon.py`, `mine/diamond.py`) |
| `trades.py` | decodes the output, validates every trade against a Python replica, aggregates by tier / year / band |
| `LAWS.bend`, `PROOF.bend`, `gen_proof.py` | the claims about the engine's decisions and their proofs; `bend PROOF.bend` is the gate |
| `live.py` | the terminal view: names whose last daily or weekly bar has the reading ON, ranked, with the honest expectancies beside them |

## Output format

Per ticker: `n`, then `2n` state words (word 0 = flags: bit 0 hole broke up, 1 hole broke down, 2 whales ≥ 50 %, 3 TE
rising, 4 Mountain ≥ 3, 5 above gold, 6 bubble hot, 7 SS candle confirmed, 8 ribbon, 9 closes above POC, 10 MACD & RSI
up, 11 fresh break ≤ 3 bars, 12/13 pivot high/low confirmed, bits 16–23 whales %, bits 24–27 bullish panels 0–7;
word 1 = ATR % × 1000), then `m`, then `16m` trade words (signal bar, date, flags with bit 30 = random control, ATR %
× 1000, entry = next open × 10000, bars to the hole exit, hole exit %, the four bracket legs in ATR, MFE, MAE, 10-bar
close %, exit bar, reserved; returns encoded (x + 10000) × 100).

## Validation

`python3.12 bend/validate.py --run` on eight tickers: every flag agrees with the Python port on 99.5–100 % of bars
(the only differences are exact ties between chip bins, which the Python dict resolves arbitrarily), ATR % within
0.001. `python3.12 bend/trades.py --validate`: every signal matched, exit bars identical, returns within the 0.01
encoding step.

## Laws (`bend PROOF.bend` → "All terms check.")

The hole machine: a pivot re-arms; an armed hole breaks up on two closes above, down on two below, else holds; a
broken hole stays broken; never both up and down. The C reading implies each of its six conditions and holds when
they all hold. The panel count is at most seven, seven when all hold, zero when none, and survives the 4-bit packing.
A bracket leg once decided stays decided, checks the stop before the target, and otherwise keeps its running value.
Floating-point arithmetic is opaque to the checker, so the laws cover the discrete decisions, which is where packing
and state-machine mistakes hide; the float paths are covered by the bar-for-bar validation above.

## What the full-universe run says (2,174 US common stocks ≥ $100M, 2016–2026)

Matched within year × ATR-band cells, C signals minus random entries under the same exits:

| timeframe | signals | Δ hole exit | Δ +2/−4 bracket | Δ 10-bar close | Δ MFE |
|---|---|---|---|---|---|
| daily | 101,261 | +0.27 % ± 0.21 | **−0.051 ± 0.016 ATR** | −0.11 % ± 0.09 | −0.04 ATR |
| weekly | 32,722 | +0.97 % ± 1.83 | **+0.049 ± 0.028 ATR** | −0.05 % ± 0.30 | +0.21 ATR |

Tiers (fresh, ≥ 6 panels, ATR ≥ 5 %, whales ≥ 85) do not change the picture. At random entries every bullish panel
state is followed by a slightly negative 10-day excess. The random control with the hole exit makes +2.1 % per trade
on daily bars by itself — that is market drift plus a trend-following exit. Full tables: `data/trades_D.md`,
`data/trades_W.md`; per-signal records: `data/trades_D.csv`, `data/trades_W.csv`.

## Daily use

```bash
python3.12 -m mine.data --universe broad          # refresh prices (Nasdaq history API; ~40 min for the universe)
python3.12 bend/live.py --pack --run --quotes      # pack, run both timeframes, show the live list with expectancies
```
