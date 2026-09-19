"""Parameter sweeps: change one knob at a time through the strategy's settings dialog, read the tester.
Coarse grids only; pick the plateau, not the peak.

usage: python3.12 sweep.py --suite homily --signal "Volatility hole" --knob "Hole k" --values 1.0 1.2 1.4 1.7 2.0 2.5 \
                          [--tickers NASDAQ:MRNA NASDAQ:PLTR] [--tfs D]
output: sweep_<knob>.csv
"""
from __future__ import annotations

import argparse
import csv
import re
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import tvh  # noqa: E402
from harvest import TWINS, variant  # noqa: E402

HERE = Path(__file__).parent


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--suite", required=True, choices=list(TWINS))
    ap.add_argument("--signal", required=True)
    ap.add_argument("--knob", required=True, help="regex matching the input label in the settings dialog")
    ap.add_argument("--values", nargs="+", required=True)
    ap.add_argument("--tickers", nargs="*", default=["NASDAQ:MRNA", "NASDAQ:PLTR", "NASDAQ:NVDA", "NASDAQ:IREN"])
    ap.add_argument("--tfs", nargs="*", default=["D"])
    ap.add_argument("--settle", type=float, default=4.0)
    args = ap.parse_args()

    code = variant(TWINS[args.suite].read_text(encoding="utf-8"), args.signal)
    out = HERE / f"sweep_{re.sub(r'[^a-z0-9]+', '_', args.knob.lower())}.csv"
    tvh.connect()
    tvh.open_chart(args.tickers[0], args.tfs[0])
    tvh.open_pine_editor()
    tvh.set_pine_source(code)
    tvh.add_to_chart()
    time.sleep(3)
    tvh.open_strategy_tester()
    with out.open("w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["knob", "value", "symbol", "tf", "net_profit_pct", "profit_factor", "win_rate", "max_dd_pct", "trades"])
        for val in args.values:
            if not tvh.open_settings(r"twin"):
                print("could not open settings dialog — run tvh.probe() and adapt selectors")
                return
            ok = tvh.set_input(args.knob, val)
            tvh.settings_ok()
            if not ok:
                print(f"knob '{args.knob}' not found in dialog")
                return
            for sym in args.tickers:
                for tf in args.tfs:
                    tvh.set_symbol(sym, tf)
                    time.sleep(args.settle)
                    m = tvh.metrics_from_summary(tvh.performance_summary())
                    w.writerow([args.knob, val, sym, tf, m["net_profit_pct"], m["profit_factor"], m["win_rate"], m["max_dd_pct"], m["trades"]])
                    fh.flush()
                    print(f"{args.knob}={val:>6} {sym:13s} {tf:3s} PF={m['profit_factor']} net%={m['net_profit_pct']} DD%={m['max_dd_pct']} n={m['trades']}")
    tvh.remove_indicator(r"twin")
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
