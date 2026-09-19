# Diamond strict — calibration sweep (94 tickers, daily bars, next-open entry, flip exit)

Generated 2026-09-19 20:29. Universe: every cached ticker except SPY/QQQ/IWM. Data 2016-09-19 → 2026-09-18. In-sample = trades signalled before 2024-01-01 (380 weeks); out-of-sample = on/after (142 weeks). Signals per week = OOS trades ÷ OOS weeks, across the whole universe.

Knobs: **confirm** = bars allowed for the diamond bar's high/low to be closed through (2/3/5). **min_move** = wave travel required since the last diamond before a new Echo/Tango diamond may fire (5/10/15). **Mountain** — (a) today's count: hype>0, flow>0, close>gold top, band rising (EMA21>EMA55 and EMA21 rising); (b) stricter: hype>10, flow>10, close>gold top, band rising, and the score may only reach 4 when at least 2 of the 4 held on each of the last 3 bars (otherwise capped at 3); (c) as (a) with the band-rising condition replaced by 20-day average volume above its 50-day average. **entry** — strict: 3 confirmed blue & Mountain 4 & above gold & not overheated; 2of3: ≥2 confirmed blue & Mountain ≥3 & above gold. Exit (both): ≥2 confirmed pink, or Mountain ≤2, or close below the gold band.

Rows with fewer than 100 OOS trades are flagged THIN; 'best' below means the top non-thin row. Random-entry control (same flip-style exit): OOS avg 3.01 %, median 0.52 %, win 0.529, PF 1.80, n 6632.

## Ranked by out-of-sample avg % per trade

| rank | confirm | min_move | Mountain | entry | n IS | n OOS | OOS avg % | OOS med % | OOS win | OOS PF | OOS /wk | tickers+ | IS avg % | IS PF | note |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 2 | 10 | c | strict | 286 | 170 | 11.24 | -1.37 | 0.418 | 3.18 | 1.20 | 0.47 | 4.87 | 2.06 | BEST (non-thin) |
| 2 | 2 | 10 | b | strict | 282 | 169 | 10.54 | -1.68 | 0.420 | 3.32 | 1.19 | 0.47 | 5.79 | 2.49 |  |
| 3 | 2 | 15 | b | strict | 225 | 114 | 9.31 | -2.64 | 0.377 | 2.68 | 0.81 | 0.36 | 5.16 | 2.19 |  |
| 4 | 5 | 10 | a | strict | 617 | 402 | 8.90 | -2.70 | 0.386 | 2.70 | 2.84 | 0.49 | 3.11 | 1.77 | MORE-SIGNALS pick |
| 5 | 3 | 10 | c | strict | 333 | 214 | 8.87 | -2.19 | 0.397 | 2.67 | 1.51 | 0.48 | 4.44 | 1.97 |  |
| 6 | 3 | 10 | b | strict | 339 | 217 | 8.11 | -2.33 | 0.406 | 2.68 | 1.53 | 0.43 | 5.12 | 2.30 |  |
| 7 | 5 | 10 | b | strict | 408 | 266 | 7.70 | -2.38 | 0.406 | 2.55 | 1.88 | 0.43 | 5.06 | 2.27 |  |
| 8 | 2 | 10 | a | strict | 427 | 271 | 7.67 | -1.87 | 0.413 | 2.57 | 1.91 | 0.48 | 3.62 | 1.87 |  |
| 9 | 5 | 10 | c | strict | 397 | 245 | 7.36 | -2.75 | 0.388 | 2.31 | 1.73 | 0.48 | 3.88 | 1.87 |  |
| 10 | 2 | 5 | b | strict | 436 | 270 | 6.87 | -2.43 | 0.389 | 2.45 | 1.91 | 0.44 | 4.16 | 2.07 |  |
| 11 | 2 | 5 | c | strict | 409 | 256 | 6.61 | -2.97 | 0.387 | 2.18 | 1.81 | 0.44 | 4.40 | 1.91 |  |
| 12 | 5 | 15 | b | strict | 343 | 204 | 6.42 | -2.84 | 0.382 | 2.17 | 1.44 | 0.43 | 4.33 | 2.04 |  |
| 13 | 3 | 15 | b | strict | 278 | 155 | 6.32 | -2.86 | 0.374 | 2.13 | 1.09 | 0.39 | 4.60 | 2.06 |  |
| 14 | 2 | 15 | c | strict | 220 | 134 | 6.10 | -3.21 | 0.381 | 1.95 | 0.95 | 0.39 | 6.82 | 2.54 |  |
| 15 | 3 | 5 | b | strict | 519 | 332 | 5.95 | -2.84 | 0.389 | 2.22 | 2.35 | 0.43 | 4.08 | 2.05 |  |
| 16 | 3 | 10 | a | strict | 511 | 338 | 5.76 | -2.87 | 0.388 | 2.11 | 2.39 | 0.46 | 3.28 | 1.81 | TODAY |
| 17 | 3 | 5 | c | strict | 468 | 311 | 5.65 | -3.26 | 0.376 | 2.01 | 2.20 | 0.48 | 4.14 | 1.85 |  |
| 18 | 5 | 5 | b | strict | 597 | 389 | 5.45 | -2.73 | 0.396 | 2.11 | 2.75 | 0.48 | 4.00 | 2.03 |  |
| 19 | 2 | 15 | a | strict | 366 | 198 | 5.28 | -3.25 | 0.359 | 1.96 | 1.40 | 0.42 | 3.66 | 1.86 |  |
| 20 | 2 | 5 | a | strict | 621 | 382 | 4.97 | -2.86 | 0.390 | 2.00 | 2.70 | 0.48 | 3.23 | 1.79 |  |
| 21 | 5 | 5 | c | strict | 528 | 355 | 4.87 | -3.55 | 0.375 | 1.84 | 2.51 | 0.48 | 3.85 | 1.80 |  |
| 22 | 3 | 15 | c | strict | 262 | 173 | 4.37 | -3.22 | 0.370 | 1.69 | 1.22 | 0.42 | 5.75 | 2.27 |  |
| 23 | 5 | 15 | b | 2of3 | 2654 | 1504 | 4.33 | -1.84 | 0.364 | 2.10 | 10.62 | 0.68 | 1.95 | 1.66 |  |
| 24 | 3 | 5 | a | strict | 733 | 463 | 4.28 | -3.12 | 0.380 | 1.85 | 3.27 | 0.48 | 3.18 | 1.79 |  |
| 25 | 5 | 15 | a | 2of3 | 2913 | 1700 | 4.15 | -2.52 | 0.340 | 1.94 | 12.01 | 0.67 | 2.39 | 1.73 |  |
| 26 | 5 | 5 | a | strict | 839 | 532 | 4.04 | -2.86 | 0.382 | 1.80 | 3.76 | 0.55 | 3.17 | 1.81 |  |
| 27 | 5 | 10 | b | 2of3 | 2956 | 1712 | 4.00 | -2.09 | 0.348 | 1.95 | 12.09 | 0.63 | 2.22 | 1.74 |  |
| 28 | 5 | 10 | a | 2of3 | 3248 | 1941 | 3.92 | -2.63 | 0.332 | 1.85 | 13.71 | 0.67 | 2.62 | 1.78 |  |
| 29 | 5 | 5 | b | 2of3 | 3444 | 1955 | 3.86 | -2.04 | 0.354 | 1.95 | 13.81 | 0.61 | 2.19 | 1.73 |  |
| 30 | 5 | 15 | a | strict | 545 | 327 | 3.83 | -3.27 | 0.361 | 1.68 | 2.31 | 0.48 | 2.81 | 1.68 |  |
| 31 | 5 | 5 | a | 2of3 | 3825 | 2218 | 3.59 | -2.56 | 0.338 | 1.81 | 15.67 | 0.65 | 2.55 | 1.77 |  |
| 32 | 5 | 15 | c | 2of3 | 2898 | 1711 | 3.52 | -2.11 | 0.351 | 1.80 | 12.09 | 0.62 | 1.94 | 1.61 |  |
| 33 | 3 | 15 | b | 2of3 | 2382 | 1360 | 3.51 | -1.77 | 0.367 | 1.88 | 9.61 | 0.65 | 1.89 | 1.64 |  |
| 34 | 2 | 10 | a | 2of3 | 2670 | 1607 | 3.48 | -2.58 | 0.337 | 1.75 | 11.35 | 0.65 | 2.85 | 1.84 |  |
| 35 | 3 | 10 | a | 2of3 | 2932 | 1768 | 3.48 | -2.62 | 0.336 | 1.76 | 12.49 | 0.66 | 2.75 | 1.81 |  |
| 36 | 2 | 15 | b | 2of3 | 2160 | 1237 | 3.45 | -1.71 | 0.366 | 1.86 | 8.74 | 0.64 | 2.11 | 1.71 |  |
| 37 | 5 | 10 | c | 2of3 | 3205 | 1926 | 3.45 | -2.31 | 0.341 | 1.75 | 13.60 | 0.63 | 2.39 | 1.72 |  |
| 38 | 3 | 10 | b | 2of3 | 2669 | 1563 | 3.45 | -2.04 | 0.355 | 1.82 | 11.04 | 0.65 | 2.30 | 1.76 |  |
| 39 | 3 | 15 | a | 2of3 | 2627 | 1533 | 3.44 | -2.39 | 0.344 | 1.77 | 10.83 | 0.64 | 2.35 | 1.72 |  |
| 40 | 3 | 5 | b | 2of3 | 3201 | 1825 | 3.41 | -2.04 | 0.359 | 1.82 | 12.89 | 0.64 | 2.24 | 1.74 |  |
| 41 | 2 | 15 | a | 2of3 | 2393 | 1390 | 3.39 | -2.39 | 0.345 | 1.75 | 9.82 | 0.64 | 2.57 | 1.79 |  |
| 42 | 2 | 10 | b | 2of3 | 2419 | 1417 | 3.38 | -2.00 | 0.356 | 1.80 | 10.01 | 0.62 | 2.39 | 1.78 |  |
| 43 | 3 | 15 | a | strict | 451 | 261 | 3.26 | -3.75 | 0.349 | 1.56 | 1.84 | 0.40 | 3.13 | 1.74 |  |
| 44 | 2 | 5 | b | 2of3 | 2960 | 1691 | 3.21 | -2.02 | 0.358 | 1.77 | 11.94 | 0.62 | 2.36 | 1.77 |  |
| 45 | 3 | 5 | a | 2of3 | 3551 | 2060 | 3.17 | -2.55 | 0.342 | 1.70 | 14.55 | 0.63 | 2.64 | 1.79 |  |
| 46 | 5 | 5 | c | 2of3 | 3759 | 2207 | 3.17 | -2.36 | 0.345 | 1.70 | 15.59 | 0.63 | 2.27 | 1.69 |  |
| 47 | 2 | 5 | a | 2of3 | 3288 | 1916 | 3.06 | -2.55 | 0.339 | 1.67 | 13.53 | 0.63 | 2.71 | 1.80 |  |
| 48 | 5 | 15 | c | strict | 323 | 207 | 3.05 | -3.68 | 0.357 | 1.47 | 1.46 | 0.43 | 4.89 | 2.11 |  |
| 49 | 3 | 10 | c | 2of3 | 2887 | 1756 | 3.02 | -2.28 | 0.343 | 1.66 | 12.40 | 0.64 | 2.60 | 1.79 |  |
| 50 | 2 | 10 | c | 2of3 | 2631 | 1596 | 3.00 | -2.25 | 0.345 | 1.65 | 11.27 | 0.61 | 2.65 | 1.80 |  |
| 51 | 3 | 15 | c | 2of3 | 2605 | 1549 | 2.92 | -2.05 | 0.351 | 1.67 | 10.94 | 0.61 | 1.90 | 1.60 |  |
| 52 | 2 | 15 | c | 2of3 | 2364 | 1404 | 2.89 | -2.05 | 0.351 | 1.66 | 9.92 | 0.60 | 2.09 | 1.66 |  |
| 53 | 3 | 5 | c | 2of3 | 3485 | 2053 | 2.80 | -2.33 | 0.351 | 1.61 | 14.50 | 0.60 | 2.46 | 1.75 |  |
| 54 | 2 | 5 | c | 2of3 | 3231 | 1906 | 2.59 | -2.36 | 0.348 | 1.56 | 13.46 | 0.64 | 2.50 | 1.75 |  |
| - | - | - | - | RANDOM CONTROL | 11811 | 6632 | 3.01 | 0.52 | 0.529 | 1.80 | 46.85 | 0.83 | 1.58 | 1.41 | random entries, exit ≈ every 10th bar |

## Per-knob marginals (mean over all combos sharing the value, OOS)

| knob | value | mean OOS avg % | mean OOS PF | mean OOS /wk | mean n OOS |
|---|---|---|---|---|---|
| confirm | 2 | 5.39 | 2.10 | 6.33 | 896 |
| confirm | 3 | 4.54 | 1.92 | 7.04 | 996 |
| confirm | 5 | 4.76 | 1.97 | 7.77 | 1100 |
| min_move | 5 | 4.31 | 1.89 | 8.29 | 1173 |
| min_move | 10 | 5.96 | 2.23 | 6.90 | 977 |
| min_move | 15 | 4.42 | 1.87 | 5.95 | 842 |
| mountain | a | 4.43 | 1.90 | 7.58 | 1073 |
| mountain | b | 5.51 | 2.18 | 6.43 | 910 |
| mountain | c | 4.75 | 1.91 | 7.13 | 1010 |
| entry | 2of3 | 3.39 | 1.78 | 12.17 | 1722 |
| entry | strict | 6.40 | 2.22 | 1.92 | 272 |

## Today vs best

| rank | confirm | min_move | Mountain | entry | n IS | n OOS | OOS avg % | OOS med % | OOS win | OOS PF | OOS /wk | tickers+ | IS avg % | IS PF | note |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 16 | 3 | 10 | a | strict | 511 | 338 | 5.76 | -2.87 | 0.388 | 2.11 | 2.39 | 0.46 | 3.28 | 1.81 | TODAY |
| 1 | 2 | 10 | c | strict | 286 | 170 | 11.24 | -1.37 | 0.418 | 3.18 | 1.20 | 0.47 | 4.87 | 2.06 | BEST (non-thin) |
| 4 | 5 | 10 | a | strict | 617 | 402 | 8.90 | -2.70 | 0.386 | 2.70 | 2.84 | 0.49 | 3.11 | 1.77 | MORE-SIGNALS pick |

## Calibration anchors (founders' screenshots)

Diamond placement depends only on `min_move` (the confirmation window and Mountain never move a diamond; Bravo diamonds depend on none of the knobs, so SLNH is a fixed reference). Weekly bars are pandas W-FRI resamples of the daily cache; they are listed by the Monday that starts the week, which is how TradingView labels weekly bars. Offset = our nearest same-colour diamond vs the founders' bar, in calendar days (0 = same bar).

### IREN weekly Echo — founders: blue 2025-04-21, pink 2025-07-21

| min_move | our diamonds in window | offset to blue 2025-04-21 | offset to pink 2025-07-21 |
|---|---|---|---|
| 5 | pink 2025-02-24, blue 2025-04-21, pink 2025-07-21, blue 2025-08-18 | 0 (same bar, 2025-04-21) | 0 (same bar, 2025-07-21) |
| 10 (default) (best) | pink 2025-02-24, blue 2025-04-21, pink 2025-07-21, blue 2025-08-18 | 0 (same bar, 2025-04-21) | 0 (same bar, 2025-07-21) |
| 15 | blue 2025-04-21, pink 2025-07-21 | 0 (same bar, 2025-04-21) | 0 (same bar, 2025-07-21) |

### ONDS daily Echo — founders: pink 2025-10-08..2025-10-10, blue 2025-11-07

| min_move | our diamonds in window | offset to pink 2025-10-08 | offset to blue 2025-11-07 |
|---|---|---|---|
| 5 | pink 2025-09-03, pink 2025-09-09, blue 2025-09-12, blue 2025-09-18, pink 2025-09-26, blue 2025-10-03, pink 2025-10-10, blue 2025-10-24, pink 2025-10-30, blue 2025-11-07, pink 2025-11-20, blue 2025-11-24, pink 2025-11-26, blue 2025-12-03, pink 2025-12-08, blue 2025-12-19, pink 2025-12-26, blue 2025-12-31 | 0 (same bar, 2025-10-10) | 0 (same bar, 2025-11-07) |
| 10 (default) (best) | pink 2025-09-03, pink 2025-09-09, pink 2025-09-26, blue 2025-10-03, pink 2025-10-10, blue 2025-10-24, pink 2025-10-30, pink 2025-11-20, blue 2025-12-03, blue 2025-12-19, blue 2025-12-31 | 0 (same bar, 2025-10-10) | -14d (2025-10-24) |
| 15 | pink 2025-09-03, blue 2025-09-18, blue 2025-10-03, blue 2025-10-24, pink 2025-10-30, pink 2025-11-20, blue 2025-12-03, blue 2025-12-19 | +20d (2025-10-30) | -14d (2025-10-24) |

### SLNH weekly Bravo — founders: blue 2025-09-15..2025-09-22, pink 2025-11-10..2025-11-17

| min_move | our diamonds in window | offset to blue 2025-09-15 | offset to pink 2025-11-10 |
|---|---|---|---|
| 5 | blue 2025-09-22, pink 2025-11-10, pink 2025-12-15 | 0 (same bar, 2025-09-22) | 0 (same bar, 2025-11-10) |
| 10 (default) (best) | blue 2025-09-22, pink 2025-11-10, pink 2025-12-15 | 0 (same bar, 2025-09-22) | 0 (same bar, 2025-11-10) |
| 15 | blue 2025-09-22, pink 2025-11-10, pink 2025-12-15 | 0 (same bar, 2025-09-22) | 0 (same bar, 2025-11-10) |

## What to change (plain English)

Today's parameters (confirm 3, min_move 10, Mountain a, strict) rank 16 of 54 out-of-sample: 5.76 % per trade, PF 2.11, 2.39 signals/week (338 OOS trades). The best non-thin row (confirm 2, min_move 10, Mountain c, strict) makes 11.24 % per trade, PF 3.18, 1.20 signals/week (170 OOS trades): +5.48 points per trade and -1.19 signals/week versus today. In-sample the same row makes 4.87 % (PF 2.06) against today's 3.28 % (PF 1.81).

If the aim is more signals rather than bigger ones: the best row that keeps at least today's signal rate is confirm 5, min_move 10, Mountain a, strict — 8.90 % per trade, PF 2.70, 2.84 signals/week (402 OOS trades); in-sample 3.11 % (PF 1.77).

Knob by knob, averaged over everything else, the OOS-best values are: confirm 2, min_move 10, Mountain b, entry strict. The 2-of-3 entry averages 3.39 % across its 27 rows against 6.40 % for strict. The random control makes 3.01 % per trade with the same exit style, so only rows well above that carry information; a row that beats it by a point or two on a few hundred trades is inside the noise. 14 of the 54 rows beat today on avg % in BOTH samples with ≥100 OOS trades: (2, 10, c, strict), (2, 10, b, strict), (2, 15, b, strict), (3, 10, c, strict), (3, 10, b, strict), (5, 10, b, strict), (2, 10, a, strict), (5, 10, c, strict), (2, 5, b, strict), (2, 5, c, strict), (5, 15, b, strict), (3, 15, b, strict), (2, 15, c, strict), (3, 5, b, strict).

Recommended change: keep min_move 10 and the strict entry (both are clear winners on the marginals), adopt Mountain b, and pick the confirmation window by what the rule is for — 2 bars for fewer, larger trades (the top row) or 5 bars for more of them (the more-signals pick). The anchors below show whether the chosen min_move keeps the diamonds on the founders' bars; the other three knobs never move a diamond, only whether a trade is taken.

Caveat: the 2024 split limits selection bias but does not remove it. 54 rows were scored on the same out-of-sample window and the best one was picked by that score, so its OOS number is optimistic; the in-sample column and the per-knob marginals (which average away single lucky cells) are the honest guides. Treat a change as real only if it wins in both samples and on the marginals, and re-check it on the next quarter's bars before trusting it. The anchors section shows separately whether a change moves the diamonds toward the founders' bars — that is a different question from return, and both matter.
