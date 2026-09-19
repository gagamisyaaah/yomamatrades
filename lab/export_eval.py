"""Export chart data (needs a plan with Chart menu → Export chart data…) and evaluate signals offline:
forward-return distributions 5 / 20 / 60 bars after each event column that is non-NA.

usage:
  python3.12 export_eval.py export --symbol NASDAQ:IREN --tf D          # triggers the download into ~/Downloads
  python3.12 export_eval.py eval  ~/Downloads/NASDAQ_IREN*.csv          # scores every event column in the CSV
"""
from __future__ import annotations

import csv
import glob
import statistics
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))


def export(symbol: str, tf: str):
    import tvh
    tvh.connect()
    tvh.open_chart(symbol, tf)
    ok = tvh.export_chart_data()
    print("export triggered" if ok else "Export chart data not found in the chart menu (plan may not include it) — try tvh.probe()")


def evaluate(path: str, horizons=(5, 20, 60)):
    rows = list(csv.DictReader(open(path, encoding="utf-8-sig")))
    if not rows:
        print("empty csv"); return
    closes = [float(r.get("close") or r.get("Close") or 0) for r in rows]
    cols = [c for c in rows[0] if c.lower() not in ("time", "open", "high", "low", "close", "volume")]
    print(f"{len(rows)} bars, {len(cols)} indicator columns")
    for c in cols:
        events = [i for i, r in enumerate(rows) if r.get(c) not in ("", "NaN", None)]
        if not 3 <= len(events) <= len(rows) // 3:
            continue  # constant series (a line) or too rare — only event-like columns are scored
        line = f"{c[:40]:40s} n={len(events):4d}"
        for h in horizons:
            rets = [(closes[i + h] / closes[i] - 1) * 100 for i in events if i + h < len(closes) and closes[i] > 0]
            if rets:
                win = sum(r > 0 for r in rets) / len(rets) * 100
                line += f" | +{h}b: med {statistics.median(rets):6.2f}%  win {win:5.1f}%"
        print(line)


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "export":
        sym = sys.argv[sys.argv.index("--symbol") + 1] if "--symbol" in sys.argv else "NASDAQ:IREN"
        tf = sys.argv[sys.argv.index("--tf") + 1] if "--tf" in sys.argv else "D"
        export(sym, tf)
    elif len(sys.argv) > 2 and sys.argv[1] == "eval":
        for p in glob.glob(sys.argv[2]):
            print("==", p); evaluate(p)
    else:
        print(__doc__)
