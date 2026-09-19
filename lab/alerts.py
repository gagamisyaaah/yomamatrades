"""Create TradingView alerts for every alertcondition() of an indicator already on the chart.
Delivery = app/email notifications (set in TradingView once); no webhook server.

usage: python3.12 alerts.py --title "SU Echo" [--only "BLUE|PINK"]
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import tvh  # noqa: E402
from browser_harness.helpers import js, wait, press_key  # noqa: E402


def open_alert_dialog():
    tvh.click_by_text(r"^(alert|create alert)$")
    wait(1.5)
    return bool(js("!!document.querySelector('[data-dialog-name*=\"alert\" i], [role=\"dialog\"]')"))


def list_conditions() -> list[str]:
    # condition dropdown lists indicator plots + alertcondition titles as options
    js("""(() => { const dlg = document.querySelector('[role="dialog"]'); const b = dlg && [...dlg.querySelectorAll('[role="button"], button, [class*="select" i]')].find(e => /condition|crossing|indicator/i.test(e.innerText||'')); if (b) b.click(); })()""")
    wait(1.0)
    return js("[...document.querySelectorAll('[role=\"option\"], [role=\"menuitem\"], [class*=\"option\" i]')].map(e => e.innerText.trim()).filter(Boolean)") or []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--title", required=True, help="indicator short title as shown in the legend")
    ap.add_argument("--only", default=None, help="regex on alertcondition names")
    args = ap.parse_args()
    tvh.connect()
    tvh.open_chart()
    if not open_alert_dialog():
        print("alert dialog not found — run tvh.probe() and adapt"); return
    opts = list_conditions()
    print("conditions seen:", json.dumps(opts[:40], indent=1))
    print("Next: pick the indicator, then each alertcondition; this scaffold stops here until the dialog DOM is confirmed live.")
    press_key("Escape")


if __name__ == "__main__":
    main()
