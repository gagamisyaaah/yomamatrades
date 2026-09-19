"""Weekly matrix: the engine's random-entry labels on weekly bars (bend/data/trades_W.csv) + every block family computed on
weekly bars at those samples → bend/data/matrix_W.pkl (same columns as the daily matrix)."""
import sys, time, numpy as np, pandas as pd
sys.path.insert(0, "."); sys.path.insert(0, "bend/evo")
from blocks import blocks
from geometry import geometry
from invented import invented
from scouted import scouted
from longlist import longlist
from pro import pro
sys.path.insert(0, "bend"); from pack import weekly  # noqa: E402

T = pd.read_csv("bend/data/trades_W.csv", parse_dates=["date"])
R = T[T.control].copy()
R["year"] = R.date.dt.year; R["band"] = pd.cut(R.atr_pct, [0, 3, 5, 8, 12, 1000], labels=["<3", "3-5", "5-8", "8-12", "≥12"])
for c in ["flip", "r24", "s53", "t10", "mfe", "mae"]:
    R[f"x_{c}"] = R[c] - R.groupby(["year", "band"], observed=True)[c].transform("mean")
f = R["flags"].values.astype(np.int64)
for k, b in {"hole_up": 0, "hole_dn": 1, "whales50": 2, "te_up": 3, "mtn3": 4, "gold": 5, "hot": 6, "ss": 7, "ribbon": 8, "poc": 9, "macdrsi": 10, "fresh": 11}.items():
    R[k] = (f >> b) & 1
R["hole_inforce"] = 1 - R.hole_up - R.hole_dn; R["panels6"] = (R.panels >= 6).astype(int); R["whales70"] = (R.whales >= 70).astype(int); R["whales_lt30"] = (R.whales < 30).astype(int)
R = R[["ticker", "bar", "date", "year", "band", "atr_pct", "r24", "r34", "s53", "t10", "flip", "mfe", "mae", "x_r24", "x_t10", "x_flip"] + [c for c in R.columns if c in {"hole_up", "hole_dn", "whales50", "te_up", "mtn3", "gold", "hot", "ss", "ribbon", "poc", "macdrsi", "fresh", "panels6", "whales70", "whales_lt30", "hole_inforce"}]]
t0 = time.time(); parts = []
for i, (t, g) in enumerate(R.groupby("ticker")):
    try:
        df = weekly(pd.read_csv(f"mine/cache/{t}.csv", parse_dates=["date"]))
    except Exception:
        continue
    if len(df) <= g.bar.max() or len(df) < 300:
        continue
    B = pd.concat([blocks(df), geometry(df), invented(df), scouted(df), longlist(df), pro(df)], axis=1).iloc[g.bar.values]
    B["ticker"] = t; B["bar"] = g.bar.values; parts.append(B)
    if i % 300 == 0:
        print(i, f"{time.time()-t0:.0f}s", flush=True)
M = R.merge(pd.concat(parts), on=["ticker", "bar"]); M.to_pickle("bend/data/matrix_W.pkl"); print(M.shape, f"{time.time()-t0:.0f}s")
