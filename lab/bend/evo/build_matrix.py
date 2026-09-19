"""Build the full matrix in one parallel pass: the engine's random-entry labels (bend/data/<tag>.csv from trades.py) +
every block family at those bars, computed per ticker across all cores. Adds market cap from mine/caps.csv.
usage: python3.12 bend/evo/build_matrix.py --trades trades_D_full --out matrix_D_full [--tf D|W] [--procs 6]"""
from __future__ import annotations

import argparse
import sys
import time
from multiprocessing import Pool
from pathlib import Path

import numpy as np
import pandas as pd

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT)); sys.path.insert(0, str(HERE)); sys.path.insert(0, str(ROOT / "bend"))


def one(args):
    t, bars, tf = args
    import warnings; warnings.filterwarnings("ignore")
    from blocks import blocks; from geometry import geometry; from invented import invented; from scouted import scouted; from longlist import longlist; from pro import pro
    from pack import weekly
    try:
        df = pd.read_csv(ROOT / f"mine/cache/{t}.csv", parse_dates=["date"])
        if tf == "W":
            df = weekly(df)
        if len(df) < 300 or len(df) <= max(bars):
            return None
        B = pd.concat([blocks(df), geometry(df), invented(df), scouted(df), longlist(df), pro(df)], axis=1).iloc[bars]
        B["ticker"] = t; B["bar"] = bars
        return B
    except Exception as e:  # noqa: BLE001
        return f"{t}: {e}"


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--trades", required=True); ap.add_argument("--out", required=True); ap.add_argument("--tf", default="D"); ap.add_argument("--procs", type=int, default=6)
    a = ap.parse_args()
    T = pd.read_csv(ROOT / f"bend/data/{a.trades}.csv", parse_dates=["date"])
    R = T[T.control].copy(); R["year"] = R.date.dt.year; R["band"] = pd.cut(R.atr_pct, [0, 3, 5, 8, 12, 1000], labels=["<3", "3-5", "5-8", "8-12", "≥12"])
    for c in ["flip", "r24", "s53", "t10", "mfe", "mae"]:
        R[f"x_{c}"] = R[c] - R.groupby(["year", "band"], observed=True)[c].transform("mean")
    f = R["flags"].values.astype(np.int64)
    for k, b in {"hole_up": 0, "hole_dn": 1, "whales50": 2, "te_up": 3, "mtn3": 4, "gold": 5, "hot": 6, "ss": 7, "ribbon": 8, "poc": 9, "macdrsi": 10, "fresh": 11}.items():
        R[k] = (f >> b) & 1
    R["hole_inforce"] = 1 - R.hole_up - R.hole_dn; R["panels6"] = (R.panels >= 6).astype(int); R["whales70"] = (R.whales >= 70).astype(int); R["whales_lt30"] = (R.whales < 30).astype(int)
    keep = ["ticker", "bar", "date", "year", "band", "atr_pct", "r24", "r34", "s53", "s54", "t10", "flip", "mfe", "mae", "x_r24", "x_t10", "x_flip", "hole_up", "hole_dn", "whales50", "te_up", "mtn3", "gold", "hot", "ss", "ribbon", "poc", "macdrsi", "fresh", "panels6", "whales70", "whales_lt30", "hole_inforce"]
    R = R[[c for c in keep if c in R]]
    caps = pd.read_csv(ROOT / "mine/caps.csv").set_index("symbol")["cap"]; R["cap"] = R.ticker.map(caps)
    jobs = [(t, g.bar.values, a.tf) for t, g in R.groupby("ticker")]
    t0 = time.time(); parts = []; errs = 0
    with Pool(a.procs) as pool:
        for i, res in enumerate(pool.imap_unordered(one, jobs, chunksize=4)):
            if isinstance(res, pd.DataFrame):
                parts.append(res)
            elif res is not None:
                errs += 1
            if i % 400 == 0:
                print(i, len(jobs), f"{time.time()-t0:.0f}s", flush=True)
    M = R.merge(pd.concat(parts), on=["ticker", "bar"])
    M.to_pickle(ROOT / f"bend/data/{a.out}.pkl")
    print(M.shape, f"{errs} ticker errors, {time.time()-t0:.0f}s; caps known for {M.cap.notna().mean():.0%} of rows")


if __name__ == "__main__":
    main()
