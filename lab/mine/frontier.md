# Hit-rate frontier — Dragon C (guarded) signals, 3547 signals, 94 tickers

P&L in ATR units of the signal bar (1 ATR ≈ the stock's daily range; ~5 % on these names). win = share of trades that ended > 0. exp = average trade in ATR. Grid = target × stop × max bars × entry style × setup quality.

## Designs with ≥ 80 % winners AND positive expectancy (55 of 1140)

| entry | quality | target | stop | max bars | trades | win | exp (ATR) | OOS trades | OOS win | OOS exp |
|---|---|---|---|---|---|---|---|---|---|---|
| next open | fresh + ≥6 panels + ATR ≥ 5 % | 0.5 | 3.0 | 10 | 671 | 0.848 | 0.091 | 392 | 0.852 | 0.099 |
| next open, gap ≤ 0.5 ATR | fresh + ≥6 panels + ATR ≥ 5 % | 0.5 | 3.0 | 10 | 594 | 0.845 | 0.089 | 347 | 0.844 | 0.08 |
| next open | ATR ≥ 5 % | 0.5 | 4.0 | 20 | 1662 | 0.882 | 0.087 | 903 | 0.895 | 0.121 |
| next open, gap ≤ 0.5 ATR | fresh + ≥6 panels + ATR ≥ 5 % | 0.5 | 4.0 | 20 | 594 | 0.889 | 0.086 | 347 | 0.896 | 0.097 |
| next open, gap ≤ 0.5 ATR | ATR ≥ 5 % | 0.5 | 4.0 | 20 | 1481 | 0.88 | 0.086 | 806 | 0.891 | 0.109 |
| next open | fresh + ≥6 panels + ATR ≥ 5 % | 0.5 | 4.0 | 20 | 671 | 0.888 | 0.083 | 392 | 0.898 | 0.108 |
| next open, gap ≤ 0.5 ATR | ATR ≥ 5 % | 0.5 | 4.0 | 10 | 1481 | 0.835 | 0.082 | 806 | 0.841 | 0.095 |
| next open | fresh + ≥6 panels + ATR ≥ 5 % | 0.5 | 4.0 | 10 | 671 | 0.851 | 0.082 | 392 | 0.852 | 0.08 |
| next open, gap ≤ 0.5 ATR | fresh + ≥6 panels + ATR ≥ 5 % | 0.5 | 4.0 | 10 | 594 | 0.848 | 0.081 | 347 | 0.844 | 0.061 |
| next open | ATR ≥ 5 % | 0.5 | 3.0 | 20 | 1662 | 0.868 | 0.078 | 903 | 0.887 | 0.134 |
| next open | ATR ≥ 5 % | 0.5 | 4.0 | 10 | 1662 | 0.837 | 0.078 | 903 | 0.848 | 0.105 |
| next open, gap ≤ 0.5 ATR | ATR ≥ 5 % | 0.5 | 3.0 | 20 | 1481 | 0.866 | 0.077 | 806 | 0.882 | 0.118 |
| next open, gap ≤ 0.5 ATR | ATR ≥ 5 % | 0.5 | 3.0 | 10 | 1481 | 0.831 | 0.075 | 806 | 0.841 | 0.101 |
| next open | fresh + ≥6 panels + ATR ≥ 5 % | 0.5 | 3.0 | 20 | 671 | 0.87 | 0.074 | 392 | 0.88 | 0.099 |
| next open, gap ≤ 0.5 ATR | fresh + ≥6 panels + ATR ≥ 5 % | 0.5 | 3.0 | 20 | 594 | 0.87 | 0.074 | 347 | 0.876 | 0.081 |
| next open | ATR ≥ 5 % | 0.5 | 3.0 | 10 | 1662 | 0.833 | 0.074 | 903 | 0.848 | 0.115 |
| next open, gap ≤ 0.5 ATR | fresh + ≥6 panels + ATR ≥ 5 % | 0.5 | 2.0 | 10 | 594 | 0.818 | 0.066 | 347 | 0.813 | 0.053 |
| next open, gap ≤ 0.5 ATR | fresh + ≥6 panels + ATR ≥ 5 % | 0.5 | 2.0 | 20 | 594 | 0.825 | 0.063 | 347 | 0.821 | 0.055 |
| next open | fresh + ≥6 panels + ATR ≥ 5 % | 0.5 | 2.0 | 10 | 671 | 0.817 | 0.059 | 392 | 0.814 | 0.052 |
| next open | fresh + ≥6 panels + ATR ≥ 5 % | 0.5 | 2.0 | 20 | 671 | 0.821 | 0.054 | 392 | 0.819 | 0.048 |
| next open | all | 0.5 | 4.0 | 20 | 3545 | 0.873 | 0.046 | 1563 | 0.874 | 0.04 |
| next open | fresh | 0.5 | 3.0 | 10 | 1824 | 0.83 | 0.045 | 839 | 0.827 | 0.016 |
| next open | all | 0.5 | 3.0 | 20 | 3545 | 0.86 | 0.045 | 1563 | 0.862 | 0.052 |
| next open, gap ≤ 0.5 ATR | ATR ≥ 5 % | 0.5 | 2.0 | 10 | 1481 | 0.803 | 0.042 | 806 | 0.801 | 0.033 |
| next open | fresh | 0.5 | 3.0 | 20 | 1824 | 0.859 | 0.04 | 839 | 0.855 | 0.018 |
| next open | fresh | 0.5 | 4.0 | 10 | 1824 | 0.832 | 0.038 | 839 | 0.828 | 0.002 |
| next open | all | 0.5 | 3.0 | 10 | 3545 | 0.827 | 0.038 | 1563 | 0.823 | 0.029 |
| next open, gap ≤ 0.5 ATR | all | 0.5 | 3.0 | 20 | 3225 | 0.856 | 0.037 | 1411 | 0.858 | 0.037 |
| next open, gap ≤ 0.5 ATR | all | 0.5 | 4.0 | 20 | 3225 | 0.87 | 0.037 | 1411 | 0.87 | 0.029 |
| next open | fresh + ≥6 panels | 0.5 | 3.0 | 10 | 1492 | 0.831 | 0.037 | 703 | 0.828 | 0.011 |
| next open | fresh | 0.5 | 4.0 | 20 | 1824 | 0.871 | 0.037 | 839 | 0.87 | 0.023 |
| next open | all | 0.5 | 4.0 | 10 | 3545 | 0.831 | 0.037 | 1563 | 0.825 | 0.02 |
| next open, gap ≤ 0.5 ATR | ATR ≥ 5 % | 0.5 | 2.0 | 20 | 1481 | 0.812 | 0.036 | 806 | 0.813 | 0.033 |
| next open | fresh + ≥6 panels | 0.5 | 4.0 | 20 | 1492 | 0.873 | 0.034 | 703 | 0.873 | 0.03 |
| next open, gap ≤ 0.5 ATR | all | 0.5 | 3.0 | 10 | 3225 | 0.824 | 0.034 | 1411 | 0.818 | 0.018 |
| next open | fresh + ≥6 panels | 0.5 | 3.0 | 20 | 1492 | 0.858 | 0.033 | 703 | 0.855 | 0.016 |
| next open, gap ≤ 0.5 ATR | all | 0.5 | 4.0 | 10 | 3225 | 0.828 | 0.032 | 1411 | 0.82 | 0.011 |
| next open | fresh | 0.5 | 2.0 | 10 | 1824 | 0.8 | 0.031 | 839 | 0.794 | 0.007 |
| next open, gap ≤ 0.5 ATR | fresh | 0.5 | 3.0 | 10 | 1668 | 0.826 | 0.031 | 759 | 0.818 | -0.011 |
| next open | fresh | 0.5 | 2.0 | 20 | 1824 | 0.81 | 0.029 | 839 | 0.802 | 0.008 |

## Best expectancy at each win-rate band (all designs)

| win band | entry | quality | target | stop | max bars | trades | win | exp (ATR) | OOS win | OOS exp |
|---|---|---|---|---|---|---|---|---|---|---|
| 80–90 % | next open | fresh + ≥6 panels + ATR ≥ 5 % | 0.5 | 3.0 | 10 | 671 | 0.848 | 0.091 | 0.852 | 0.099 |
| 80–90 % | next open, gap ≤ 0.5 ATR | fresh + ≥6 panels + ATR ≥ 5 % | 0.5 | 3.0 | 10 | 594 | 0.845 | 0.089 | 0.844 | 0.08 |
| 80–90 % | next open | ATR ≥ 5 % | 0.5 | 4.0 | 20 | 1662 | 0.882 | 0.087 | 0.895 | 0.121 |
| 70–80 % | next open | fresh + ≥6 panels + ATR ≥ 5 % | 1.5 | 4.0 | 20 | 671 | 0.709 | 0.227 | 0.712 | 0.264 |
| 70–80 % | next open, gap ≤ 0.5 ATR | fresh + ≥6 panels + ATR ≥ 5 % | 1.5 | 4.0 | 20 | 594 | 0.709 | 0.22 | 0.718 | 0.29 |
| 70–80 % | next open | fresh + ≥6 panels | 1.5 | 4.0 | 20 | 1492 | 0.708 | 0.214 | 0.694 | 0.148 |
| 60–70 % | next open, gap ≤ 0.5 ATR | ATR ≥ 5 % | 2.0 | 4.0 | 20 | 1481 | 0.638 | 0.299 | 0.658 | 0.402 |
| 60–70 % | next open | ATR ≥ 5 % | 2.0 | 4.0 | 20 | 1662 | 0.637 | 0.293 | 0.653 | 0.384 |
| 60–70 % | next open | fresh + ≥6 panels + ATR ≥ 5 % | 2.0 | 4.0 | 20 | 671 | 0.647 | 0.293 | 0.648 | 0.338 |
| 50–60 % | next open | fresh + ≥6 panels + ATR ≥ 5 % | 3.0 | 4.0 | 20 | 671 | 0.584 | 0.543 | 0.597 | 0.628 |
| 50–60 % | next open, gap ≤ 0.5 ATR | fresh + ≥6 panels + ATR ≥ 5 % | 3.0 | 4.0 | 20 | 594 | 0.582 | 0.519 | 0.605 | 0.67 |
| 50–60 % | next open | fresh + ≥6 panels + ATR ≥ 5 % | 3.0 | 3.0 | 20 | 671 | 0.56 | 0.506 | 0.571 | 0.553 |
| 0–50 % | next open, gap ≤ 0.5 ATR | fresh + ≥6 panels + ATR ≥ 5 % | 3.0 | 2.0 | 20 | 594 | 0.481 | 0.381 | 0.476 | 0.357 |
| 0–50 % | next open | fresh + ≥6 panels + ATR ≥ 5 % | 3.0 | 2.0 | 20 | 671 | 0.478 | 0.379 | 0.464 | 0.31 |
| 0–50 % | next open | fresh + ≥6 panels | 3.0 | 2.0 | 20 | 1492 | 0.487 | 0.36 | 0.468 | 0.281 |