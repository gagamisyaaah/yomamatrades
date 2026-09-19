"""Pack cached daily bars into the engine's binary format (one file per ticker, little-endian U32 words).

Layout per bar, 6 words: yyyymmdd, open×10000, high×10000, low×10000, close×10000, volume/100 (prices above $429,496 cannot be packed → skipped).
usage: python3.12 bend/pack.py [--tickers MRNA IREN ...] [--limit N] [--tf D|W]
writes bend/data/<TF>/<TICKER>.bin and bend/data/<TF>/list.txt (ticker and bar count per line).
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

import pandas as pd

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
from mine import data  # noqa: E402


def weekly(df: pd.DataFrame) -> pd.DataFrame:
    w = df.set_index("date").resample("W-FRI").agg({"open": "first", "high": "max", "low": "min", "close": "last", "volume": "sum"}).dropna()
    return w.reset_index()


def pack(ticker: str, tf: str, out: Path) -> int:
    df = data.fetch(ticker)
    if df is None:
        return 0
    if tf == "W":
        df = weekly(df)
    words = []
    for r in df.itertuples(index=False):
        words += [int(r.date.strftime("%Y%m%d")), int(round(r.open * 10000)), int(round(r.high * 10000)), int(round(r.low * 10000)), int(round(r.close * 10000)), int(r.volume // 100)]
    if any(w < 0 or w > 0xFFFFFFFF for w in words):
        return 0
    (out / f"{ticker}.bin").write_bytes(struct.pack(f"<{len(words)}I", *words))
    return len(df)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tickers", nargs="*")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--tf", default="D", choices=["D", "W"])
    args = ap.parse_args()
    out = HERE / "data" / args.tf
    out.mkdir(parents=True, exist_ok=True)
    ticks = args.tickers or sorted(p.stem for p in (HERE.parent / "mine" / "cache").glob("*.csv"))
    if args.limit:
        ticks = ticks[: args.limit]
    lines = []
    for t in ticks:
        n = pack(t, args.tf, out)
        if n:
            lines.append(f"{t} {n}")
    (out / "list.txt").write_text("\n".join(lines) + "\n")
    print(f"packed {len(lines)} tickers → {out}")


if __name__ == "__main__":
    main()
