"""Backtest arbitrary strategy files (e.g. the auto-wrapped community scripts) across tickers on TradingView.

usage: python3.12 harvest_files.py --files scout/wrapped/*.pine [--tickers ...] [--tfs D] [--out backtest_scout.csv]
Same mechanics as harvest.py (one unsaved Untitled strategy, Update on chart per file, Key stats + status-line metrics),
but the legend title is read from each file's strategy() declaration.
"""
from __future__ import annotations

import argparse
import csv
import os
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import tvh  # noqa: E402
from harvest import METRICS, TICKERS  # noqa: E402

HERE = Path(__file__).parent


def legend_regex(code: str) -> str:
    m = re.search(r'strategy\(\s*"([^"]*)"\s*(?:,\s*(?:shorttitle\s*=\s*)?"([^"]*)")?', code)
    if not m:
        return r"."
    title = (m.group(2) or m.group(1) or "").strip()
    return "^" + re.escape(title[:18])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--files", nargs="+", required=True)
    ap.add_argument("--tickers", nargs="*", default=TICKERS)
    ap.add_argument("--tfs", nargs="*", default=["D"])
    ap.add_argument("--out", default="backtest_scout.csv")
    ap.add_argument("--settle", type=float, default=2.0)
    args = ap.parse_args()
    judge = None
    if os.environ.get("TYPESAFE_API_KEY"):
        import typesafe_judge as judge  # noqa
    out = HERE / args.out
    fields = ["file", "title", "symbol", "tf", "net_profit_pct", "profit_factor", "win_rate", "max_dd_pct", "trades", "avg_trade_pct", *METRICS, "robustness", "overfit_risk", "raw"]
    tvh.connect()
    from browser_harness.helpers import switch_tab, press_key, activate_tab
    tab = tvh.TAB_FILE.read_text().strip()
    switch_tab(tab)
    activate_tab(tab)
    press_key("Escape")
    tvh.focus_emulation()
    tvh.set_symbol_ui(args.tickers[0], args.tfs[0])
    tvh.open_pine_editor()
    tvh.new_script("strategy")
    prev = None
    last_title = None
    done = 0
    with out.open("a", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=fields)
        if fh.tell() == 0:
            w.writeheader()
        for f in args.files:
            code = Path(f).read_text(encoding="utf-8")
            title_re = legend_regex(code)
            if last_title:
                tvh.remove_indicator(last_title)
            tvh.open_pine_editor()
            tvh.set_pine_source(code)
            res = tvh.add_to_chart()
            if not res["ok"]:
                print(f"{Path(f).name}: did not compile → {res['tail'][-160:]}")
                w.writerow({"file": Path(f).name, "title": title_re, "symbol": "", "tf": "", "raw": "compile: " + res["tail"][-200:]})
                fh.flush()
                last_title = None
                continue
            last_title = title_re
            if not tvh.open_strategy_tester():
                print(f"{Path(f).name}: report did not open; skipping")
                continue
            if not tvh.set_symbol_ui(args.tickers[0], args.tfs[0]):
                print("could not reset the chart symbol; stopping")
                return
            ks, vals, fresh = tvh.wait_cell(prev, 40, title_re)
            prev = (ks, vals)
            for si, sym in enumerate(args.tickers):
                for ti, tf in enumerate(args.tfs):
                    try:
                        if not tvh.set_symbol_ui(sym, tf):
                            print(f"{sym} {tf}: chart did not switch; skipped")
                            continue
                        if not (si == 0 and ti == 0):
                            ks, vals, fresh = tvh.wait_cell(prev, 40, title_re)
                            prev = (ks, vals)
                    except Exception as e:  # noqa: BLE001 — a blocked page must not end the run
                        print(f"{Path(f).name} {sym} {tf}: {type(e).__name__}; skipped")
                        continue
                    if not fresh:
                        print(f"{Path(f).name} {sym} {tf}: report did not refresh; skipped")
                        continue
                    m = tvh.metrics_from_key_stats(ks)
                    m.update({k: (vals[i] if i < len(vals) else None) for i, k in enumerate(METRICS)})
                    row = {"file": Path(f).name, "title": title_re, "symbol": sym, "tf": tf, **m, "robustness": "", "overfit_risk": "", "raw": (tvh.legend_head() + " ‖ " + str(ks))[:300]}
                    if judge and m.get("trades"):
                        try:
                            a = judge.triage_backtest({"ticker": sym, "timeframe": tf, "strategy": Path(f).name, **m})
                            row["robustness"] = round(a["robustness"]["score"], 2)
                            row["overfit_risk"] = round(a["overfit_risk"]["noul"], 2)
                        except Exception as e:  # noqa: BLE001
                            row["robustness"] = f"err:{e}"[:40]
                    w.writerow(row)
                    fh.flush()
                    done += 1
                    print(f"{Path(f).name[:34]:34s} {sym:13s} {tf:3s} PF={m['profit_factor']} net%={m['net_profit_pct']} trades={m['trades']} alpha%={m['alpha_pct']}")
        if last_title:
            tvh.remove_indicator(last_title)
    tvh.set_symbol_ui("NASDAQ:MRNA", "D")
    print(f"wrote {out} ({done} backtests)")


if __name__ == "__main__":
    main()
