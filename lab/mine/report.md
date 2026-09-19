# Mining report — 94 tickers, 174,363 ticker-days (2016-09-19 → 2026-09-03)

Train < 2024-01-01: 111,960 rows · Test ≥ 2024-01-01: 62,403 rows · bear-regime share 23%

## LONG spike: +3 ATR reached within 10 bars without a −1.5 ATR drawdown (entry next open)

Base rate: 18.6% of all ticker-days · mean fwd10 0.32 ATR (bull days 0.31, bear days 0.35) · fwd columns below are in ATR units, lift is within-ticker

### What the panels showed (lift = P(target | state) / base; top 25 by lift, support ≥ 300)

| feature | value | n | P(target) | lift | fwd10 % | fwd5 % | bear fwd10 % |
|---|---|---|---|---|---|---|---|
| di_flow | (17.401, 48.651] | 34622 | 22.7% | 1.21 | 0.55 | 0.23 | 0.64 |
| dr_rsi14 | (62.624, 99.597] | 34822 | 22.3% | 1.19 | 0.51 | 0.22 | 0.51 |
| di_overheated | True | 26780 | 22.4% | 1.19 | 0.50 | 0.23 | 0.87 |
| di_green_dist | (4.688, 30.748] | 28569 | 22.1% | 1.18 | 0.49 | 0.21 | 0.59 |
| di_bubble | (5.158, 84.553] | 34871 | 22.2% | 1.18 | 0.51 | 0.22 | 0.86 |
| di_gold_dist | (1.351, 12.727] | 34871 | 22.1% | 1.18 | 0.50 | 0.22 | 0.56 |
| dr_te_slope | (0.114, 1.106] | 34760 | 22.1% | 1.17 | 0.48 | 0.22 | 0.60 |
| dr_n_bull | 5 | 19031 | 21.8% | 1.17 | 0.43 | 0.20 | 0.56 |
| dr_macd_dif | (0.779, 10.962] | 34871 | 21.9% | 1.16 | 0.50 | 0.23 | 0.51 |
| di_mountain | 4 | 48261 | 21.7% | 1.16 | 0.48 | 0.21 | 0.38 |
| di_silver_pos | (0.929, 51.235] | 33012 | 21.7% | 1.16 | 0.48 | 0.21 | 0.69 |
| dr_n_bear | 0 | 54901 | 21.5% | 1.15 | 0.41 | 0.19 | 0.41 |
| dr_n_bear | 1 | 20520 | 21.4% | 1.15 | 0.53 | 0.24 | 0.53 |
| di_mountain | 3 | 23661 | 21.4% | 1.15 | 0.42 | 0.21 | 0.64 |
| dr_whales | (81.477, 99.756] | 34835 | 21.6% | 1.15 | 0.45 | 0.21 | 0.64 |
| di_silver_pos | (0.674, 0.929] | 33011 | 21.4% | 1.15 | 0.41 | 0.19 | 0.27 |
| di_above_gold | True | 78703 | 21.4% | 1.15 | 0.45 | 0.20 | 0.47 |
| dr_n_bull | 6 | 18528 | 21.4% | 1.15 | 0.42 | 0.19 | 0.45 |
| dr_n_bull | 7 | 9930 | 21.5% | 1.14 | 0.47 | 0.19 | 0.35 |
| dr_n_bull | 4 | 20343 | 21.3% | 1.14 | 0.44 | 0.18 | 0.52 |
| dr_ribbon_bull | True | 92347 | 21.2% | 1.14 | 0.42 | 0.19 | 0.51 |
| dr_hole_state | 1 | 46492 | 21.2% | 1.14 | 0.43 | 0.19 | 0.53 |
| dr_retail | (1.75, 18.004] | 34835 | 21.3% | 1.14 | 0.40 | 0.18 | 0.25 |
| dr_retail | (-0.001, 1.75] | 34835 | 21.2% | 1.14 | 0.51 | 0.23 | 0.69 |
| dr_macd_dif | (0.278, 0.779] | 34871 | 21.1% | 1.13 | 0.40 | 0.19 | 0.36 |

### Lowest-lift states (what NOT to see)

| feature | value | n | P(target) | lift | fwd10 % |
|---|---|---|---|---|---|
| dr_retail | (75.648, 99.894] | 34835 | 14.9% | 0.81 | 0.17 |
| di_gold_dist | (-13.501, -1.975] | 34871 | 14.8% | 0.81 | 0.20 |
| dr_hole_state | -1 | 35770 | 14.6% | 0.79 | 0.20 |
| dr_n_bear | 6 | 19462 | 14.5% | 0.79 | 0.18 |
| dr_macd_dif | (-3.102, -0.603] | 34871 | 14.4% | 0.78 | 0.17 |
| dr_te_slope | (-0.522, -0.0877] | 34760 | 14.3% | 0.78 | 0.21 |
| dr_rsi14 | (-0.001, 40.26] | 34822 | 14.1% | 0.77 | 0.21 |
| di_st_echo | 0 | 1037 | 14.2% | 0.75 | 0.80 |
| dr_n_bear | 7 | 11443 | 13.8% | 0.75 | 0.18 |
| di_purple_dist | (4.549, 25.496] | 26686 | 12.8% | 0.70 | 0.15 |

### Learned rules — Dragon only (tree depth ≤ 4; ranked by out-of-sample lift)

| rule | train n | train lift | train fwd10 | TEST n | TEST lift | TEST fwd10 | TEST win10 | TEST bear fwd10 | signals/wk (test) | tickers |
|---|---|---|---|---|---|---|---|---|---|---|
| dr_n_bear <= 3.5 AND dr_rsi14 > 60.36 AND dr_poc_ratio > 0.786 AND dr_poc_ratio <= 1.73 | 1461 | 1.61 | 0.76 | 1652 | 1.4 | 0.69 | 0.542 | -2.6 | 11.86 | 67 |
| dr_n_bear <= 3.5 AND dr_rsi14 > 60.36 AND dr_poc_ratio <= 0.786 AND dr_whales > 94.121 | 4692 | 1.03 | 0.23 | 3035 | 1.19 | 0.56 | 0.548 | 1.37 | 21.79 | 93 |
| dr_n_bear <= 3.5 AND dr_rsi14 > 60.36 AND dr_poc_ratio <= 0.786 AND dr_whales <= 94.121 | 21514 | 1.21 | 0.5 | 9937 | 1.18 | 0.59 | 0.548 | 1.37 | 71.34 | 94 |
| dr_n_bear <= 3.5 AND dr_rsi14 <= 60.36 AND dr_retail <= 7.067 AND dr_ss_confirm <= 0.5 | 10547 | 1.23 | 0.62 | 4532 | 1.17 | 0.38 | 0.513 | 0.69 | 32.54 | 94 |
| dr_n_bear > 3.5 AND dr_rsi14 <= 36.276 AND dr_whales <= 0.788 AND dr_retail > 98.176 | 1293 | 0.98 | 0.35 | 390 | 1.08 | 0.52 | 0.585 | 0.89 | 2.8 | 59 |
| dr_n_bear <= 3.5 AND dr_rsi14 <= 60.36 AND dr_retail <= 7.067 AND dr_ss_confirm > 0.5 | 3036 | 0.99 | 0.35 | 1120 | 1.05 | 0.32 | 0.506 | -0.04 | 8.04 | 90 |
| dr_n_bear <= 3.5 AND dr_rsi14 <= 60.36 AND dr_retail > 7.067 AND dr_rsi14 > 38.399 | 24710 | 1.04 | 0.23 | 16282 | 1.05 | 0.31 | 0.491 | 1.19 | 116.9 | 94 |
| dr_n_bear <= 3.5 AND dr_rsi14 > 60.36 AND dr_poc_ratio > 0.786 AND dr_poc_ratio > 1.73 | 359 | 0.79 | -0.49 | 376 | 0.98 | -0.06 | 0.431 | None | 2.7 | 37 |

### Learned rules — Diamond only (tree depth ≤ 4; ranked by out-of-sample lift)

| rule | train n | train lift | train fwd10 | TEST n | TEST lift | TEST fwd10 | TEST win10 | TEST bear fwd10 | signals/wk (test) | tickers |
|---|---|---|---|---|---|---|---|---|---|---|
| di_below_gold <= 0.5 AND di_flow > 26.366 AND di_c_bravo > -0.5 AND di_bubble > 10.503 | 1139 | 1.06 | -0.22 | 531 | 1.48 | 1.02 | 0.606 | None | 3.81 | 57 |
| di_below_gold <= 0.5 AND di_flow > 26.366 AND di_c_bravo > -0.5 AND di_bubble <= 10.503 | 7140 | 1.42 | 0.63 | 3492 | 1.35 | 0.76 | 0.553 | 1.27 | 25.07 | 94 |
| di_below_gold <= 0.5 AND di_flow <= 26.366 AND di_bubble <= 3.983 AND di_purple_dist <= 3.482 | 31248 | 1.06 | 0.3 | 17225 | 1.15 | 0.46 | 0.515 | 1.34 | 123.67 | 94 |
| di_below_gold <= 0.5 AND di_flow <= 26.366 AND di_bubble > 3.983 AND di_hype > 15.065 | 7439 | 1.03 | 0.44 | 4010 | 1.11 | 0.48 | 0.54 | 1.18 | 28.79 | 92 |
| di_below_gold <= 0.5 AND di_flow <= 26.366 AND di_bubble > 3.983 AND di_hype <= 15.065 | 16753 | 1.24 | 0.55 | 9624 | 1.1 | 0.3 | 0.507 | 0.02 | 69.1 | 92 |
| di_below_gold > 0.5 AND di_purple_dist <= 5.574 AND di_hype > -22.713 AND di_bubble <= 2.038 | 28297 | 0.86 | 0.14 | 16049 | 0.89 | 0.31 | 0.53 | 0.89 | 115.22 | 94 |
| di_below_gold > 0.5 AND di_purple_dist <= 5.574 AND di_hype <= -22.713 AND di_purple_dist <= 5.355 | 3763 | 0.63 | 0.05 | 1869 | 0.83 | 0.23 | 0.539 | 0.91 | 13.42 | 94 |
| di_below_gold > 0.5 AND di_purple_dist > 5.574 AND di_gold_dist <= -2.295 AND di_purple_dist > 10.726 | 839 | 0.5 | 0.0 | 298 | 0.83 | 0.33 | 0.53 | -0.39 | 2.14 | 42 |

### Learned rules — Dragon + Diamond (tree depth ≤ 4; ranked by out-of-sample lift)

| rule | train n | train lift | train fwd10 | TEST n | TEST lift | TEST fwd10 | TEST win10 | TEST bear fwd10 | signals/wk (test) | tickers |
|---|---|---|---|---|---|---|---|---|---|---|
| dr_n_bear <= 3.5 AND di_flow > 26.366 AND di_c_bravo > -0.5 AND dr_rsi14 > 77.327 | 1611 | 1.08 | 0.52 | 988 | 1.41 | 0.74 | 0.558 | 3.02 | 7.09 | 87 |
| dr_n_bear <= 3.5 AND di_flow > 26.366 AND di_c_bravo > -0.5 AND dr_rsi14 <= 77.327 | 6706 | 1.44 | 0.5 | 3048 | 1.35 | 0.81 | 0.56 | 1.27 | 21.88 | 94 |
| dr_n_bear <= 3.5 AND di_flow <= 26.366 AND di_bubble <= 3.983 AND di_purple_dist <= 3.482 | 30853 | 1.07 | 0.3 | 17315 | 1.14 | 0.46 | 0.514 | 1.32 | 124.31 | 94 |
| dr_n_bear <= 3.5 AND di_flow <= 26.366 AND di_bubble > 3.983 AND dr_whales > 13.324 | 23424 | 1.16 | 0.47 | 13342 | 1.1 | 0.36 | 0.519 | 0.44 | 95.79 | 93 |
| dr_n_bear > 3.5 AND di_purple_dist <= 5.574 AND di_bubble <= 2.038 AND di_hype > -22.713 | 28767 | 0.85 | 0.13 | 16010 | 0.9 | 0.31 | 0.53 | 0.9 | 114.94 | 94 |
| dr_n_bear > 3.5 AND di_purple_dist <= 5.574 AND di_bubble <= 2.038 AND di_hype <= -22.713 | 3629 | 0.67 | 0.08 | 1759 | 0.82 | 0.22 | 0.538 | 0.95 | 12.63 | 94 |
| dr_n_bear > 3.5 AND di_purple_dist <= 5.574 AND di_bubble > 2.038 AND dr_poc_ratio > -0.121 | 3351 | 1.11 | 0.58 | 2140 | 0.78 | 0.17 | 0.501 | 1.38 | 15.36 | 86 |
| dr_n_bear <= 3.5 AND di_flow <= 26.366 AND di_bubble <= 3.983 AND di_purple_dist > 3.482 | 3382 | 0.81 | 0.1 | 2254 | 0.72 | -0.02 | 0.43 | 0.32 | 16.18 | 94 |

## SHORT spike: −3 ATR within 10 bars without a +1.5 ATR rally

Base rate: 12.1% of all ticker-days · mean fwd10 0.32 ATR (bull days 0.31, bear days 0.35) · fwd columns below are in ATR units, lift is within-ticker

### What the panels showed (lift = P(target | state) / base; top 25 by lift, support ≥ 300)

| feature | value | n | P(target) | lift | fwd10 % | fwd5 % | bear fwd10 % |
|---|---|---|---|---|---|---|---|
| dr_n_bull | 7 | 9930 | 15.6% | 1.27 | 0.47 | 0.19 | 0.35 |
| di_overheated | True | 26780 | 15.8% | 1.24 | 0.50 | 0.23 | 0.87 |
| di_bubble | (5.158, 84.553] | 34871 | 15.4% | 1.21 | 0.51 | 0.22 | 0.86 |
| di_n_blue_c | 3 | 7339 | 14.9% | 1.19 | 0.34 | 0.15 | 0.37 |
| di_gold_dist | (1.351, 12.727] | 34871 | 14.7% | 1.18 | 0.50 | 0.22 | 0.56 |
| di_silver_pos | (0.929, 51.235] | 33012 | 14.7% | 1.18 | 0.48 | 0.21 | 0.69 |
| dr_n_bull | 6 | 18528 | 14.7% | 1.18 | 0.42 | 0.19 | 0.45 |
| dr_rsi14 | (62.624, 99.597] | 34822 | 14.4% | 1.17 | 0.51 | 0.22 | 0.51 |
| dr_n_bear | 0 | 54901 | 14.5% | 1.17 | 0.41 | 0.19 | 0.41 |
| dr_te_slope | (0.114, 1.106] | 34760 | 14.5% | 1.17 | 0.48 | 0.22 | 0.60 |
| di_mountain | 4 | 48261 | 14.3% | 1.17 | 0.48 | 0.21 | 0.38 |
| dr_poc_ratio | (0.199, 24.92] | 34873 | 13.9% | 1.16 | 0.40 | 0.18 | 0.27 |
| dr_whales | (81.477, 99.756] | 34835 | 14.3% | 1.15 | 0.45 | 0.21 | 0.64 |
| di_n_blue | 3 | 24921 | 14.0% | 1.15 | 0.32 | 0.12 | 0.29 |
| dr_rsi_up | True | 54786 | 14.1% | 1.15 | 0.42 | 0.17 | 0.48 |
| dr_retail | (-0.001, 1.75] | 34835 | 14.5% | 1.15 | 0.51 | 0.23 | 0.69 |
| dr_ss_confirm | True | 55621 | 14.1% | 1.14 | 0.38 | 0.16 | 0.50 |
| dr_j | (87.984, 126.866] | 34873 | 14.2% | 1.14 | 0.33 | 0.15 | 0.47 |
| di_n_pink | 0 | 25852 | 13.8% | 1.14 | 0.34 | 0.13 | 0.33 |
| dr_macd_dif | (0.779, 10.962] | 34871 | 14.2% | 1.14 | 0.50 | 0.23 | 0.51 |
| di_hype | (14.789, 49.121] | 34873 | 13.7% | 1.14 | 0.51 | 0.20 | 0.54 |
| dr_retail | (1.75, 18.004] | 34835 | 13.9% | 1.13 | 0.40 | 0.18 | 0.25 |
| di_bravo | 1 | 6260 | 13.5% | 1.12 | 0.30 | 0.09 | 0.38 |
| di_green_dist | (4.688, 30.748] | 28569 | 14.1% | 1.12 | 0.49 | 0.21 | 0.59 |
| di_above_gold | True | 78703 | 13.7% | 1.12 | 0.45 | 0.20 | 0.47 |

### Lowest-lift states (what NOT to see)

| feature | value | n | P(target) | lift | fwd10 % |
|---|---|---|---|---|---|
| dr_n_bear | 5 | 19874 | 9.9% | 0.84 | 0.18 |
| di_gold_dist | (-13.501, -1.975] | 34871 | 9.7% | 0.82 | 0.20 |
| dr_macd_dif | (-3.102, -0.603] | 34871 | 9.7% | 0.81 | 0.17 |
| di_purple_dist | (4.549, 25.496] | 26686 | 9.6% | 0.81 | 0.15 |
| dr_te_slope | (-0.522, -0.0877] | 34760 | 9.6% | 0.81 | 0.21 |
| dr_rsi14 | (-0.001, 40.26] | 34822 | 9.5% | 0.80 | 0.21 |
| di_n_pink_c | 3 | 5943 | 9.3% | 0.79 | 0.18 |
| dr_poc_ratio | (-0.9, -0.155] | 34873 | 8.6% | 0.77 | 0.22 |
| di_st_tango | 0 | 2375 | 8.8% | 0.76 | 0.60 |
| di_st_echo | 0 | 1037 | 5.0% | 0.43 | 0.80 |

### Learned rules — Dragon only (tree depth ≤ 4; ranked by out-of-sample lift)

| rule | train n | train lift | train fwd10 | TEST n | TEST lift | TEST fwd10 | TEST win10 | TEST bear fwd10 | signals/wk (test) | tickers |
|---|---|---|---|---|---|---|---|---|---|---|
| dr_poc_ratio > -0.185 AND dr_ss_confirm > 0.5 AND dr_whales > 0.651 AND dr_macd_dif <= 1.396 | 28462 | 1.16 | 0.35 | 15541 | 1.22 | 0.43 | 0.518 | 1.21 | 111.58 | 94 |
| dr_poc_ratio > -0.185 AND dr_ss_confirm > 0.5 AND dr_whales > 0.651 AND dr_macd_dif > 1.396 | 4475 | 1.43 | 0.16 | 2491 | 1.17 | 0.77 | 0.591 | -1.05 | 17.88 | 88 |
| dr_poc_ratio > -0.185 AND dr_ss_confirm <= 0.5 AND dr_rsi14 > 13.832 AND dr_j > 4.74 | 48381 | 1.02 | 0.31 | 27753 | 1.02 | 0.32 | 0.509 | 0.75 | 199.25 | 94 |
| dr_poc_ratio > -0.185 AND dr_ss_confirm <= 0.5 AND dr_rsi14 > 13.832 AND dr_j <= 4.74 | 10096 | 0.85 | 0.27 | 6345 | 0.96 | 0.15 | 0.492 | 0.62 | 45.55 | 94 |
| dr_poc_ratio <= -0.185 AND dr_poc_ratio > -0.632 AND dr_whales <= 12.49 AND dr_poc_ratio > -0.373 | 8454 | 0.95 | 0.04 | 5234 | 0.71 | 0.27 | 0.531 | 0.73 | 37.58 | 93 |
| dr_poc_ratio <= -0.185 AND dr_poc_ratio > -0.632 AND dr_whales > 12.49 AND dr_n_bull > 0.5 | 2680 | 0.76 | 0.23 | 1405 | 0.61 | 0.45 | 0.517 | 1.67 | 10.09 | 77 |
| dr_poc_ratio <= -0.185 AND dr_poc_ratio > -0.632 AND dr_whales > 12.49 AND dr_n_bull <= 0.5 | 2244 | 0.39 | 0.17 | 1488 | 0.42 | 0.26 | 0.54 | 0.67 | 10.68 | 78 |
| dr_poc_ratio <= -0.185 AND dr_poc_ratio > -0.632 AND dr_whales <= 12.49 AND dr_poc_ratio <= -0.373 | 4895 | 0.61 | 0.31 | 1960 | 0.4 | 0.5 | 0.578 | 0.89 | 14.07 | 62 |

### Learned rules — Diamond only (tree depth ≤ 4; ranked by out-of-sample lift)

| rule | train n | train lift | train fwd10 | TEST n | TEST lift | TEST fwd10 | TEST win10 | TEST bear fwd10 | signals/wk (test) | tickers |
|---|---|---|---|---|---|---|---|---|---|---|
| di_bubble > 6.402 AND di_bubble <= 8.734 AND di_st_bravo <= 0.0 AND di_green_dist <= 5.499 | 506 | 0.53 | 1.11 | 255 | 1.73 | 0.2 | 0.541 | -1.16 | 1.83 | 18 |
| di_bubble > 6.402 AND di_bubble <= 8.734 AND di_st_bravo > 0.0 AND di_bubble > 6.687 | 7216 | 1.19 | 0.52 | 3803 | 1.39 | 0.46 | 0.525 | 0.37 | 27.3 | 90 |
| di_bubble > 6.402 AND di_bubble > 8.734 AND di_gold_dist > 2.041 AND di_bubble <= 12.595 | 3326 | 1.3 | 0.58 | 1828 | 1.3 | 0.8 | 0.601 | 0.92 | 13.12 | 78 |
| di_bubble > 6.402 AND di_bubble > 8.734 AND di_gold_dist <= 2.041 AND di_n_pink_c <= 0.5 | 1536 | 2.1 | -0.14 | 682 | 1.26 | 0.53 | 0.575 | None | 4.9 | 40 |
| di_bubble > 6.402 AND di_bubble <= 8.734 AND di_st_bravo <= 0.0 AND di_green_dist > 5.499 | 305 | 1.03 | 1.09 | 121 | 1.22 | 0.76 | 0.603 | -0.38 | 0.87 | 19 |
| di_bubble > 6.402 AND di_bubble > 8.734 AND di_gold_dist <= 2.041 AND di_n_pink_c > 0.5 | 586 | 1.46 | 0.76 | 244 | 1.14 | 1.02 | 0.684 | None | 1.75 | 28 |
| di_bubble > 6.402 AND di_bubble > 8.734 AND di_gold_dist > 2.041 AND di_bubble > 12.595 | 657 | 1.83 | -0.12 | 211 | 1.11 | 1.11 | 0.621 | None | 1.51 | 28 |
| di_bubble <= 6.402 AND di_silver_pos > 0.086 AND di_purple_dist <= 3.562 AND di_flow <= 19.712 | 45397 | 1.09 | 0.25 | 28959 | 1.04 | 0.35 | 0.514 | 1.06 | 207.91 | 94 |

### Learned rules — Dragon + Diamond (tree depth ≤ 4; ranked by out-of-sample lift)

| rule | train n | train lift | train fwd10 | TEST n | TEST lift | TEST fwd10 | TEST win10 | TEST bear fwd10 | signals/wk (test) | tickers |
|---|---|---|---|---|---|---|---|---|---|---|
| dr_poc_ratio > -0.185 AND di_bubble > 8.646 AND di_gold_dist <= 2.041 AND dr_macd_dif <= 1.071 | 1012 | 2.24 | -0.14 | 399 | 1.52 | 0.62 | 0.586 | None | 2.86 | 28 |
| dr_poc_ratio > -0.185 AND di_bubble > 8.646 AND di_gold_dist > 2.041 AND dr_rsi14 <= 68.589 | 310 | 0.73 | 2.58 | 92 | 1.41 | 0.92 | 0.62 | 1.34 | 0.66 | 31 |
| dr_poc_ratio > -0.185 AND di_bubble > 8.646 AND di_gold_dist > 2.041 AND dr_rsi14 > 68.589 | 3791 | 1.43 | 0.29 | 2017 | 1.28 | 0.82 | 0.602 | 1.05 | 14.48 | 79 |
| dr_poc_ratio > -0.185 AND di_bubble <= 8.646 AND dr_whales <= 0.149 AND dr_retail > 85.615 | 1230 | 1.29 | 0.04 | 619 | 1.26 | 0.09 | 0.53 | 0.75 | 4.44 | 66 |
| dr_poc_ratio > -0.185 AND di_bubble <= 8.646 AND dr_whales <= 0.149 AND dr_retail <= 85.615 | 3680 | 0.54 | 0.57 | 342 | 1.19 | -0.13 | 0.462 | 0.0 | 2.46 | 39 |
| dr_poc_ratio > -0.185 AND di_bubble <= 8.646 AND dr_whales > 0.149 AND di_purple_dist <= 3.562 | 65966 | 1.07 | 0.34 | 38281 | 1.1 | 0.4 | 0.514 | 1.1 | 274.84 | 94 |
| dr_poc_ratio > -0.185 AND di_bubble > 8.646 AND di_gold_dist <= 2.041 AND dr_macd_dif > 1.071 | 1236 | 1.65 | 0.3 | 576 | 0.99 | 0.74 | 0.616 | None | 4.14 | 43 |
| dr_poc_ratio > -0.185 AND di_bubble <= 8.646 AND dr_whales > 0.149 AND di_purple_dist > 3.562 | 15292 | 0.88 | 0.2 | 9840 | 0.93 | 0.09 | 0.481 | 0.17 | 70.65 | 94 |

## EXIT / top: while a Dragon (≥4/7) or Diamond (≥2 confirmed) long is open, a −3 ATR drop starts here

Base rate: 5.2% of all ticker-days · mean fwd10 0.32 ATR (bull days 0.31, bear days 0.35) · fwd columns below are in ATR units, lift is within-ticker

### What the panels showed (lift = P(target | state) / base; top 25 by lift, support ≥ 300)

| feature | value | n | P(target) | lift | fwd10 % | fwd5 % | bear fwd10 % |
|---|---|---|---|---|---|---|---|
| dr_n_bull | 7 | 9930 | 13.3% | 2.44 | 0.47 | 0.19 | 0.35 |
| di_n_blue_c | 3 | 7339 | 12.8% | 2.29 | 0.34 | 0.15 | 0.37 |
| dr_n_bull | 6 | 18528 | 12.6% | 2.26 | 0.42 | 0.19 | 0.45 |
| di_gold_dist | (1.351, 12.727] | 34871 | 12.4% | 2.23 | 0.50 | 0.22 | 0.56 |
| dr_rsi14 | (62.624, 99.597] | 34822 | 12.1% | 2.19 | 0.51 | 0.22 | 0.51 |
| dr_te_slope | (0.114, 1.106] | 34760 | 12.2% | 2.18 | 0.48 | 0.22 | 0.60 |
| di_silver_pos | (0.929, 51.235] | 33012 | 12.2% | 2.15 | 0.48 | 0.21 | 0.69 |
| dr_n_bull | 5 | 19031 | 11.9% | 2.15 | 0.43 | 0.20 | 0.56 |
| di_n_blue_c | 2 | 28940 | 11.7% | 2.15 | 0.35 | 0.15 | 0.31 |
| di_mountain | 4 | 48261 | 11.6% | 2.12 | 0.48 | 0.21 | 0.38 |
| dr_n_bull | 4 | 20343 | 11.4% | 2.11 | 0.44 | 0.18 | 0.52 |
| dr_n_bear | 0 | 54901 | 11.8% | 2.11 | 0.41 | 0.19 | 0.41 |
| di_overheated | True | 26780 | 12.4% | 2.10 | 0.50 | 0.23 | 0.87 |
| dr_hole_state | 1 | 46492 | 11.0% | 2.04 | 0.43 | 0.19 | 0.53 |
| dr_whales | (81.477, 99.756] | 34835 | 11.4% | 2.04 | 0.45 | 0.21 | 0.64 |
| di_hype | (14.789, 49.121] | 34873 | 10.8% | 2.03 | 0.51 | 0.20 | 0.54 |
| di_bubble | (5.158, 84.553] | 34871 | 11.9% | 2.02 | 0.51 | 0.22 | 0.86 |
| dr_macd_dif | (0.779, 10.962] | 34871 | 11.3% | 2.02 | 0.50 | 0.23 | 0.51 |
| dr_ss_confirm | True | 55621 | 10.8% | 1.97 | 0.38 | 0.16 | 0.50 |
| di_green_dist | (4.688, 30.748] | 28569 | 11.0% | 1.96 | 0.49 | 0.21 | 0.59 |
| dr_poc_ratio | (0.199, 24.92] | 34873 | 10.2% | 1.95 | 0.40 | 0.18 | 0.27 |
| dr_retail | (-0.001, 1.75] | 34835 | 11.3% | 1.94 | 0.51 | 0.23 | 0.69 |
| di_flow | (17.401, 48.651] | 34622 | 10.0% | 1.93 | 0.55 | 0.23 | 0.64 |
| di_above_gold | True | 78703 | 10.3% | 1.89 | 0.45 | 0.20 | 0.47 |
| di_c_bravo | 1 | 64710 | 10.2% | 1.87 | 0.42 | 0.18 | 0.45 |

### Lowest-lift states (what NOT to see)

| feature | value | n | P(target) | lift | fwd10 % |
|---|---|---|---|---|---|
| di_mountain | 0 | 51933 | 0.5% | 0.10 | 0.19 |
| di_below_gold | True | 69710 | 0.4% | 0.08 | 0.18 |
| di_silver_pos | (-5.074000000000001, 0.116] | 33012 | 0.4% | 0.08 | 0.20 |
| di_gold_dist | (-13.501, -1.975] | 34871 | 0.4% | 0.07 | 0.20 |
| dr_n_bull | 0 | 57073 | 0.3% | 0.07 | 0.20 |
| dr_te_slope | (-0.522, -0.0877] | 34760 | 0.2% | 0.05 | 0.21 |
| di_n_pink_c | 3 | 5943 | 0.2% | 0.05 | 0.18 |
| dr_rsi14 | (-0.001, 40.26] | 34822 | 0.2% | 0.04 | 0.21 |
| dr_n_bear | 6 | 19462 | 0.2% | 0.04 | 0.18 |
| dr_n_bear | 7 | 11443 | 0.1% | 0.01 | 0.18 |

### Learned rules — Dragon only (tree depth ≤ 4; ranked by out-of-sample lift)

| rule | train n | train lift | train fwd10 | TEST n | TEST lift | TEST fwd10 | TEST win10 | TEST bear fwd10 | signals/wk (test) | tickers |
|---|---|---|---|---|---|---|---|---|---|---|
| dr_n_bull > 3.5 AND dr_whales > 2.938 AND dr_macd_dif <= 1.398 AND dr_retail <= 43.866 | 33828 | 2.33 | 0.43 | 19060 | 2.44 | 0.45 | 0.514 | 1.06 | 136.84 | 94 |
| dr_n_bull > 3.5 AND dr_whales > 2.938 AND dr_macd_dif > 1.398 AND dr_rsi14 > 65.281 | 4915 | 2.83 | 0.15 | 2693 | 2.09 | 0.78 | 0.597 | -0.72 | 19.33 | 88 |
| dr_n_bull > 3.5 AND dr_whales > 2.938 AND dr_macd_dif <= 1.398 AND dr_retail > 43.866 | 3531 | 1.93 | 0.38 | 2184 | 1.59 | 0.41 | 0.505 | 1.84 | 15.68 | 94 |
| dr_n_bull > 3.5 AND dr_whales > 2.938 AND dr_macd_dif > 1.398 AND dr_rsi14 <= 65.281 | 356 | 1.43 | 0.86 | 294 | 1.32 | 0.79 | 0.575 | -2.36 | 2.11 | 72 |
| dr_n_bull <= 3.5 AND dr_j > 83.456 AND dr_j > 100.134 AND dr_whales > 23.874 | 761 | 1.54 | -0.01 | 421 | 1.2 | -0.0 | 0.451 | -0.28 | 3.02 | 90 |
| dr_n_bull <= 3.5 AND dr_j > 83.456 AND dr_j > 100.134 AND dr_whales <= 23.874 | 1807 | 0.7 | 0.18 | 836 | 0.7 | -0.03 | 0.482 | 1.16 | 6.0 | 94 |
| dr_n_bull <= 3.5 AND dr_j <= 83.456 AND dr_n_bull > 2.5 AND dr_n_bear <= 0.5 | 2558 | 0.5 | 0.39 | 1539 | 0.62 | 0.27 | 0.493 | 0.2 | 11.05 | 94 |
| dr_n_bull <= 3.5 AND dr_j > 83.456 AND dr_j <= 100.134 AND dr_whales <= 25.099 | 3122 | 0.3 | 0.08 | 1639 | 0.45 | 0.25 | 0.502 | 0.99 | 11.77 | 94 |

### Learned rules — Diamond only (tree depth ≤ 4; ranked by out-of-sample lift)

| rule | train n | train lift | train fwd10 | TEST n | TEST lift | TEST fwd10 | TEST win10 | TEST bear fwd10 | signals/wk (test) | tickers |
|---|---|---|---|---|---|---|---|---|---|---|
| di_gold_dist > 0.031 AND di_gold_dist > 0.703 AND di_bubble > 8.734 AND di_gold_dist > 2.041 | 3983 | 2.7 | 0.46 | 2039 | 2.38 | 0.83 | 0.603 | 0.92 | 14.64 | 79 |
| di_gold_dist > 0.031 AND di_gold_dist > 0.703 AND di_bubble > 8.734 AND di_gold_dist <= 2.041 | 1877 | 3.54 | 0.08 | 829 | 2.25 | 0.61 | 0.606 | None | 5.95 | 42 |
| di_gold_dist > 0.031 AND di_gold_dist > 0.703 AND di_bubble <= 8.734 AND di_green_dist > 2.404 | 26033 | 2.19 | 0.43 | 14690 | 2.22 | 0.51 | 0.527 | 1.26 | 105.47 | 94 |
| di_gold_dist > 0.031 AND di_gold_dist <= 0.703 AND di_n_blue_c > 1.5 AND di_green_dist > 1.382 | 3175 | 2.42 | 0.28 | 1722 | 2.18 | 0.36 | 0.489 | 1.03 | 12.36 | 94 |
| di_gold_dist > 0.031 AND di_gold_dist > 0.703 AND di_bubble <= 8.734 AND di_green_dist <= 2.404 | 3342 | 1.52 | 0.78 | 1620 | 2.18 | 0.45 | 0.521 | 0.47 | 11.63 | 94 |
| di_gold_dist > 0.031 AND di_gold_dist <= 0.703 AND di_n_blue_c > 1.5 AND di_green_dist <= 1.382 | 381 | 1.39 | 0.64 | 232 | 2.01 | 0.58 | 0.517 | 1.35 | 1.67 | 78 |
| di_gold_dist <= 0.031 AND di_n_blue_c > 1.5 AND di_flow > -18.7 AND di_purple_dist > 5.317 | 528 | 1.07 | 0.55 | 322 | 1.87 | 0.44 | 0.441 | -0.48 | 2.31 | 62 |
| di_gold_dist <= 0.031 AND di_n_blue_c > 1.5 AND di_flow > -18.7 AND di_purple_dist <= 5.317 | 3551 | 2.0 | 0.26 | 2301 | 1.72 | 0.23 | 0.488 | 0.74 | 16.52 | 94 |

### Learned rules — Dragon + Diamond (tree depth ≤ 4; ranked by out-of-sample lift)

| rule | train n | train lift | train fwd10 | TEST n | TEST lift | TEST fwd10 | TEST win10 | TEST bear fwd10 | signals/wk (test) | tickers |
|---|---|---|---|---|---|---|---|---|---|---|
| dr_n_bull > 3.5 AND di_bubble > 8.734 AND dr_te_slope > 0.166 AND dr_j <= 102.008 | 3754 | 2.95 | 0.27 | 1915 | 2.44 | 0.84 | 0.607 | 0.88 | 13.75 | 78 |
| dr_n_bull > 3.5 AND di_bubble > 8.734 AND dr_te_slope <= 0.166 AND di_n_pink_c <= 0.5 | 1063 | 4.28 | -0.06 | 496 | 2.43 | 0.52 | 0.567 | None | 3.56 | 42 |
| dr_n_bull > 3.5 AND di_bubble <= 8.734 AND dr_whales > 2.938 AND dr_whales > 81.755 | 13981 | 2.04 | 0.49 | 9248 | 2.37 | 0.44 | 0.516 | 1.15 | 66.4 | 94 |
| dr_n_bull > 3.5 AND di_bubble <= 8.734 AND dr_whales > 2.938 AND dr_whales <= 81.755 | 22934 | 2.35 | 0.38 | 12102 | 2.25 | 0.45 | 0.51 | 1.35 | 86.89 | 94 |
| dr_n_bull > 3.5 AND di_bubble > 8.734 AND dr_te_slope > 0.166 AND dr_j > 102.008 | 627 | 1.75 | 0.21 | 313 | 2.17 | 0.81 | 0.617 | 1.05 | 2.25 | 66 |
| dr_n_bull <= 3.5 AND di_n_blue_c > 1.5 AND di_silver_pos > 0.058 AND di_purple_dist > 5.371 | 467 | 1.05 | 0.28 | 330 | 2.06 | 0.31 | 0.427 | -0.81 | 2.37 | 60 |
| dr_n_bull > 3.5 AND di_bubble > 8.734 AND dr_te_slope <= 0.166 AND di_n_pink_c > 0.5 | 374 | 2.88 | 0.88 | 157 | 1.98 | 1.02 | 0.662 | None | 1.13 | 28 |
| dr_n_bull <= 3.5 AND di_n_blue_c > 1.5 AND di_silver_pos > 0.058 AND di_purple_dist <= 5.371 | 4380 | 2.15 | 0.25 | 2872 | 1.88 | 0.29 | 0.494 | 0.48 | 20.62 | 94 |