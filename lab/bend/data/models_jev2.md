# Jev over 342 cross-field models: useful × portable × (½ + ½ documented)

| domain | model | family | useful | portable | documented | rank | measurement |
|---|---|---|---|---|---|---|---|
| economics | Amihud illiquidity | ENERGY | 1.73 | 0.85 | 0.72 | 1.26463 | |return| per dollar volume over 20 bars |
| statistics | Winsorised momentum | FORCE | 1.86 | 0.84 | 0.6 | 1.2499200000000001 | 20-bar return with the two largest daily moves removed |
| psychology | Capitulation | ENERGY | 1.69 | 0.87 | 0.54 | 1.132131 | 3-ATR down day on 4× volume closing near the low |
| economics | Turn of the month | STRUCTURE | 1.54 | 0.81 | 0.68 | 1.047816 | day of month within the last or first three trading days |
| stochastic processes | Drawdown duration | STRUCTURE | 1.47 | 0.92 | 0.44 | 0.973728 | bars since the 60-bar high |
| control theory | PID proportional | STRUCTURE | 1.48 | 0.85 | 0.5 | 0.9435 | distance to the 50-day mean in ATR |
| economics | Bubble indicator | STRUCTURE | 1.4 | 0.88 | 0.51 | 0.93016 | log price above its 250-bar trend with accelerating slope |
| control theory | Dead band | STRUCTURE | 1.49 | 0.91 | 0.35 | 0.9152325000000001 | bars within a ±0.5 ATR band around the 20-day mean |
| architecture | Cantilever | STRUCTURE | 1.46 | 0.88 | 0.38 | 0.8865119999999999 | price extended beyond the 20-day VWAP by more than 3 ATR |
| medicine | Remission | STRUCTURE | 1.43 | 0.88 | 0.4 | 0.8808799999999999 | bars since the last 2-ATR down day with no new low |
| social dynamics | Bandwagon | FORCE | 1.36 | 0.86 | 0.49 | 0.871352 | consecutive up-days with rising volume |
| stochastic processes | Ornstein–Uhlenbeck speed | FORCE | 1.57 | 0.78 | 0.4 | 0.8572200000000001 | regression of daily change on lagged deviation from the 60-bar mean |
| military and operations research | Retreat | STRUCTURE | 1.41 | 0.82 | 0.47 | 0.8498069999999999 | close below the 10-bar low on volume above average |
| economics | Kyle's lambda | FORCE | 1.64 | 0.61 | 0.69 | 0.8453379999999999 | slope of price change on signed volume over 60 bars |
| economics | Overnight vs intraday | STRUCTURE | 1.47 | 0.75 | 0.53 | 0.8434125 | share of the 20-bar move made in gaps vs within sessions |
| genetics and evolution | Red Queen | FORCE | 1.3 | 0.86 | 0.47 | 0.8217300000000001 | relative strength of the stock vs its own 250-bar trend |
| social dynamics | Critical mass | ENERGY | 1.28 | 0.91 | 0.41 | 0.821184 | volume above 2× average for 3 consecutive bars |
| signal processing | Kurtosis of returns | STRUCTURE | 1.15 | 0.93 | 0.53 | 0.8181674999999999 | excess kurtosis over 60 bars |
| statistics | Median absolute deviation | ENERGY | 1.4 | 0.87 | 0.34 | 0.81606 | robust volatility over 20 bars relative to ATR |
| statistics | Quantile position | STRUCTURE | 1.34 | 0.81 | 0.5 | 0.8140500000000002 | percentile of the close within the 250-bar close distribution |
| signal processing | Skewness of returns | STRUCTURE | 1.14 | 0.9 | 0.58 | 0.81054 | skewness over 60 bars |
| extreme value | Peaks over threshold | ENERGY | 1.29 | 0.88 | 0.41 | 0.8003159999999999 | count of |return| over 2 ATR in 60 bars |
| psychology | Euphoria | ENERGY | 1.29 | 0.89 | 0.39 | 0.7979295000000002 | 3-ATR up day on 4× volume closing near the high after a 20-bar rally |
| economics | Roll spread | ENERGY | 1.23 | 0.81 | 0.6 | 0.7970400000000001 | negative lag-1 autocovariance of daily changes as an effective spread |
| stochastic processes | Local time at a level | STRUCTURE | 1.29 | 0.91 | 0.35 | 0.7923825000000002 | bars spent within 0.25 ATR of the 50-day mean in the last 50 |
| extreme value | Gnedenko tail index | STRUCTURE | 1.43 | 0.82 | 0.35 | 0.791505 | Hill estimator of the return tail over 250 bars |
| materials science | Annealing | ENERGY | 1.33 | 0.85 | 0.35 | 0.7630875000000001 | shrinking daily ranges with rising volume over 20 bars |
| military and operations research | Siege | ENERGY | 1.35 | 0.84 | 0.34 | 0.7597800000000001 | 20 bars within a 3-ATR range with volume declining |
| stochastic processes | Variance ratio 20 | ENERGY | 1.23 | 0.87 | 0.41 | 0.7544204999999999 | variance of 20-day returns over 20× the 1-day variance |
| stochastic processes | Drawup duration | STRUCTURE | 1.26 | 0.91 | 0.31 | 0.7510230000000001 | bars since the 60-bar low |
| mechanics | Inertia | FORCE | 1.22 | 0.81 | 0.49 | 0.736209 | bars the current direction has persisted weighted by the size of the moves |
| stochastic processes | Variance ratio 5 | ENERGY | 1.2 | 0.89 | 0.37 | 0.7315800000000001 | variance of 5-day returns over 5× the 1-day variance |
| psychology | Anchoring | STRUCTURE | 1.44 | 0.78 | 0.3 | 0.7300800000000001 | distance to the 250-bar high and low as anchors, in ATR |
| control theory | Overshoot | STRUCTURE | 1.32 | 0.8 | 0.37 | 0.7233600000000001 | excursion beyond the 20-day mean after crossing it, in ATR |
| mechanics | Work done | ENERGY | 1.39 | 0.78 | 0.33 | 0.7209930000000001 | sum of signed (close − open) × volume over 10 bars divided by total volume |
| economics | Supply-demand imbalance | FORCE | 1.21 | 0.8 | 0.47 | 0.71148 | up-volume minus down-volume over 10 bars relative to total |
| signal processing | Savitzky–Golay slope | FORCE | 1.21 | 0.9 | 0.29 | 0.702405 | smoothed derivative of the close over 11 bars, in ATR |
| economics | Price discovery | FORCE | 1.23 | 0.79 | 0.44 | 0.699624 | share of the daily move that happens in the opening gap over 10 bars |
| control theory | PID integral | STRUCTURE | 1.24 | 0.83 | 0.34 | 0.689564 | cumulative distance to the 50-day mean over 20 bars |
| control theory | PID derivative | FORCE | 1.15 | 0.89 | 0.34 | 0.6857449999999999 | change of the distance to the 50-day mean over 5 bars |
| information theory | Mutual information price-volume | ENERGY | 1.09 | 0.89 | 0.41 | 0.6839205 | mutual information of return sign and volume direction over 60 bars |
| signal processing | Envelope detector | STRUCTURE | 1.22 | 0.86 | 0.3 | 0.6819799999999999 | 20-bar high-low envelope width relative to ATR |
| architecture | Foundation | STRUCTURE | 1.39 | 0.76 | 0.29 | 0.681378 | number of touches of the 60-bar low with rising lows |
| stochastic processes | Hidden regime | ENERGY | 1.24 | 0.84 | 0.3 | 0.67704 | bars since the volatility regime (ATR ratio) last crossed 1 |
| economics | Information ratio | FORCE | 1.33 | 0.73 | 0.39 | 0.6747755000000001 | 20-bar return over the standard deviation of daily returns |
| soviet school | Zeldovich collapse | ENERGY | 1.25 | 0.83 | 0.3 | 0.674375 | volume collapsing into a narrow price band (share of 20-bar volume within 1 ATR) |
| stochastic processes | First-passage distance | STRUCTURE | 1.26 | 0.82 | 0.3 | 0.67158 | distance to the nearer of the 20-bar high and low in ATR |
| fluids | Buoyancy | STRUCTURE | 1.16 | 0.87 | 0.33 | 0.671118 | closes above the 20-day VWAP on below-average volume for 5 bars |
| stochastic processes | Lévy jump count | STRUCTURE | 1.21 | 0.88 | 0.26 | 0.670824 | number of returns beyond 3 daily-change standard deviations in 60 bars |
| psychology | Attention | ENERGY | 1.33 | 0.71 | 0.41 | 0.6657315 | volume relative to its 250-bar median |
| psychology | Recency bias | FORCE | 1.38 | 0.69 | 0.38 | 0.6570179999999999 | 5-bar return vs 60-bar return divergence |
| agriculture | Harvest | ENERGY | 1.2 | 0.76 | 0.44 | 0.6566399999999999 | volume spike after a 20-bar rally (distribution) |
| economics | Diminishing returns | FORCE | 1.17 | 0.81 | 0.38 | 0.653913 | 5-bar return relative to the return of the prior 5 bars in a run |
| thermodynamics | Phase transition | ENERGY | 1.06 | 0.92 | 0.34 | 0.6533840000000001 | sharp change in the volatility regime: ATR(5)/ATR(50) crossing 2 or 0.5 within 5 bars |
| astronomy | Supernova | ENERGY | 1.12 | 0.87 | 0.34 | 0.6528480000000001 | a 4-ATR up day on 5× volume within the last 60 bars |
| games and sports | Momentum in games | FORCE | 1.25 | 0.7 | 0.49 | 0.651875 | consecutive wins: up-day streak weighted by size |
| psychology | Regret | STRUCTURE | 1.11 | 0.84 | 0.38 | 0.6433559999999999 | gap up on high volume followed by a close below the open |
| control theory | Kalman innovation | STRUCTURE | 1.45 | 0.67 | 0.32 | 0.64119 | z-score of today's close vs the Kalman prediction |
| control theory | Gain scheduling | FORCE | 1.29 | 0.71 | 0.4 | 0.6411299999999999 | trend slope conditioned on the volatility regime |
| stochastic processes | Ornstein–Uhlenbeck half-life | FORCE | 1.18 | 0.76 | 0.42 | 0.636728 | log 2 over the reversion speed |
| military and operations research | Supply line length | STRUCTURE | 1.07 | 0.89 | 0.33 | 0.6332795000000001 | bars since the last 20-bar high as the extension of the advance |
| control theory | Kalman velocity | FORCE | 1.19 | 0.81 | 0.31 | 0.6313545 | Kalman-filtered velocity of the close (constant-velocity model) |
| topology and geometry | Angle of attack | STRUCTURE | 1.17 | 0.87 | 0.24 | 0.631098 | arctangent of the 10-bar slope in ATR per bar |
| stochastic processes | Markov down→down | STRUCTURE | 1.09 | 0.89 | 0.3 | 0.630565 | probability of a down day after a down day over 60 bars |
| statistics | Bootstrap stability | STRUCTURE | 1.51 | 0.62 | 0.34 | 0.6272540000000001 | share of bootstrap resamples where the 20-bar slope keeps its sign |
| control theory | Settling time | STRUCTURE | 1.38 | 0.72 | 0.26 | 0.625968 | bars to stay within ±0.5 ATR of the 20-day mean after a 2-ATR deviation |
| thermodynamics | Cooling curve | STRUCTURE | 1.19 | 0.8 | 0.31 | 0.62356 | decay of daily range after a 3-ATR day, fitted exponent over 10 bars |
| signal processing | Zero-crossing rate | STRUCTURE | 1.03 | 0.93 | 0.3 | 0.622635 | sign changes of the detrended close over 20 bars |
| stochastic processes | Hawkes intensity | ENERGY | 1.16 | 0.8 | 0.34 | 0.62176 | exponentially decaying count of |return| > 1.5 ATR events |
| waves and optics | Refraction | STRUCTURE | 1.1 | 0.83 | 0.35 | 0.616275 | change of trend slope on crossing the 50-day mean |
| games and sports | Chess tempo | STRUCTURE | 1.15 | 0.83 | 0.29 | 0.6156524999999999 | bars the trend has held without a 1-ATR pullback |
| mechanics | Center of mass | STRUCTURE | 1.25 | 0.74 | 0.32 | 0.6105 | volume-weighted average price over the last swing vs the current close, in ATR |
| psychology | Round-number magnet | STRUCTURE | 1.18 | 0.71 | 0.44 | 0.6032159999999999 | distance of the close to the nearest round price level in ATR |
| economics | Liquidity spiral | ENERGY | 1.02 | 0.82 | 0.44 | 0.602208 | falling dollar volume with rising ranges over 10 bars |
| computing | Load average | ENERGY | 1.18 | 0.79 | 0.29 | 0.601269 | volume relative to average over 1, 5 and 15 bars |
| physiology | Fever | STRUCTURE | 1.09 | 0.89 | 0.23 | 0.5966115000000001 | daily range above 2 ATR for 3 consecutive bars |
| ecology | Resilience | STRUCTURE | 1.24 | 0.74 | 0.3 | 0.59644 | bars to recover half of a 2-ATR drop |
| medicine | Triage | ENERGY | 1.33 | 0.63 | 0.42 | 0.594909 | ranking of the day's move by range and volume vs 60 bars |
| meteorology | Pressure system | STRUCTURE | 1.08 | 0.86 | 0.28 | 0.5944320000000001 | 20-day VWAP above or below the 50-day VWAP and the gap in ATR |
| stochastic processes | Markov up→up | STRUCTURE | 1.05 | 0.89 | 0.27 | 0.5934075000000001 | probability of an up day after an up day over 60 bars |
| architecture | Ceiling | STRUCTURE | 1.26 | 0.73 | 0.29 | 0.593271 | number of touches of the 60-bar high with falling highs |
| physiology | Pulse | STRUCTURE | 1.12 | 0.82 | 0.29 | 0.592368 | dominant short cycle (3–8 bars) amplitude relative to ATR |
| psychology | Loss aversion asymmetry | ENERGY | 0.93 | 0.88 | 0.44 | 0.589248 | volume on down-days over volume on up-days in the last 20 bars |
| soviet school | Kolmogorov–Smirnov | STRUCTURE | 1.13 | 0.84 | 0.24 | 0.5885039999999999 | KS distance between the recent 20-bar and the 250-bar return distributions |
| physiology | Refractory period | STRUCTURE | 1.06 | 0.87 | 0.27 | 0.585597 | bars since the last 2-ATR move during which no further 2-ATR move occurred |
| meteorology | Storm intensity | STRUCTURE | 1.01 | 0.85 | 0.35 | 0.5794875 | maximum daily range over 5 bars in ATR |
| mechanics | Torque | STRUCTURE | 1.08 | 0.85 | 0.26 | 0.5783400000000001 | close position within the daily range times daily range, summed over 5 bars |
| control theory | Bang-bang control | STRUCTURE | 1.21 | 0.74 | 0.29 | 0.577533 | alternation between range extremes: closes at the 20-bar high or low |
| information theory | Redundancy | STRUCTURE | 1.01 | 0.91 | 0.25 | 0.5744375 | lag-1 to lag-5 autocorrelation sum of returns over 40 bars |
| chemistry | Saturation | STRUCTURE | 1.07 | 0.87 | 0.23 | 0.5725035 | share of the 20-bar range covered by the last 5 bars |
| reliability engineering | Mean time between failures | STRUCTURE | 1.11 | 0.83 | 0.24 | 0.571206 | mean bars between 2-ATR down days over 250 bars |
| mechanics | Power | FORCE | 1.27 | 0.65 | 0.38 | 0.569595 | work per bar: 5-bar net move times average volume, in ATR |
| chemistry | Crystallisation | STRUCTURE | 1.01 | 0.87 | 0.29 | 0.5667615 | narrowing of daily ranges with closes near the same level for 10 bars |
| architecture | Arch | STRUCTURE | 1.06 | 0.82 | 0.28 | 0.556288 | swing high with symmetric rise and fall in bars and size |
| music and language | Rest | ENERGY | 1.02 | 0.85 | 0.28 | 0.55488 | bars with range below 0.5 ATR in the last 10 |
| economics | Herding | ENERGY | 1.06 | 0.75 | 0.39 | 0.552525 | share of days with volume above average and direction agreeing with the 5-bar trend |
| chemistry | Le Chatelier | STRUCTURE | 1.02 | 0.76 | 0.42 | 0.550392 | response opposing a shock: rebound size after a 2-ATR down day |
| extreme value | Expected 20-day maximum | STRUCTURE | 1.3 | 0.66 | 0.28 | 0.54912 | expected maximum excursion in ATR from the tail index |
| mechanics | Elastic collision | STRUCTURE | 1.12 | 0.76 | 0.29 | 0.5490240000000001 | bounce off a level: size of the rebound after touching the 50-day mean, in ATR |
| information theory | Surprise | STRUCTURE | 1.18 | 0.71 | 0.3 | 0.5445699999999999 | negative log probability of today's return under the 60-bar distribution |
| ecology | Keystone bar | STRUCTURE | 0.99 | 0.81 | 0.35 | 0.5412825000000001 | share of the 20-bar volume in the single largest bar |
| epidemiology | Superspreader bar | ENERGY | 0.96 | 0.88 | 0.28 | 0.540672 | a single bar with volume above 4× average and range above 2 ATR |
| waves and optics | Signal envelope | STRUCTURE | 1.01 | 0.87 | 0.23 | 0.5404005000000001 | Hilbert-style envelope of the detrended close over 20 bars |
| statistical physics | Metastability | STRUCTURE | 1.12 | 0.77 | 0.25 | 0.539 | bars spent within ±1 ATR of a level before a 2-ATR escape |
| signal processing | Wiener filter slope | FORCE | 1.14 | 0.75 | 0.25 | 0.534375 | optimal linear prediction of the next close minus the current close, in ATR |
| meteorology | Barometer falling | STRUCTURE | 0.94 | 0.86 | 0.32 | 0.5335439999999999 | three consecutive declines of the 20-day VWAP |
| stochastic processes | Martingale test | STRUCTURE | 1.13 | 0.74 | 0.27 | 0.530987 | mean of the next-day return conditional on the sign of today's over 60 bars |
| economics | Kelly fraction | STRUCTURE | 1.3 | 0.6 | 0.36 | 0.5304 | expected edge over variance from the 60-bar return distribution |
| extreme value | Block maxima trend | STRUCTURE | 0.95 | 0.87 | 0.28 | 0.52896 | trend of the monthly maximum range over 12 months |
| signal processing | Spectral flatness | STRUCTURE | 1.02 | 0.84 | 0.23 | 0.526932 | flatness of the 64-bar return spectrum |
| electromagnetism | Resistance | ENERGY | 0.97 | 0.78 | 0.39 | 0.5258370000000001 | volume required per ATR of movement over 10 bars |
| structural engineering | Load-bearing level | ENERGY | 1.05 | 0.75 | 0.32 | 0.51975 | volume at the 50-bar low relative to average |
| extreme value | Return level | STRUCTURE | 1.12 | 0.73 | 0.27 | 0.5191760000000001 | the 250-bar 1-in-50 daily move vs today's move |
| quantum | Tunnelling | STRUCTURE | 1.11 | 0.7 | 0.33 | 0.5167050000000001 | closes beyond the 50-bar high on volume below average, followed by return |
| statistics | Residual autocorrelation | STRUCTURE | 1.05 | 0.74 | 0.32 | 0.51282 | lag-1 autocorrelation of residuals from the 20-bar fit |
| statistical physics | Detrended fluctuation exponent | STRUCTURE | 0.86 | 0.89 | 0.33 | 0.508991 | DFA exponent of daily returns over 100 bars |
| economics | Elasticity | ENERGY | 0.99 | 0.79 | 0.3 | 0.5083650000000001 | percent price change per percent volume change over 20 bars |
| neuroscience | Neural adaptation | FORCE | 1.08 | 0.7 | 0.33 | 0.5027400000000001 | decline of 5-bar velocity while volume stays high |
| structural engineering | Safety factor | STRUCTURE | 1.22 | 0.6 | 0.37 | 0.50142 | distance to the stop level (4 ATR) vs the 20-bar MAE distribution |
| quantum | Zero-point energy | ENERGY | 1.01 | 0.82 | 0.21 | 0.501061 | minimum realized range over the last 60 bars relative to the current range |
| information theory | Kullback–Leibler divergence | STRUCTURE | 1.0 | 0.81 | 0.23 | 0.49815000000000004 | divergence of the recent 20-bar return distribution from the 250-bar one |
| mechanics | Gravitational pull | ENERGY | 0.91 | 0.83 | 0.31 | 0.4947215 | strength of a level: volume traded within 0.5 ATR of the 50-day mean over 50 bars |
| military and operations research | Force concentration | ENERGY | 0.98 | 0.76 | 0.32 | 0.49156800000000006 | share of the 20-bar volume in the top 3 bars |
| waves and optics | Phase lag price vs volume | ENERGY | 0.91 | 0.83 | 0.3 | 0.490945 | lag maximising the cross-correlation of |returns| and volume over 60 bars |
| topology and geometry | Golden ratio retrace | STRUCTURE | 1.02 | 0.77 | 0.25 | 0.490875 | closeness of the current retrace to 0.382 or 0.618 of the last leg |
| oceanography | Tidal range | STRUCTURE | 0.9 | 0.83 | 0.31 | 0.489285 | difference between the 20-bar high and low in ATR |
| soviet school | Krylov subspace | STRUCTURE | 1.09 | 0.76 | 0.18 | 0.48875599999999997 | projection of the last 20 closes on the first three powers of the shift (trend, curvature, oscillation) |
| signal processing | Matched filter | STRUCTURE | 1.33 | 0.56 | 0.3 | 0.4841200000000001 | correlation of the last 20 bars with the average pre-breakout shape from history |
| soviet school | Kolmogorov–Wiener predictor | STRUCTURE | 1.22 | 0.62 | 0.28 | 0.48409599999999997 | linear prediction of the next close from the last 10 returns, in ATR |
| military and operations research | OODA loop | STRUCTURE | 1.02 | 0.63 | 0.49 | 0.478737 | bars between a volume surge and the price reaction |
| agriculture | Drought | ENERGY | 1.11 | 0.68 | 0.26 | 0.4755240000000001 | 20 bars with volume below 0.7× average |
| waves and optics | Wavelet energy at scale 5 | ENERGY | 0.9 | 0.88 | 0.2 | 0.4752 | Haar wavelet energy of closes at scale 5 relative to total energy |
| geophysics | Erosion | FORCE | 0.9 | 0.84 | 0.25 | 0.47250000000000003 | slow drift of the 50-day mean vs the 250-day mean |
| materials science | Brittleness | STRUCTURE | 0.98 | 0.72 | 0.33 | 0.46922400000000003 | large single-bar moves relative to the median range |
| topology and geometry | Convex hull area | STRUCTURE | 0.89 | 0.87 | 0.21 | 0.46845149999999997 | area of the (bar index, close) hull over 20 bars in ATR·bars |
| materials science | Yield point | STRUCTURE | 0.84 | 0.78 | 0.42 | 0.465192 | level beyond which price moves fast: the 20-bar high on a volume expansion |
| thermodynamics | Free energy | ENERGY | 0.9 | 0.75 | 0.37 | 0.4623750000000001 | trend energy minus noise energy: net 20-bar move squared minus variance of daily changes |
| information theory | Transfer entropy | ENERGY | 0.98 | 0.7 | 0.34 | 0.45962 | information from volume to next-day return sign over 60 bars |
| materials science | Fatigue crack | STRUCTURE | 0.91 | 0.77 | 0.31 | 0.4589585 | repeated tests of a level (touches of the 20-bar low in 20 bars) |
| oceanography | Rip current | ENERGY | 1.03 | 0.68 | 0.28 | 0.44825600000000004 | close near the low on volume above average within an uptrend |
| statistical physics | Avalanche size | STRUCTURE | 0.83 | 0.85 | 0.27 | 0.44799249999999996 | cumulative move of the current same-direction run in ATR |
| chemistry | Catalyst | ENERGY | 1.02 | 0.71 | 0.23 | 0.445383 | volume ratio above 3 with a range above 2 ATR |
| computing | Garbage collection | ENERGY | 0.92 | 0.73 | 0.31 | 0.439898 | bars of volume decline after a surge |
| music and language | Rhythm regularity | STRUCTURE | 0.83 | 0.84 | 0.24 | 0.432264 | coefficient of variation of intervals between swing highs |
| soviet school | Markov chain | STRUCTURE | 0.95 | 0.73 | 0.24 | 0.42997 | 2-state transition matrix of daily direction over 60 bars |
| quantum | Superposition collapse | STRUCTURE | 0.97 | 0.68 | 0.3 | 0.42874000000000007 | bars of a narrow range then a large single-bar move (measurement) |
| computing | Cache hit | STRUCTURE | 0.95 | 0.72 | 0.25 | 0.4275 | share of the last 20 closes within 0.5 ATR of a previous close |
| soviet school | Pontryagin hazard | STRUCTURE | 1.04 | 0.63 | 0.3 | 0.42588000000000004 | reversal hazard as a function of bars in trend and distance from the mean |
| soviet school | Gnedenko extremes | STRUCTURE | 0.79 | 0.86 | 0.25 | 0.42462500000000003 | tail index of the daily-range distribution |
| statistical physics | Susceptibility | ENERGY | 0.73 | 0.79 | 0.47 | 0.4238745 | response of returns to volume shocks: slope of |return| on volume z-score over 60 bars |
| physiology | Adaptation | STRUCTURE | 1.03 | 0.63 | 0.3 | 0.421785 | shrinking response to repeated volume surges over 60 bars |
| statistics | Regression R² | STRUCTURE | 0.95 | 0.71 | 0.25 | 0.4215625 | R² of the 20-bar linear fit of the close |
| astronomy | Black hole | ENERGY | 0.84 | 0.76 | 0.32 | 0.421344 | volume concentrating at one price level for 20 bars with shrinking range |
| waves and optics | Standing wave | STRUCTURE | 0.81 | 0.84 | 0.23 | 0.418446 | price oscillating between two levels for 20 bars (range with 3+ touches each side) |
| soviet school | Tikhonov trend | FORCE | 1.03 | 0.66 | 0.23 | 0.41807700000000003 | ridge-regularised 20-bar slope |
| signal processing | Hilbert instantaneous frequency | FORCE | 0.83 | 0.84 | 0.19 | 0.4148339999999999 | instantaneous cycle frequency of the detrended close |
| mechanics | Inelastic collision | ENERGY | 0.89 | 0.73 | 0.27 | 0.41255949999999997 | volume absorbed at a level: volume on bars touching the 20-bar low divided by average volume |
| waves and optics | Wavelet energy at scale 20 | ENERGY | 0.8 | 0.86 | 0.19 | 0.40936 | Haar wavelet energy at scale 20 relative to total energy |
| physiology | Homeostasis | STRUCTURE | 0.86 | 0.78 | 0.22 | 0.40918800000000005 | bars within ±1 ATR of the 20-day median in the last 20 |
| mechanics | Moment of inertia | ENERGY | 0.74 | 0.85 | 0.3 | 0.40885 | dispersion of volume across price levels in the last 20 bars (second moment of volume-at-price) |
| hydrology | Hurst rescaled range | STRUCTURE | 0.82 | 0.81 | 0.23 | 0.408483 | R/S exponent over 100 bars |
| astronomy | Orbital period | STRUCTURE | 0.76 | 0.87 | 0.23 | 0.406638 | dominant cycle period from autocorrelation over 250 bars |
| soviet school | Stratonovich filter | FORCE | 1.29 | 0.47 | 0.34 | 0.406221 | Kalman filtered level and velocity with innovation |
| stochastic processes | Runs test | STRUCTURE | 0.81 | 0.85 | 0.18 | 0.406215 | z-score of the number of runs in the 30-bar sign sequence |
| computing | Exponential backoff | ENERGY | 0.93 | 0.71 | 0.23 | 0.4060845 | interval between volume surges growing over 60 bars |
| statistics | Bayesian shrinkage slope | STRUCTURE | 1.13 | 0.53 | 0.34 | 0.40126300000000004 | 20-bar slope shrunk toward the 250-bar slope by its noise |
| fluids | Hydraulic jump | STRUCTURE | 0.83 | 0.76 | 0.27 | 0.400558 | sudden range expansion after a fast, low-range move |
| neuroscience | Habituation | ENERGY | 0.9 | 0.69 | 0.29 | 0.400545 | decreasing range response to same-size volume surges |
| reliability engineering | Survival probability | STRUCTURE | 1.03 | 0.61 | 0.26 | 0.395829 | share of historic 10-bar runs that survived to 15 bars |
| meteorology | Wind shear | STRUCTURE | 0.84 | 0.76 | 0.24 | 0.395808 | difference between the 5-bar and 20-bar slopes in ATR/bar |
| electromagnetism | Voltage | ENERGY | 0.86 | 0.74 | 0.24 | 0.394568 | distance to the 20-day high in ATR times the volume ratio |
| genetics and evolution | Selection pressure | ENERGY | 0.81 | 0.73 | 0.33 | 0.3932145000000001 | share of volume on days that closed in the top third of their range over 20 bars |
| materials science | Creep | FORCE | 0.77 | 0.79 | 0.29 | 0.3923535000000001 | slow steady drift on low volume over 20 bars |
| statistical physics | Ergodicity breaking | STRUCTURE | 0.83 | 0.76 | 0.23 | 0.387942 | difference between the time-average and the recent-window average return over 250 bars |
| social dynamics | Opinion polarisation | STRUCTURE | 0.72 | 0.81 | 0.32 | 0.38491200000000003 | share of bars closing in the top or bottom quarter of their range over 20 bars |
| genetics and evolution | Punctuated equilibrium | STRUCTURE | 0.68 | 0.91 | 0.24 | 0.383656 | long flat periods interrupted by jumps: ratio of the largest 5-bar move to the median 5-bar move over 100 bars |
| geophysics | Seismic gap | STRUCTURE | 0.72 | 0.85 | 0.23 | 0.37638 | bars since the last 2-ATR move relative to the historic mean interval |
| geophysics | Foreshocks | STRUCTURE | 0.67 | 0.92 | 0.21 | 0.37292200000000003 | increasing count of 1-ATR moves in the 10 bars before now |
| chemistry | Activation energy | ENERGY | 0.83 | 0.63 | 0.4 | 0.36603 | volume spike needed before a breakout historically vs the current volume ratio |
| thermodynamics | Pressure | ENERGY | 0.91 | 0.65 | 0.23 | 0.3637725 | volume divided by the daily range (volume per unit of price movement) vs its 50-bar mean |
| statistical physics | Mean first-passage time | FORCE | 0.83 | 0.7 | 0.25 | 0.363125 | historic bars needed to travel 2 ATR from the current velocity regime |
| statistical physics | Fluctuation-dissipation | ENERGY | 0.78 | 0.72 | 0.28 | 0.359424 | ratio of the response to shocks to the resting variance over 60 bars |
| neuroscience | Lateral inhibition | STRUCTURE | 0.73 | 0.78 | 0.26 | 0.35872200000000004 | up-days followed by down-days: lag-1 sign anticorrelation over 20 bars |
| genetics and evolution | Fitness landscape | STRUCTURE | 0.76 | 0.76 | 0.24 | 0.358112 | count of local 20-bar highs within the last 100 bars |
| ecology | Succession | STRUCTURE | 0.76 | 0.73 | 0.29 | 0.357846 | time since the last regime change (SuperTrend flip) as a maturity measure |
| stochastic processes | Brownian bridge probability | STRUCTURE | 1.01 | 0.57 | 0.24 | 0.356934 | probability of reaching the 20-bar high before the 20-bar low from the current point |
| ecology | Predation pressure | ENERGY | 0.79 | 0.72 | 0.25 | 0.3555 | down-volume share during up-days over 10 bars |
| mechanics | Friction | ENERGY | 0.67 | 0.84 | 0.26 | 0.354564 | ratio of gross path length to net displacement over 20 bars (energy lost to noise) |
| chemistry | Reaction rate | FORCE | 0.83 | 0.64 | 0.33 | 0.353248 | speed of the move after a catalyst day (a 2-ATR bar): net move over the next 5 bars |
| reliability engineering | Weibull shape | STRUCTURE | 0.73 | 0.79 | 0.22 | 0.35178699999999996 | shape of the distribution of up-run lengths |
| physiology | Fatigue | STRUCTURE | 0.75 | 0.69 | 0.34 | 0.346725 | declining volume on successive up-days in a run |
| information theory | Lempel–Ziv complexity | STRUCTURE | 0.71 | 0.82 | 0.19 | 0.34640899999999997 | compressibility of the 30-bar sign sequence |
| games and sports | Elo rating | FORCE | 0.59 | 0.79 | 0.48 | 0.344914 | rating of the stock's own recent returns vs its long-term returns |
| quantum | Entanglement | STRUCTURE | 0.73 | 0.78 | 0.21 | 0.344487 | co-movement of volume direction and close position beyond chance over 20 bars |
| thermodynamics | Temperature | ENERGY | 0.77 | 0.71 | 0.25 | 0.3416875 | realized daily-range variance over 10 bars relative to 100 bars |
| signal processing | Kotelnikov sampling | STRUCTURE | 0.71 | 0.77 | 0.25 | 0.3416875 | dominant cycle period relative to the 20-bar horizon |
| physiology | Blood pressure | STRUCTURE | 0.64 | 0.82 | 0.3 | 0.34112 | ratio of the 20-bar high-low range to the median daily range |
| neuroscience | Spike train regularity | STRUCTURE | 0.72 | 0.8 | 0.18 | 0.33984 | coefficient of variation of the intervals between 1.5-ATR days |
| geophysics | Liquefaction | ENERGY | 0.68 | 0.81 | 0.23 | 0.33874200000000004 | volume rising while daily ranges shrink over 10 bars |
| topology and geometry | Fractal dimension | STRUCTURE | 0.68 | 0.82 | 0.19 | 0.33177199999999996 | Katz dimension of the 30-bar path |
| information theory | Shannon entropy of signs | STRUCTURE | 0.7 | 0.78 | 0.21 | 0.33032999999999996 | entropy of the up/down sequence over 20 bars |
| statistical physics | Order parameter | STRUCTURE | 0.65 | 0.8 | 0.27 | 0.3302 | coherence of daily direction, volume direction and close position over 10 bars |
| hydrology | Watershed | STRUCTURE | 0.72 | 0.7 | 0.3 | 0.3276 | the 50-bar high as a divide: side of it and distance in ATR |
| signal processing | Cepstrum | STRUCTURE | 0.67 | 0.84 | 0.16 | 0.32642399999999994 | periodicity of the log power spectrum (echoes of a shock) |
| topology and geometry | Curvature | STRUCTURE | 0.68 | 0.8 | 0.2 | 0.3264 | second difference of the 10-bar smoothed close, in ATR |
| astronomy | Luminosity | ENERGY | 0.81 | 0.6 | 0.33 | 0.32319000000000003 | dollar volume relative to the 250-bar median |
| electromagnetism | Skin effect | STRUCTURE | 0.58 | 0.87 | 0.28 | 0.32294399999999995 | intraday extremes (high, low) moving more than closes over 10 bars |
| topology and geometry | Fibonacci time | STRUCTURE | 0.62 | 0.88 | 0.17 | 0.31917599999999996 | bars since the swing low relative to 8, 13, 21, 34 |
| soviet school | Lyapunov exponent | STRUCTURE | 0.69 | 0.79 | 0.17 | 0.3188835 | divergence rate of nearby 5-bar path segments in the last 60 bars |
| quantum | Uncertainty | ENERGY | 0.61 | 0.83 | 0.25 | 0.3164375 | product of the 20-bar price standard deviation and velocity standard deviation |
| waves and optics | Interference | STRUCTURE | 0.8 | 0.67 | 0.18 | 0.31624 | agreement of the 5-, 20- and 60-bar trend slopes (constructive) vs disagreement |
| signal processing | Phase-locked loop | STRUCTURE | 0.77 | 0.67 | 0.22 | 0.314699 | phase error between price and its dominant cycle |
| ecology | Extinction risk | ENERGY | 0.52 | 0.89 | 0.35 | 0.31239000000000006 | consecutive days with volume below 0.5× average |
| electromagnetism | Dipole | STRUCTURE | 0.72 | 0.71 | 0.22 | 0.311832 | difference between the volume above and below the close within the 20-bar volume-at-price |
| medicine | Dose-response | FORCE | 0.89 | 0.53 | 0.32 | 0.31132200000000004 | price response per unit of volume over 20 bars |
| fluids | Laminar flow | FORCE | 0.84 | 0.59 | 0.25 | 0.30974999999999997 | path efficiency above 0.7 with low volume variance over 10 bars |
| meteorology | Dew point | STRUCTURE | 0.6 | 0.85 | 0.21 | 0.30855 | level where selling stops: the 20-bar low as a fraction of the 60-bar range |
| statistical physics | Correlation length | STRUCTURE | 0.58 | 0.8 | 0.31 | 0.30391999999999997 | number of bars over which return autocorrelation stays positive |
| fluids | Viscosity | ENERGY | 0.72 | 0.69 | 0.22 | 0.303048 | volume per ATR of movement over 20 bars relative to 100 bars |
| topology and geometry | Persistent homology | STRUCTURE | 0.65 | 0.79 | 0.17 | 0.30039750000000004 | number of price bands with volume peaks persisting across 20, 40 and 60 bars |
| economics | Auction theory | STRUCTURE | 0.77 | 0.57 | 0.36 | 0.29845199999999994 | close near the high on rising volume (winning bids) |
| medicine | Fever curve | STRUCTURE | 0.61 | 0.8 | 0.22 | 0.29768 | daily range rising for 3 bars then falling |
| games and sports | Fatigue in sport | STRUCTURE | 0.72 | 0.64 | 0.28 | 0.294912 | declining ranges over a run of up-days |
| chemistry | Half-life of a shock | ENERGY | 0.65 | 0.68 | 0.33 | 0.2939300000000001 | bars until the daily range falls back to its pre-shock level |
| quantum | Decoherence time | STRUCTURE | 0.65 | 0.72 | 0.25 | 0.2925 | bars over which return autocorrelation decays to zero |
| oceanography | Rogue wave | STRUCTURE | 0.57 | 0.84 | 0.22 | 0.29206799999999994 | a daily range above 4 ATR within the last 20 bars |
| social dynamics | Contagion | ENERGY | 1.06 | 0.38 | 0.45 | 0.29203 | up-volume share rising over 5 bars |
| thermodynamics | Carnot efficiency | STRUCTURE | 0.5 | 0.84 | 0.36 | 0.28559999999999997 | net move divided by gross intraday range over 20 bars |
| games and sports | Poker pot odds | STRUCTURE | 0.63 | 0.69 | 0.31 | 0.2847285 | reward-to-risk of the bracket given the distance to the 20-bar high and low |
| topology and geometry | Self-similarity | STRUCTURE | 0.68 | 0.68 | 0.23 | 0.2843760000000001 | correlation of the 5-bar and 20-bar path shapes |
| hydrology | Flash flood | ENERGY | 0.6 | 0.76 | 0.24 | 0.28271999999999997 | 2-ATR move on a day with volume above 3× average |
| quantum | Quantum walk spread | FORCE | 0.68 | 0.66 | 0.24 | 0.278256 | ballistic vs diffusive spread: net displacement vs square root of bars over 30 bars |
| quantum | Energy levels | STRUCTURE | 0.58 | 0.76 | 0.25 | 0.27549999999999997 | quantised step sizes: clustering of 20-bar moves around multiples of the ATR |
| music and language | Crescendo | STRUCTURE | 0.8 | 0.56 | 0.22 | 0.27328 | rising ranges and volume over 5 bars |
| fluids | Pressure gradient | FORCE | 0.79 | 0.54 | 0.26 | 0.268758 | difference between the 5-bar and 20-bar volume-weighted mean prices, in ATR |
| control theory | Damping ratio | STRUCTURE | 0.55 | 0.8 | 0.19 | 0.26180000000000003 | ratio of successive overshoots around the 20-day mean |
| waves and optics | Spectral centroid | STRUCTURE | 0.57 | 0.78 | 0.17 | 0.26009099999999996 | volume-weighted centre of the return power spectrum over 64 bars |
| mechanics | Hooke's law | FORCE | 0.7 | 0.58 | 0.28 | 0.25983999999999996 | restoring force: distance from the 20-day mean times the 60-bar reversion speed |
| mechanics | Momentum conservation | FORCE | 1.17 | 0.3 | 0.48 | 0.25973999999999997 | net signed dollar volume over 10 bars vs the price change it produced (impact per momentum unit) |
| music and language | Harmonic ratio | STRUCTURE | 0.55 | 0.81 | 0.15 | 0.2561625 | ratio of the last two swing sizes near 2:1 or 3:2 |
| epidemiology | Epidemic curve | STRUCTURE | 0.58 | 0.68 | 0.29 | 0.254388 | cumulative up-volume over 20 bars fitted to a logistic curve: position on the curve |
| thermodynamics | Boltzmann distribution | STRUCTURE | 0.58 | 0.7 | 0.25 | 0.25375 | share of daily moves larger than 1 ATR vs the exponential expectation |
| medicine | Vital signs | ENERGY | 0.65 | 0.59 | 0.31 | 0.25119250000000004 | range, volume and efficiency each within their normal 250-bar band |
| hydrology | Baseflow | FORCE | 0.59 | 0.69 | 0.23 | 0.2503665 | the slow component of the move: 60-bar slope in ATR/bar |
| statistical physics | Random walk exponent | STRUCTURE | 0.55 | 0.76 | 0.18 | 0.24662 | log of net displacement over log of bars, over 30 bars |
| hydrology | Flood recurrence | STRUCTURE | 0.52 | 0.76 | 0.24 | 0.245024 | historic interval between 3-ATR days vs the current gap |
| mechanics | Escape velocity | FORCE | 0.73 | 0.5 | 0.34 | 0.24455000000000002 | 5-bar velocity relative to the velocity that historically preceded 20-day breakouts |
| genetics and evolution | Mutation rate | STRUCTURE | 0.5 | 0.82 | 0.19 | 0.24394999999999997 | frequency of direction changes over 20 bars |
| topology and geometry | Torsion | STRUCTURE | 0.55 | 0.74 | 0.18 | 0.24013 | change of curvature over 5 bars |
| topology and geometry | Winding number | FORCE | 0.54 | 0.78 | 0.14 | 0.24008400000000005 | net rotations in the (velocity, acceleration) plane over 40 bars |
| soviet school | Chebyshev bound | STRUCTURE | 0.8 | 0.49 | 0.22 | 0.23912 | share of moves beyond k standard deviations vs the 1/k² bound |
| topology and geometry | Symmetry | STRUCTURE | 0.53 | 0.78 | 0.15 | 0.237705 | mirror symmetry of the last two swings (size and duration) |
| meteorology | Front passage | STRUCTURE | 0.64 | 0.6 | 0.23 | 0.23616 | day when the 5-bar slope changes sign with volume above average |
| chemistry | Diffusion coefficient | ENERGY | 0.56 | 0.68 | 0.24 | 0.23609600000000006 | variance growth per bar over 10, 20 and 40 bars (linearity test) |
| reliability engineering | Bathtub hazard | STRUCTURE | 0.59 | 0.62 | 0.29 | 0.23594099999999998 | reversal hazard as a function of trend age from history |
| mechanics | Projectile apex | FORCE | 0.65 | 0.58 | 0.22 | 0.22997 | bars since velocity crossed from positive to negative in an up-leg |
| geophysics | Strain accumulation | ENERGY | 0.48 | 0.76 | 0.22 | 0.222528 | squeeze bars times volatility compression |
| chemistry | Equilibrium constant | STRUCTURE | 0.49 | 0.75 | 0.21 | 0.2223375 | ratio of up-volume to down-volume over 50 bars |
| oceanography | Swell | STRUCTURE | 0.48 | 0.79 | 0.17 | 0.22183199999999997 | long-period cycle (40–60 bars) amplitude relative to ATR |
| chemistry | Titration endpoint | FORCE | 0.54 | 0.69 | 0.17 | 0.21797099999999997 | the bar where cumulative up-volume overtakes cumulative down-volume in the last 20 bars |
| fluids | Capillary action | FORCE | 0.59 | 0.62 | 0.19 | 0.21765099999999996 | slow steady rise on low volume: 10-bar efficiency above 0.6 with volume below 0.8× average |
| games and sports | Home advantage | FORCE | 0.41 | 0.77 | 0.36 | 0.21467599999999998 | share of up-days that opened above the prior close |
| geophysics | Gutenberg–Richter | STRUCTURE | 0.48 | 0.72 | 0.23 | 0.21254399999999998 | log count of days above 1, 2, 3 ATR over 250 bars (b-value) |
| soviet school | Kantorovich transport | STRUCTURE | 0.6 | 0.6 | 0.18 | 0.21239999999999998 | Wasserstein distance between the 20-bar and 60-bar volume-at-price profiles |
| soviet school | Kotelnikov theorem | STRUCTURE | 0.63 | 0.53 | 0.25 | 0.20868750000000003 | dominant cycle sampled adequately by the 20-bar window |
| thermodynamics | Heat diffusion | ENERGY | 0.52 | 0.65 | 0.23 | 0.20787 | spread of volume-at-price around the point of control over 20 bars |
| agriculture | Growing season | STRUCTURE | 0.52 | 0.66 | 0.21 | 0.207636 | bars since the 250-bar low as the age of the crop |
| soviet school | Kolmogorov complexity | STRUCTURE | 0.54 | 0.66 | 0.16 | 0.206712 | compressibility of the recent sign sequence |
| psychology | Disposition effect | ENERGY | 0.7 | 0.44 | 0.33 | 0.20482 | volume on up-days after a 20-bar drawdown vs before |
| mechanics | Terminal velocity | FORCE | 0.6 | 0.55 | 0.23 | 0.20295000000000002 | 5-bar velocity relative to the 60-bar maximum 5-bar velocity |
| waves and optics | Beat frequency | STRUCTURE | 0.47 | 0.76 | 0.13 | 0.20181799999999997 | difference between the two dominant cycle periods found in 100 bars |
| electromagnetism | Magnetic hysteresis | STRUCTURE | 0.43 | 0.8 | 0.17 | 0.20124 | asymmetry of the path up vs the path down through the same levels over the last swing pair |
| genetics and evolution | Genetic drift | STRUCTURE | 0.53 | 0.64 | 0.18 | 0.200128 | random-walk component: net move minus trend component over 20 bars |
| fluids | Reynolds number | FORCE | 0.41 | 0.76 | 0.24 | 0.193192 | velocity × swing size ÷ viscosity (noise): 20-bar velocity × leg size ÷ daily-change std |
| fluids | Tidal cycle | STRUCTURE | 0.45 | 0.73 | 0.17 | 0.1921725 | position within the 20-bar cycle of the dominant period found by autocorrelation |
| statistical physics | Percolation density | STRUCTURE | 0.42 | 0.75 | 0.22 | 0.19215 | fraction of the last 20 bars with volume above the 50-bar median |
| statistics | Cook's distance | STRUCTURE | 0.63 | 0.48 | 0.27 | 0.192024 | influence of the largest bar on the 20-bar slope |
| information theory | Rate distortion | STRUCTURE | 0.59 | 0.5 | 0.28 | 0.1888 | error of a two-level approximation of the 20-bar path |
| hydrology | Evaporation | STRUCTURE | 0.43 | 0.71 | 0.23 | 0.18775949999999997 | volume declining over 10 bars during a flat range |
| thermodynamics | Entropy production | STRUCTURE | 0.42 | 0.73 | 0.21 | 0.185493 | growth of return-sign entropy over the last 20 bars vs the prior 20 |
| electromagnetism | Capacitance | ENERGY | 0.47 | 0.68 | 0.15 | 0.18377 | volume stored during compression, released on expansion (squeeze volume ÷ breakout volume) |
| mechanics | Angular momentum | FORCE | 0.4 | 0.79 | 0.15 | 0.18170000000000003 | rotation in (price, velocity) phase space over 10 bars (signed area swept) |
| materials science | Hardness | ENERGY | 0.57 | 0.54 | 0.18 | 0.181602 | volume needed to move price 1 ATR relative to 250 bars |
| information theory | Channel capacity | ENERGY | 0.4 | 0.73 | 0.24 | 0.18103999999999998 | signal variance over noise variance for the 20-bar trend |
| computing | Compression ratio | STRUCTURE | 0.48 | 0.65 | 0.16 | 0.18095999999999998 | gzip-like compressibility of the 60-bar sign sequence |
| genetics and evolution | Convergent evolution | STRUCTURE | 0.61 | 0.49 | 0.21 | 0.18083449999999998 | agreement of three independent trend measures (slope, efficiency, structure) |
| music and language | Tempo | STRUCTURE | 0.58 | 0.53 | 0.17 | 0.179829 | swings per 20 bars |
| statistical physics | Ising magnetisation | STRUCTURE | 0.36 | 0.8 | 0.21 | 0.17423999999999998 | fraction of up-days in the last 20 minus 0.5, times 2 |
| thermodynamics | Heat capacity | ENERGY | 0.49 | 0.59 | 0.19 | 0.1720145 | volume needed to move price one ATR over 20 bars |
| neuroscience | Synaptic plasticity | FORCE | 0.35 | 0.79 | 0.23 | 0.1700475 | strengthening of the price response to volume over 60 bars (slope change) |
| astronomy | Gravitational lensing | STRUCTURE | 0.39 | 0.73 | 0.19 | 0.1693965 | distortion of the path around a high-volume level: path length within 1 ATR of it |
| structural engineering | Resonance risk | STRUCTURE | 0.48 | 0.61 | 0.15 | 0.16835999999999998 | swing amplitude growth over 3 swings with a stable period |
| geophysics | Plate boundary | STRUCTURE | 0.48 | 0.55 | 0.24 | 0.16368000000000002 | distance to the nearest high-volume price level in ATR |
| thermodynamics | Thermal equilibrium | ENERGY | 0.49 | 0.55 | 0.19 | 0.1603525 | bars since the 5-bar variance and 50-bar variance last matched |
| computing | Hash collision | STRUCTURE | 0.59 | 0.47 | 0.15 | 0.1594475 | closes repeating the same 0.5 % bin within 20 bars |
| waves and optics | Diffraction | STRUCTURE | 0.45 | 0.58 | 0.22 | 0.15921 | spread of price after passing a narrow squeeze |
| neuroscience | Action potential threshold | ENERGY | 0.39 | 0.7 | 0.16 | 0.15833999999999998 | volume z-score above 2 as the firing threshold, count in 20 bars |
| fluids | Vortex shedding | STRUCTURE | 0.39 | 0.7 | 0.16 | 0.15833999999999998 | alternation of up and down closes with regular period over 20 bars |
| ecology | Niche width | STRUCTURE | 0.36 | 0.73 | 0.2 | 0.15768 | number of distinct 0.5 % price bins visited in 20 bars |
| fluids | Wave breaking | STRUCTURE | 0.37 | 0.7 | 0.21 | 0.156695 | swing amplitude growing while period shortens over the last 3 swings |
| epidemiology | Incubation period | STRUCTURE | 0.36 | 0.68 | 0.25 | 0.15300000000000002 | bars between a volume surge and the subsequent price move |
| materials science | Stress-strain | ENERGY | 0.45 | 0.55 | 0.23 | 0.1522125 | volume applied vs price displacement over 10 bars |
| soviet school | Gelfand pairs | STRUCTURE | 0.54 | 0.46 | 0.22 | 0.15152400000000002 | agreement of two symmetric measures (up-leg and down-leg structure) |
| mechanics | Resonance | STRUCTURE | 0.41 | 0.62 | 0.18 | 0.14997799999999997 | ratio of swing amplitude to the amplitude of the swing before, when periods match |
| ecology | Carrying capacity | STRUCTURE | 0.42 | 0.58 | 0.2 | 0.14615999999999998 | distance to the 250-bar high as a saturation level |
| agriculture | Crop rotation | STRUCTURE | 0.36 | 0.68 | 0.19 | 0.145656 | alternation of up and down 5-bar periods |
| waves and optics | Doppler shift | STRUCTURE | 0.33 | 0.73 | 0.2 | 0.14454 | shortening of swing periods as price accelerates |
| fluids | Bernoulli | FORCE | 0.34 | 0.69 | 0.23 | 0.144279 | fast flow means low pressure: high velocity with volume below average over 5 bars |
| geophysics | Sediment layers | STRUCTURE | 0.32 | 0.77 | 0.17 | 0.144144 | number of distinct volume-at-price peaks in 60 bars |
| epidemiology | Case fatality | STRUCTURE | 0.35 | 0.63 | 0.29 | 0.14222249999999997 | share of breakouts in the last 100 bars that reversed within 5 bars |
| music and language | Zipf law | STRUCTURE | 0.36 | 0.64 | 0.23 | 0.141696 | rank-size slope of the 60-bar daily moves |
| geophysics | Omori aftershock decay | STRUCTURE | 0.33 | 0.72 | 0.19 | 0.141372 | fitted decay exponent of daily range after a 3-ATR shock |
| thermodynamics | Adiabatic compression | STRUCTURE | 0.35 | 0.64 | 0.21 | 0.13551999999999997 | range contraction with rising volume over 10 bars |
| statistical physics | Self-organised criticality | STRUCTURE | 0.32 | 0.7 | 0.19 | 0.13327999999999998 | count of moves above 1.5 ATR in 20 bars vs the power-law expectation |
| neuroscience | Integrate-and-fire | FORCE | 0.33 | 0.69 | 0.16 | 0.132066 | cumulative signed volume since the last 2-ATR move relative to its historic firing level |
| epidemiology | Herd immunity threshold | STRUCTURE | 0.34 | 0.62 | 0.25 | 0.13175 | share of the 60-bar range above which up-moves stall historically |
| oceanography | Thermocline | STRUCTURE | 0.38 | 0.58 | 0.18 | 0.13003599999999998 | the level separating high-volume and low-volume price bands in the 60-bar profile |
| statistical physics | Spin-glass frustration | STRUCTURE | 0.3 | 0.7 | 0.21 | 0.12705 | fraction of bars where close position and volume direction disagree over 20 bars |
| epidemiology | Basic reproduction number | STRUCTURE | 0.26 | 0.77 | 0.25 | 0.12512500000000001 | up-days in the last 5 over up-days in the 5 before |
| physiology | Heart-rate variability | ENERGY | 0.28 | 0.74 | 0.19 | 0.123284 | coefficient of variation of daily ranges over 20 bars |
| mechanics | Pendulum period | STRUCTURE | 0.27 | 0.8 | 0.14 | 0.12312000000000002 | average bars between successive swing highs over the last 5 swings |
| military and operations research | Lanchester attrition | ENERGY | 0.3 | 0.7 | 0.16 | 0.12179999999999999 | up-volume squared minus down-volume squared over 10 bars |
| structural engineering | Buckling | FORCE | 0.41 | 0.48 | 0.19 | 0.11709599999999998 | acceleration turning negative at high velocity |
| thermodynamics | Boiling point | STRUCTURE | 0.37 | 0.5 | 0.19 | 0.11007499999999999 | bars of compression before a range expansion of 2× (historic distribution vs the current count) |
| ecology | Allee effect | ENERGY | 0.25 | 0.74 | 0.19 | 0.11007499999999999 | dollar volume below a critical threshold relative to its 250-bar median |
| electromagnetism | Faraday induction | FORCE | 0.29 | 0.66 | 0.14 | 0.109098 | rate of change of the volume profile centre over 10 bars |
| epidemiology | Contact rate | ENERGY | 0.33 | 0.52 | 0.25 | 0.10725 | volume relative to average as the mixing rate |
| ecology | Lotka–Volterra cycle | STRUCTURE | 0.27 | 0.67 | 0.17 | 0.10582650000000002 | phase of the up-volume/down-volume cycle over 40 bars |
| astronomy | Redshift | STRUCTURE | 0.3 | 0.56 | 0.17 | 0.09828 | lengthening of swing periods over the last 5 swings |
| soviet school | Landau order parameter | STRUCTURE | 0.29 | 0.56 | 0.19 | 0.096628 | magnetisation-like coherence of direction, volume and range over 10 bars |
| soviet school | Dubinin adsorption | STRUCTURE | 0.32 | 0.51 | 0.18 | 0.096288 | volume absorbed at a level as a function of distance to it |
| ecology | Island biogeography | STRUCTURE | 0.23 | 0.68 | 0.2 | 0.09384 | range size vs the number of touches of its edges over 20 bars |
| electromagnetism | Inductance | FORCE | 0.27 | 0.55 | 0.25 | 0.09281250000000002 | resistance to change of velocity: lag between volume surge and price response |
| mechanics | Drag coefficient | FORCE | 0.31 | 0.49 | 0.21 | 0.0918995 | deceleration of velocity as a function of velocity squared over 20 bars |
| structural engineering | Natural frequency | STRUCTURE | 0.2 | 0.78 | 0.15 | 0.08970000000000002 | dominant cycle of the detrended close |
| astronomy | Perihelion | STRUCTURE | 0.23 | 0.61 | 0.21 | 0.0848815 | closeness of the close to the cycle low |
| fluids | Turbulence cascade | ENERGY | 0.19 | 0.75 | 0.17 | 0.0833625 | ratio of variance at 1-, 5-, 20-day scales (deviation from the 5/3 law) |
| mechanics | Newton's second law | FORCE | 0.23 | 0.59 | 0.18 | 0.08006299999999998 | force = mass × acceleration: dollar volume × the change in 5-bar velocity, in ATR/bar² |
| soviet school | Arnold tongues | STRUCTURE | 0.21 | 0.53 | 0.13 | 0.0628845 | locking of the swing period to a multiple of 5 bars |
| thermodynamics | Latent heat | ENERGY | 0.33 | 0.32 | 0.17 | 0.061776000000000005 | volume absorbed without price movement during the last 5 bars (volume × (1 − efficiency)) |
| meteorology | Humidity | ENERGY | 0.26 | 0.35 | 0.12 | 0.050960000000000005 | volume relative to the 50-bar median as moisture available for a move |
| meteorology | Jet stream | FORCE | 0.24 | 0.34 | 0.15 | 0.046919999999999996 | 60-bar slope strength when the 60-bar efficiency is above 0.5 |
