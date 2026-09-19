"""Compile lab: paste each Pine script into your logged-in TradingView, Add to chart, read back the verdict.

usage:  python3.12 compile_lab.py [--only startupio|homily] [--symbol NASDAQ:MRNA] [--tf D] [--keep]
output: compile_report.csv + a console line per script (TypeSafe classifies the console text if a key is set)
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

ROOT = Path(__file__).resolve().parents[1]
SUITES = {
    "homily": ROOT / "tv-indicator-clones" / "homily",
    "startupio": ROOT / "tv-indicator-clones" / "startupio",
}


def short_title(code: str) -> str:
    m = re.search(r'indicator\(\s*"[^"]*"\s*,\s*"([^"]+)"', code) or re.search(r'strategy\(\s*"[^"]*"\s*,\s*"([^"]+)"', code)
    if m:
        return m.group(1)
    m = re.search(r'(?:indicator|strategy)\(\s*"([^"]+)"', code)
    return m.group(1) if m else "?"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", choices=list(SUITES))
    ap.add_argument("--symbol", default="NASDAQ:MRNA")
    ap.add_argument("--tf", default="D")
    ap.add_argument("--keep", action="store_true", help="leave indicators on the chart")
    ap.add_argument("--files", nargs="*", help="explicit .pine paths")
    args = ap.parse_args()

    files = [Path(f) for f in args.files] if args.files else sorted(
        p for k, d in SUITES.items() if not args.only or k == args.only for p in d.glob("*.pine"))

    judge = None
    if os.environ.get("TYPESAFE_API_KEY"):
        import typesafe_judge as judge  # noqa

    tvh.connect()
    from browser_harness.helpers import switch_tab, press_key
    switch_tab(tvh.TAB_FILE.read_text().strip())   # the lab tab (already on the chart)
    press_key("Escape")
    tvh.focus_emulation()
    tvh.remove_indicator(r"^Homily MACD")           # leftover from the smoke test
    tvh.open_pine_editor()
    out = Path(__file__).with_name("compile_report.csv")
    rows = []
    for f in files:
        code = f.read_text(encoding="utf-8")
        title = short_title(code)
        kind = "strategy" if re.search(r"^\s*strategy\(", code, re.M) else "indicator"
        n, added, console = -1, False, ""
        try:
            tvh.new_script(kind)
            n = tvh.set_pine_source(code)
            res = tvh.add_to_chart()
            added = res["ok"]
            console = res["tail"] or tvh.read_console()[-400:]
        except Exception as e:  # keep going; the row records the failure
            console = f"lab error: {type(e).__name__}: {str(e)[:200]}"
        head_ok = n == 1
        verdict = "compiled" if added else "check"
        judged = ""
        if judge:
            try:
                a = judge.classify_console(f.name, console or ("legend added: " + str(added)))
                judged = f'{a["outcome"]["choice"]} ({a["outcome"]["confidence"]:.2f}); needs_code_change={a["needs_code_change"]["noul"]:.2f}'
            except Exception as e:  # keep the lab running without the judge
                judged = f"judge error: {e}"
        row = {"file": f.name, "title": title, "lines": n, "editor_ok": head_ok, "added_to_chart": added,
               "verdict": verdict, "typesafe": judged, "console": console.replace("\n", " | ")[:600]}
        rows.append(row)
        print(f"{'OK ' if added else 'ERR'} {f.name:45s} {judged or verdict}  {row['console'][:160]}")
        if added and not args.keep:
            tvh.remove_indicator("^" + re.escape(title[:14]))
            time.sleep(0.8)
    with out.open("w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"\nwrote {out}  ({sum(r['added_to_chart'] for r in rows)}/{len(rows)} added to chart)")


if __name__ == "__main__":
    main()
