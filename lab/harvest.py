"""Strategy Tester harvesting: signals × tickers × timeframes → backtest_grid.csv (+ ranked summary).

Per signal variant: paste the strategy twin (default "Signal" swapped), Add to chart once, then walk
symbols/timeframes by URL — the strategy stays on the chart and recalculates; read Performance Summary each time.

usage: python3.12 harvest.py [--suite homily|startupio|all] [--tickers ...] [--tfs 240 D W] [--limit N]
"""
from __future__ import annotations

import argparse
import csv
import os
import re
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import tvh  # noqa: E402

HERE = Path(__file__).parent
TWINS = {
    "homily": HERE / "strategies" / "strat_homily.pine",
    "startupio": HERE / "strategies" / "strat_startupio.pine",
    "vcp": HERE / "strategies" / "strat_vcp.pine",
}
TICKERS = ["NASDAQ:MRNA", "NASDAQ:PLTR", "NASDAQ:NVDA", "NASDAQ:INTC", "NASDAQ:IREN", "NASDAQ:ONDS", "NASDAQ:SLNH", "NASDAQ:CLSK"]   # BITF became Keel Infrastructure (ticker changed); dropped
TFS = ["D", "W"]          # 4H on the Basic plan only reaches back a few months — bull-biased, so it is opt-in (--tfs 240 D W)
METRICS = ["bh_pct", "alpha_pct", "tpw", "bear_pnl_pct", "bull_pnl_pct", "short_pnl_pct", "exposure_pct"]   # status-line plot order in every twin


def signals(code: str) -> list[str]:
    m = re.search(r'input\.string\("[^"]+",\s*"Signal",\s*options=\[([^\]]+)\]', code)
    return [s.strip().strip('"') for s in m.group(1).split(",")] if m else []


def variant(code: str, value: str | None, label: str = "Signal") -> str:
    """Swap the default of the input.string whose title is `label` (the twin's Signal / Exit knobs)."""
    if value is None:
        return code
    return re.sub(r'(input\.string\()"[^"]+"(,\s*"' + re.escape(label) + '")', lambda m: f'{m.group(1)}"{value}"{m.group(2)}', code, count=1)


def default_of(code: str, label: str) -> str:
    m = re.search(r'input\.string\("([^"]+)",\s*"' + re.escape(label) + '"', code)
    return m.group(1) if m else ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--suite", default="all")
    ap.add_argument("--tickers", nargs="*", default=TICKERS)
    ap.add_argument("--tfs", nargs="*", default=TFS)
    ap.add_argument("--limit", type=int, default=0, help="stop after N backtests (smoke test)")
    ap.add_argument("--settle", type=float, default=2.0, help="seconds to wait for the tester after a symbol change")
    ap.add_argument("--signals", nargs="*", help="regex filters: only Signal options matching one of these")
    ap.add_argument("--exits", nargs="*", help="Exit options to iterate (default: the twin's default exit only)")
    args = ap.parse_args()

    judge = None
    if os.environ.get("TYPESAFE_API_KEY"):
        import typesafe_judge as judge  # noqa

    out = HERE / "backtest_grid.csv"
    fields = ["suite", "signal", "exit", "symbol", "tf", "net_profit_pct", "profit_factor", "win_rate", "max_dd_pct", "trades", "avg_trade_pct", *METRICS, "robustness", "overfit_risk", "raw"]
    done = 0
    tvh.connect()
    from browser_harness.helpers import switch_tab, press_key
    switch_tab(tvh.TAB_FILE.read_text().strip())
    press_key("Escape")
    tvh.focus_emulation()
    from browser_harness.helpers import activate_tab
    activate_tab(tvh.TAB_FILE.read_text().strip())   # a hidden tab drops clicks and pastes
    tvh.remove_indicator(r"twin")
    tvh.set_symbol_ui(args.tickers[0], args.tfs[0])
    tvh.open_pine_editor()
    tvh.new_script("strategy")            # ONE unsaved Untitled strategy; every variant is pasted into it and
    prev = None                           # "Update on chart" swaps the instance in place, report stays open
    with out.open("a", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=fields)
        if fh.tell() == 0:
            w.writeheader()
        for suite, path in TWINS.items():
            if args.suite not in ("all", suite):
                continue
            code = path.read_text(encoding="utf-8")
            sigs = [s for s in signals(code) if not args.signals or any(re.search(p, s, re.I) for p in args.signals)]
            exits = args.exits or [None]
            for sig in sigs:
              for ex in exits:
                exname = ex or default_of(code, "Exit")
                tvh.open_pine_editor()
                tvh.set_pine_source(variant(variant(code, sig), ex, "Exit"))
                res = tvh.add_to_chart()
                if not res["ok"]:
                    print(f"{suite}:{sig}/{exname} did not compile → {res['tail'][-200:]}")
                    continue
                if not tvh.open_strategy_tester():
                    print(f"{suite}:{sig}/{exname} strategy report did not open; skipping")
                    continue
                if not tvh.set_symbol_ui(args.tickers[0], args.tfs[0]):
                    print("could not reset the chart symbol; stopping")
                    return
                ks, vals, fresh = tvh.wait_cell(prev, 40)          # the freshly updated variant's first cell
                prev = (ks, vals)
                for si, sym in enumerate(args.tickers):
                    for ti, tf in enumerate(args.tfs):
                        if not tvh.set_symbol_ui(sym, tf):
                            print(f"{suite}:{sig} {sym} {tf}: chart did not switch (legend: {tvh.legend_head()}); skipped")
                            continue
                        if not (si == 0 and ti == 0):
                            ks, vals, fresh = tvh.wait_cell(prev, 40)
                            prev = (ks, vals)
                        if not fresh:
                            print(f"{suite}:{sig}/{exname} {sym} {tf}: report did not refresh in time; skipped")
                            continue
                        m = tvh.metrics_from_key_stats(ks)
                        m.update({k: (vals[i] if i < len(vals) else None) for i, k in enumerate(METRICS)})
                        row = {"suite": suite, "signal": sig, "exit": exname, "symbol": sym, "tf": tf, **m, "robustness": "", "overfit_risk": "", "raw": (tvh.legend_head() + " ‖ " + str(ks))[:300]}
                        if judge and m.get("trades"):
                            try:
                                a = judge.triage_backtest({"ticker": sym, "timeframe": tf, "strategy": f"{suite}:{sig}", **m})
                                row["robustness"] = round(a["robustness"]["score"], 2)
                                row["overfit_risk"] = round(a["overfit_risk"]["noul"], 2)
                            except Exception as e:
                                row["robustness"] = f"err:{e}"[:40]
                        w.writerow(row)
                        fh.flush()
                        done += 1
                        print(f"{suite:9s} {sig:22s} {exname:18s} {sym:13s} {tf:3s}  PF={m['profit_factor']}  net%={m['net_profit_pct']}  DD%={m['max_dd_pct']}  trades={m['trades']}  BH%={m['bh_pct']}  alpha%={m['alpha_pct']}  tpw={m['tpw']}  bear%={m['bear_pnl_pct']}  short%={m['short_pnl_pct']}")
                        if args.limit and done >= args.limit:
                            tvh.remove_indicator(r"twin")
                            tvh.set_symbol_ui("NASDAQ:MRNA", "D")
                            print(f"limit reached; wrote {out}")
                            return
    tvh.remove_indicator(r"twin")
    tvh.set_symbol_ui("NASDAQ:MRNA", "D")
    print(f"wrote {out} ({done} backtests)")


if __name__ == "__main__":
    main()
