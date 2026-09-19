"""Decode the engine's combined output (per ticker: n, 2n state words, m, 16m trade words) → DataFrames; validate against a
Python replica of the same rules; aggregate.
usage: python3.12 bend/trades.py --dir bend/data/T8 [--run] [--validate] [--stats] [--split 2024-01-01] [--out name]"""
import argparse, struct, subprocess, sys, time
from pathlib import Path
import numpy as np, pandas as pd
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from mine import data, dragon, diamond, ta
from mine.frontier import outcome

COLS = ["bar", "date", "flags", "atrw", "entw", "flip_bars", "flip", "r24", "r34", "s53", "s54", "mfe", "mae", "t10", "exit_bar", "res"]
ENC = {"flip", "r24", "r34", "s53", "s54", "mfe", "mae", "t10"}


def decode(d: Path):
    order = [l.split()[0] for l in (d / "list.txt").read_text().split("\n") if l.strip()]
    raw = (d / "states.bin").read_bytes(); w = np.frombuffer(raw, dtype="<u4")
    pos = 0; states = {}; trades = []
    for t in order:
        n = int(w[pos]); st = w[pos + 1:pos + 1 + 2 * n].reshape(n, 2); pos += 1 + 2 * n
        m = int(w[pos]); tr = w[pos + 1:pos + 1 + 16 * m].reshape(m, 16); pos += 1 + 16 * m
        states[t] = st
        if m:
            df = pd.DataFrame(tr.astype(np.int64), columns=COLS); df.insert(0, "ticker", t); trades.append(df)
    assert pos == len(w), (pos, len(w))
    tr = pd.concat(trades, ignore_index=True) if trades else pd.DataFrame(columns=["ticker"] + COLS)
    for c in ENC:
        tr[c] = tr[c].astype(np.int64) / 100.0 - 10000.0
    tr["control"] = (tr["flags"] & (1 << 30)) > 0
    tr["fresh"] = (tr["flags"] & 2048) > 0
    tr["panels"] = (tr["flags"].values >> 24) & 15
    tr["whales"] = (tr["flags"].values >> 16) & 255
    tr["atr_pct"] = tr["atrw"] / 1000.0
    tr["entry"] = tr["entw"] / 10000.0
    tr["date"] = pd.to_datetime(tr["date"].astype(str), format="%Y%m%d")
    return states, tr


def replica(t: str):
    df = data.fetch(t).reset_index(drop=True); dr = dragon.states(df); di = diamond.states(df)
    C = ((dr.hole_state == 1) & (dr.whales >= 50) & dr.te_bull & ~di.overheated & (di.mountain >= 3) & di.above_gold).fillna(False).values
    rising = C & ~np.roll(C, 1); rising[0] = False
    o, h, l, c = df.open.values, df.high.values, df.low.values, df.close.values; n = len(c)
    atrp = (ta.atr(df, 14) / df.close * 100).values; hole = dr.hole_state.values
    rows = []
    for i in np.where(rising)[0]:
        if i + 2 >= n: continue
        ent = o[i + 1]; atr = ent * atrp[i] / 100.0
        j = i + 1
        while j < n - 1 and hole[j] != -1: j += 1
        k20 = min(i + 20, n - 1)
        rows.append({"ticker": t, "bar": i, "flip": (c[j] / ent - 1) * 100, "flip_bars": j - i, "r24": outcome(h, l, o, c, i, ent, atr, 2, 4, 20), "r34": outcome(h, l, o, c, i, ent, atr, 3, 4, 20),
                     "s53": outcome(h, l, o, c, i, ent, atr, 0.5, 3, 10), "s54": outcome(h, l, o, c, i, ent, atr, 0.5, 4, 20), "mfe": (h[i + 1:k20 + 1].max() - ent) / atr, "mae": (l[i + 1:k20 + 1].min() - ent) / atr,
                     "t10": (c[min(i + 10, n - 1)] / ent - 1) * 100})
    return pd.DataFrame(rows)


def stats(g: pd.DataFrame) -> dict:
    r = g["flip"]; wins = r[r > 0].sum(); losses = -r[r <= 0].sum()
    return {"n": len(g), "avg%": round(r.mean(), 2), "med%": round(r.median(), 2), "win": round((r > 0).mean(), 3), "PF": round(wins / losses, 2) if losses else float("inf"),
            "bars": round(g["flip_bars"].mean(), 1), "r24": round(g["r24"].mean(), 3), "r24win": round((g["r24"] > 0).mean(), 3), "r34": round(g["r34"].mean(), 3),
            "s53": round(g["s53"].mean(), 3), "s53win": round((g["s53"] > 0).mean(), 3), "t10%": round(g["t10"].mean(), 2), "mfe": round(g["mfe"].mean(), 2), "mae": round(g["mae"].mean(), 2)}


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--dir", default="bend/data/T8"); ap.add_argument("--run", action="store_true"); ap.add_argument("--validate", action="store_true")
    ap.add_argument("--stats", action="store_true"); ap.add_argument("--split", default="2024-01-01"); ap.add_argument("--out", default=None)
    a = ap.parse_args(); d = Path(a.dir)
    if a.run:
        t0 = time.time(); r = subprocess.run(["./bend/dragon", str(d), "--threads", "8"], capture_output=True, text=True); print(r.stdout.strip(), r.stderr.strip()[:300], f"({time.time()-t0:.1f} s wall)")
    states, tr = decode(d)
    print(f"{len(states)} tickers, {len(tr)} trade records ({(~tr.control).sum()} C, {tr.control.sum()} control)")
    if a.validate:
        for t in states:
            py = replica(t); b = tr[(tr.ticker == t) & ~tr.control]
            both = py.merge(b, on=["ticker", "bar"], suffixes=("_py", "_b"))
            errs = {c: float(np.nanmax(np.abs(both[c + "_py"] - both[c + "_b"]))) if len(both) else float("nan") for c in ["flip", "r24", "r34", "s53", "s54", "mfe", "mae", "t10"]}
            fb = int((both["flip_bars_py"] != both["flip_bars_b"]).sum()) if len(both) else 0
            print(f"{t:5s} py {len(py):3d} bend {len(b):3d} matched {len(both):3d}  flip_bars mismatches {fb}  max|Δ| " + " ".join(f"{c}={v:.3f}" for c, v in errs.items()))
    if a.stats:
        tr["year"] = tr.date.dt.year
        spy = data.fetch("SPY").set_index("date")["close"]; reg = (spy > spy.rolling(200).mean())
        tr["regime"] = tr.date.map(reg).fillna(True).astype(bool)
        oos = tr[tr.date >= a.split]
        rows = []
        for name, g in [("C, all", oos[~oos.control]), ("random control, same exits", oos[oos.control]), ("C + guard (SPY>200d at signal)", oos[~oos.control & oos.regime]),
                        ("D: C + fresh", oos[~oos.control & oos.fresh]), ("E: D + ≥6 panels", oos[~oos.control & oos.fresh & (oos.panels >= 6)]), ("F: C + ATR≥5%", oos[~oos.control & (oos.atr_pct >= 5)]),
                        ("F + guard", oos[~oos.control & (oos.atr_pct >= 5) & oos.regime]), ("E + F + guard", oos[~oos.control & oos.fresh & (oos.panels >= 6) & (oos.atr_pct >= 5) & oos.regime]),
                        ("control, ATR≥5%", oos[oos.control & (oos.atr_pct >= 5)])]:
            if len(g): rows.append({"rule": name, **stats(g)})
        out = pd.DataFrame(rows); print(f"\n== {a.split}+ ==\n" + out.to_string(index=False))
        yr = tr[~tr.control].groupby("year").apply(lambda g: pd.Series(stats(g))); print("\n== C by signal year ==\n" + yr.to_string())
        if a.out:
            tr.to_parquet(Path("bend/data") / f"{a.out}.parquet", index=False) if False else tr.to_csv(Path("bend/data") / f"{a.out}.csv", index=False)
            md = lambda d: "| " + " | ".join(map(str, d.columns)) + " |\n|" + "---|" * len(d.columns) + "\n" + "\n".join("| " + " | ".join(str(v) for v in r) + " |" for r in d.itertuples(index=False))  # noqa: E731
            (Path("bend/data") / f"{a.out}.md").write_text(f"# {a.dir} {a.split}+\n\n" + md(out) + "\n\n## C by signal year\n\n" + md(yr.reset_index()))
            print("wrote", a.out)


if __name__ == "__main__":
    main()
