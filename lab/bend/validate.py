"""Compare the Bend engine's per-bar flags with the Python ports on a few tickers.
usage: python3.12 bend/validate.py [--dir bend/data/T8] [--tickers MRNA ...] [--run]"""
import argparse, struct, subprocess, sys, time
from pathlib import Path
import numpy as np, pandas as pd
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from mine import data, dragon, diamond, ta
ap = argparse.ArgumentParser(); ap.add_argument("--dir", default="bend/data/T8"); ap.add_argument("--tickers", nargs="*"); ap.add_argument("--run", action="store_true"); ap.add_argument("--skip", type=int, default=200)
a = ap.parse_args()
d = Path(a.dir)
if a.run:
    t0 = time.time(); r = subprocess.run(["./bend/dragon", str(d), "--threads", "8"], capture_output=True, text=True); print(r.stdout.strip(), r.stderr.strip()[:300], f"({time.time()-t0:.1f} s wall)")
order = [l.split()[0] for l in (d / "list.txt").read_text().split("\n") if l.strip()]
raw = open(d / "states.bin", "rb").read(); w = struct.unpack(f"<{len(raw)//4}I", raw)
BITS = {"hole_up": 1, "hole_dn": 2, "whales50": 4, "te": 8, "mtn3": 16, "gold": 32, "hot": 64, "ss": 128, "ribbon": 256, "poc": 512, "macdrsi": 1024, "fresh": 2048}
print(f"{'ticker':6s} {'bars':>5s} " + " ".join(f"{k:>8s}" for k in BITS) + f" {'whales':>7s} {'npan':>5s} {'atr%':>6s}")
pos = 0; S = a.skip; worst = {}
for t in order:
    n = w[pos]; flags = np.array(w[pos+1:pos+1+2*n:2]); atrp = np.array(w[pos+2:pos+2+2*n:2]); pos += 1 + 2*n
    m = w[pos]; pos += 1 + 16 * m  # skip the trade records
    if a.tickers and t not in a.tickers: continue
    df = data.fetch(t).reset_index(drop=True); dr = dragon.states(df); di = diamond.states(df); f = flags
    py = {"hole_up": (dr.hole_state == 1).values, "hole_dn": (dr.hole_state == -1).values, "whales50": (dr.whales >= 50).fillna(False).values,
          "te": dr.te_bull.fillna(False).values, "mtn3": (di.mountain >= 3).values, "gold": di.above_gold.values, "hot": di.overheated.fillna(False).values,
          "ss": dr.ss_confirm.fillna(False).values, "ribbon": dr.ribbon_bull.fillna(False).values, "poc": dr.poc_bull.fillna(False).values,
          "macdrsi": (dr.macd_bull & dr.rsi_up).fillna(False).values, "fresh": ((dr.hole_state == 1) & (dr.hole_state.shift(3) != 1)).values}
    agree = {k: np.mean(((f & b) > 0)[S:] == py[k][S:]) for k, b in BITS.items()}
    wh = (f >> 16) & 255; npan = (f >> 24) & 15
    wh_err = np.nanmean(np.abs(wh[S:] - dr.whales.values[S:])); npan_agree = np.mean(npan[S:] == dr.n_bull.values[S:])
    atr_err = np.nanmean(np.abs(atrp[S:]/1000.0 - (ta.atr(df,14)/df.close*100).values[S:]))
    print(f"{t:6s} {n:5d} " + " ".join(f"{agree[k]:8.3f}" for k in BITS) + f" {wh_err:7.2f} {npan_agree:5.2f} {atr_err:6.3f}")
    for k in BITS: worst[k] = min(worst.get(k, 1.0), agree[k])
print("worst  " + " "*6 + " ".join(f"{worst[k]:8.3f}" for k in BITS))
