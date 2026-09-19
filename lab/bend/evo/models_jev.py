"""Jev ranks real-world models from other fields as sources of trading building blocks (typed judgments; the matrix decides
what actually works). Output bend/data/models_jev.md/.csv."""
import json, os, sys, time
sys.path.insert(0, ".")
for ln in open(".env"):
    if "=" in ln and not ln.startswith("#"):
        k, v = ln.strip().split("=", 1); os.environ.setdefault(k, v.strip('"'))
from typesafe_judge import ask
MODELS = {
 # physics
 "Langevin dynamics / OU process": "price as a particle with friction: drift toward a mean plus noise; measurable as reversion speed and noise temperature",
 "Ising model / spin glass": "traders as spins that align with neighbours: measurable as the fraction of aligned days and susceptibility (response to small news)",
 "Percolation threshold": "buying spreads through a network once density passes a threshold: measurable as up-day density crossing a critical level",
 "Hawkes self-exciting process": "large moves raise the probability of further large moves: measurable as decaying intensity of |return| shocks",
 "Kuramoto synchronisation": "oscillators lock phase: measurable as phase coherence between price and volume cycles",
 "Turbulence energy cascade (Kolmogorov 5/3)": "energy flows from large to small scales: measurable as the ratio of variance at 20-day vs 5-day vs 1-day scales",
 "Lévy flights / heavy tails": "moves are mostly small with rare jumps: measurable as kurtosis and jump count of recent returns",
 "Criticality / power-law avalanches": "systems near a critical point show scale-free bursts: measurable as the slope of the range distribution",
 "Brownian bridge / first passage": "probability of hitting a level before a time: measurable as the ATR-scaled distance to the 20-day high and low over remaining bars",
 "Damped harmonic oscillator": "overshoot and decay after a shock: measurable as the decay ratio of successive swing amplitudes",
 "Thermodynamic entropy of returns": "disorder of the recent path: measurable as Shannon entropy of the sign sequence",
 "Quantum harmonic oscillator ground state": "position spread is minimal at the ground state: measurable as price variance relative to the minimum implied by ATR",
 "Quantum tunnelling probability": "a barrier can be crossed with small probability: measurable as closes beyond a resistance with low volume",
 "Uncertainty relation price × velocity": "spread in level × spread in velocity has a floor: measurable as the product of price std and velocity std",
 "Quantum walk / random walk with memory": "steps depend on internal state: measurable as the autocorrelation structure of returns at several lags",
 # biology
 "Lotka–Volterra predator–prey": "buyers and sellers as populations: measurable as up-volume/down-volume ratio and its cyclic drift",
 "SIR epidemic": "an idea infects participants then burns out: measurable as the reproduction number of up-days",
 "Replicator dynamics (evolutionary game)": "strategies grow with fitness: measurable as the share of volume on winning days",
 "Allee effect": "populations below a threshold collapse: measurable as liquidity falling below a critical dollar volume",
 "Immune memory": "a repeated shock gets a faster response: measurable as reaction size to the second shock vs the first",
 "Logistic growth / carrying capacity": "growth saturates: measurable as the flattening of the trend slope",
 "Chemotaxis": "cells move up a gradient: measurable as volume flowing toward the stronger price direction",
 "Heart-rate variability": "healthy systems are variable: measurable as the coefficient of variation of daily ranges",
 "Circadian / seasonal rhythm": "biological clocks: measurable as day-of-week and turn-of-month effects",
 # information / signal
 "Transfer entropy volume → price": "information flow from one series to another: measurable as lagged mutual information of volume and returns",
 "Kolmogorov complexity / compressibility": "how compressible the sign sequence is: measurable as the LZ complexity of recent up/down sequence",
 "Kotelnikov / Nyquist sampling": "a signal must be sampled at twice its frequency: measurable as the dominant cycle period vs the horizon",
 "Spectral flatness": "noise is flat, tone is peaked: measurable as the flatness of the return spectrum",
 # soviet school
 "Kolmogorov–Wiener optimal filtering": "the best linear predictor of the next value: measurable as the Wiener-filtered slope and its innovation",
 "Kalman filter (Stratonovich)": "state estimate with uncertainty: measurable as the filtered velocity and the innovation z-score",
 "Lyapunov exponent": "sensitivity to initial conditions: measurable as divergence rate of nearby path segments",
 "Pontryagin maximum principle": "optimal control: measurable as the distance from the optimal exit given the hazard of reversal",
 "Kantorovich optimal transport": "cost to move one distribution to another: measurable as the Wasserstein distance between the recent and older volume-at-price profiles",
 "Gnedenko extreme value theory": "maxima follow a stable law: measurable as the expected 20-day max excursion given the recent tail index",
 "Tikhonov regularisation": "smooth the ill-posed fit: measurable as the regularised trend slope's stability",
 "Markov chain of states": "next state depends on the current: measurable as transition probabilities up→up, down→down over 60 bars",
 "Zeldovich pancake / gravitational collapse": "matter collapses into sheets: measurable as volume concentrating into a narrow price band",
 # geophysics / other
 "Omori aftershock law": "aftershocks decay as 1/t: measurable as the volatility decay curve after a shock",
 "Gutenberg–Richter magnitude law": "frequency vs size follows a power law: measurable as the count of ≥1, ≥2, ≥3 ATR days",
 "Little's law (queueing)": "items in system = arrival rate × time: measurable as volume ÷ turnover speed",
 "PID control error": "proportional-integral-derivative of the deviation from a target: measurable as P, I, D of the distance to the 50-day mean",
 "Hurst reservoir (hydrology)": "long memory in flows: measurable as the rescaled range exponent",
 "Metcalfe / network effects": "value scales with connections squared: measurable as dollar volume growth vs price growth",
}
rows = []
for name, desc in MODELS.items():
    try:
        a = ask({"model": name, "how_it_would_be_measured": desc, "setting": "daily bars, one symbol's OHLCV, thousands of US stocks, entries scored by a +2/−4 ATR bracket over 20 bars against random entries"},
                {"useful": {"type": "score", "instructions": "How useful is this model as a source of a repeatable entry-timing measurement in this setting?", "criteria": ["decorative analogy", "weak", "useful", "a known quant workhorse"]},
                 "portable": {"type": "noul", "instructions": "Can the described measurement be computed from one symbol's OHLCV history alone?"},
                 "family": {"type": "choice", "instructions": "Which family does the measurement belong to?", "criteria": {"STRUCTURE": "shape, levels, swings", "FORCE": "momentum, flow, velocity", "ENERGY": "volatility, volume regime, liquidity"}},
                 "documented": {"type": "noul", "instructions": "Is there published quantitative-finance evidence that measurements of this kind carry information about future returns?"}})
        rows.append({"model": name, "useful": a["useful"].get("score"), "portable": round(a["portable"].get("noul", 0), 2), "family": a["family"].get("choice"), "documented": round(a["documented"].get("noul", 0), 2)})
        print(rows[-1], flush=True)
    except Exception as e:
        print("ERR", name, str(e)[:80], flush=True)
    time.sleep(0.2)
import pandas as pd
D = pd.DataFrame(rows); D["useful_n"] = pd.to_numeric(D.useful, errors="coerce"); D["rank"] = D.useful_n.fillna(0) * D.portable * (0.5 + 0.5 * D.documented)
D = D.sort_values("rank", ascending=False); D.to_csv("bend/data/models_jev.csv", index=False)
md = lambda d: "| " + " | ".join(map(str, d.columns)) + " |\n|" + "---|" * len(d.columns) + "\n" + "\n".join("| " + " | ".join(str(v) for v in r) + " |" for r in d.itertuples(index=False))  # noqa: E731
open("bend/data/models_jev.md", "w").write("# Jev's ranking of cross-field models as block sources\n\n" + md(D[["model", "family", "useful", "portable", "documented", "rank"]]) + "\n")
print(D[["model", "family", "useful", "portable", "documented", "rank"]].to_string(index=False))
