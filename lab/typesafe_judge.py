"""TypeSafe (System One / jev) judgments for the TradingView indicator lab.

Each function asks ONE narrow typed question over structured state and returns the typed answer plus the
probability distribution, per the typesafe-ai skill. Needs TYPESAFE_API_KEY in the environment.
"""
from __future__ import annotations

import json
import os
import time
import urllib.request

API = "https://api.typesafe.ai/v1/systemone"
MODEL = os.environ.get("TYPESAFE_MODEL", "jev-latest")


def ask(state, questions: dict, retries: int = 4) -> dict:
    body = json.dumps({"state": state, "model": MODEL, "questions": questions}).encode()
    req = urllib.request.Request(
        API, data=body, method="POST",
        headers={"Authorization": f"Bearer {os.environ['TYPESAFE_API_KEY']}", "Content-Type": "application/json"},
    )
    for attempt in range(retries):
        try:
            with urllib.request.urlopen(req, timeout=30) as r:
                return json.load(r)["answers"]
        except urllib.error.HTTPError as e:
            if e.code in (429, 529) and attempt < retries - 1:
                time.sleep(2 ** attempt)
                continue
            raise


# ── 1. Compile lab: classify the Pine console text jev reads back ─────────────────────────────────
def classify_console(script_name: str, console_text: str) -> dict:
    state = {"script": script_name, "pine_console": console_text[:4000]}
    return ask(state, {
        "outcome": {
            "type": "choice",
            "instructions": "What did TradingView's Pine console report for this script?",
            "criteria": {
                "compiled_clean": "no error line; the script was added to the chart",
                "compile_error": "a red 'line N: ...' error that stops compilation",
                "runtime_error": "compiled but a runtime/study error (array out of bounds, too many drawings, timeout)",
                "warning_only": "orange/yellow warnings but the script runs",
                "login_or_permission": "the page asked to sign in or blocked the action",
            },
        },
        "needs_code_change": {
            "type": "noul",
            "instructions": "Does fixing this require editing the Pine source (as opposed to retrying or logging in)?",
        },
    })


# ── 2. Ground-truth mining: does a founder's post state a checkable claim? ───────────────────────
def extract_claim(author: str, text: str) -> dict:
    state = {"author": author, "post": text[:2000]}
    return ask(state, {
        "signal": {
            "type": "choice",
            "instructions": "Which indicator event is this post reporting, if any?",
            "criteria": {
                "blue_diamond": "a blue/bullish diamond or buy confirmation",
                "pink_diamond": "a pink/bearish diamond or sell confirmation",
                "volatility_hole": "a volatility hole / black hole with or without boundaries",
                "momentum_bar_level": "a momentum-bar / support / resistance price level",
                "candle_colour": "a red or yellow candle signal",
                "none": "no concrete indicator event",
            },
        },
        "has_numbers": {
            "type": "noul",
            "instructions": "Does the post state explicit price levels or dates that can be checked against a chart?",
        },
        "conviction": {
            "type": "score",
            "instructions": "How strongly does the author commit to a direction?",
            "criteria": ["hedged or neutral", "leaning", "clear call", "emphatic call with target"],
        },
    })


# ── 3. Backtest triage: rank a Strategy Tester row before humans look at it ─────────────────────
def triage_backtest(row: dict) -> dict:
    """row = {ticker, timeframe, strategy, net_profit_pct, profit_factor, win_rate, max_dd_pct, trades}"""
    return ask(row, {
        "robustness": {
            "type": "score",
            "instructions": "Judge how trustworthy this backtest is as evidence, considering trade count, drawdown and profit factor together.",
            "criteria": ["noise (too few trades or DD swamps profit)", "weak", "promising", "strong and well-sampled"],
        },
        "overfit_risk": {
            "type": "noul",
            "instructions": "Is this result likely an artifact of one outsized move rather than a repeatable edge?",
        },
    })


if __name__ == "__main__":
    print(json.dumps(classify_console("demo", "line 12: Undeclared identifier 'foo'"), indent=2))
