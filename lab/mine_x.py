"""Ground-truth mining: walk the founders' X timelines in your logged-in Chrome and collect every post that
states a checkable claim (hole boundaries, momentum bars, diamond counts, red/yellow candles).

usage: python3.12 mine_x.py [--accounts dannycheng2022 sunxliao wayneliangs] [--scrolls 40] [--query "volatility hole"]
output: x_posts.jsonl (raw) + x_claims.csv (TypeSafe-typed: signal, has_numbers, conviction + extracted numbers/tickers/dates)
"""
from __future__ import annotations

import argparse
import csv
import json
import os
import re
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import tvh  # noqa: E402
from browser_harness.helpers import js, new_tab, goto_url, wait_for_load, scroll, wait  # noqa: E402

HERE = Path(__file__).parent
NUM = re.compile(r"\$?\d{1,5}(?:\.\d{1,3})?")
TICK = re.compile(r"\$[A-Z]{1,5}\b")


def collect_articles() -> list[dict]:
    return js(r"""(() => [...document.querySelectorAll('article')].map(a => {
        const t = a.querySelector('time'); const link = t && t.closest('a');
        const txt = [...a.querySelectorAll('[data-testid="tweetText"]')].map(e => e.innerText).join('\n');
        return {date: t ? t.getAttribute('datetime') : null, url: link ? link.href : null, text: txt};
      }).filter(x => x.text && x.url))()""") or []


def harvest(url: str, scrolls: int) -> dict[str, dict]:
    goto_url(url)
    wait_for_load(30)
    wait(3)
    seen: dict[str, dict] = {}
    stale = 0
    for _ in range(scrolls):
        for a in collect_articles():
            if a["url"] not in seen:
                seen[a["url"]] = a
        before = len(seen)
        scroll(600, 500, dy=2500)
        wait(1.6)
        stale = stale + 1 if len(seen) == before else 0
        if stale >= 4:
            break
    return seen


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--accounts", nargs="*", default=["dannycheng2022", "sunxliao", "wayneliangs"])
    ap.add_argument("--scrolls", type=int, default=40)
    ap.add_argument("--query", default=None, help='e.g. "volatility hole" → uses X search per account')
    args = ap.parse_args()

    judge = None
    if os.environ.get("TYPESAFE_API_KEY"):
        import typesafe_judge as judge  # noqa

    tvh.connect()
    new_tab("https://x.com/home")
    wait_for_load(30)
    posts: dict[str, dict] = {}
    for acct in args.accounts:
        url = f"https://x.com/search?q={args.query.replace(' ', '%20')}%20from%3A{acct}&src=typed_query&f=live" if args.query else f"https://x.com/{acct}"
        got = harvest(url, args.scrolls)
        for u, p in got.items():
            p["account"] = acct
        posts.update(got)
        print(f"{acct}: {len(got)} posts")

    raw = HERE / "x_posts.jsonl"
    with raw.open("w", encoding="utf-8") as fh:
        for p in posts.values():
            fh.write(json.dumps(p, ensure_ascii=False) + "\n")

    out = HERE / "x_claims.csv"
    with out.open("w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["account", "date", "url", "tickers", "numbers", "signal", "signal_p", "has_numbers", "conviction", "text"])
        for p in posts.values():
            tickers = " ".join(sorted(set(TICK.findall(p["text"]))))
            numbers = " ".join(NUM.findall(p["text"])[:12])
            sig = sp = hn = conv = ""
            if judge and (tickers or numbers):
                try:
                    a = judge.extract_claim(p["account"], p["text"])
                    sig = a["signal"]["choice"]
                    sp = round(a["signal"]["confidence"], 2)
                    hn = round(a["has_numbers"]["noul"], 2)
                    conv = round(a["conviction"]["score"], 2)
                except Exception as e:
                    sig = f"err:{e}"[:40]
            w.writerow([p["account"], p["date"], p["url"], tickers, numbers, sig, sp, hn, conv, p["text"].replace("\n", " ")[:500]])
    print(f"wrote {raw} and {out} ({len(posts)} posts)")


if __name__ == "__main__":
    main()
