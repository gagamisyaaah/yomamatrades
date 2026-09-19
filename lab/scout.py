"""Community-library scout: search TradingView's public scripts for each term, open the script pages,
copy the source of open-source hits into corpus/<term>/<slug>.pine with a manifest.

Uses the public script pages (tradingview.com/scripts/?text=...) which render the source in-page for
open-source scripts, so no Indicators dialog scraping is needed.

usage: python3.12 scout.py [--terms "MCDX" "diamond" "volatility hole" "money flow" "chip distribution"] [--per-term 8]
"""
from __future__ import annotations

import argparse
import json
import re
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import tvh  # noqa: E402
from browser_harness.helpers import js, new_tab, goto_url, wait_for_load, wait  # noqa: E402

HERE = Path(__file__).parent
TERMS = ["MCDX", "diamond", "volatility hole", "money flow", "chip distribution", "trend expert", "wavetrend"]


def search_links(term: str, limit: int) -> list[dict]:
    goto_url(f"https://www.tradingview.com/scripts/?text={term.replace(' ', '%20')}&script_access=open")
    wait_for_load(30)
    wait(2.5)
    links = js(r"""(() => { const out = []; const seen = new Set();
        for (const a of document.querySelectorAll('a[href*="/script/"]')) {
            const href = a.href.split('?')[0]; if (seen.has(href)) continue; seen.add(href);
            out.push({href, title: (a.innerText || '').trim().slice(0, 120)}); }
        return out; })()""") or []
    return [l for l in links if l["title"]][:limit]


def read_source() -> dict:
    # open-source pages carry the code in a <pre>/code block; protected ones say so
    return js(r"""(() => {
        const btn = [...document.querySelectorAll('button, a')].find(b => /source code/i.test(b.innerText || ''));
        if (btn) btn.click();
        const pre = [...document.querySelectorAll('pre, code, [class*="code"]')].map(e => e.innerText || '').filter(t => /@version=/.test(t)).sort((a,b) => b.length - a.length)[0] || '';
        const likes = (document.body.innerText.match(/(\d[\d,]*)\s+(likes|boosts)/i) || [])[1] || '';
        const protectedTxt = /protected script|invite-only/i.test(document.body.innerText) ? 'protected' : 'open';
        return {code: pre, likes, access: protectedTxt, title: document.title}; })()""") or {}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--terms", nargs="*", default=TERMS)
    ap.add_argument("--per-term", type=int, default=8)
    args = ap.parse_args()
    tvh.connect()
    new_tab("https://www.tradingview.com/scripts/")
    wait_for_load(30)
    manifest = []
    for term in args.terms:
        d = HERE / "corpus" / re.sub(r"[^a-z0-9]+", "_", term.lower())
        d.mkdir(parents=True, exist_ok=True)
        for l in search_links(term, args.per_term):
            goto_url(l["href"])
            wait_for_load(30)
            wait(2.0)
            src = read_source()
            if src.get("code"):
                slug = re.sub(r"[^a-z0-9]+", "_", l["title"].lower())[:60] or "script"
                (d / f"{slug}.pine").write_text(src["code"], encoding="utf-8")
            manifest.append({"term": term, **l, "likes": src.get("likes"), "access": src.get("access"), "chars": len(src.get("code", ""))})
            print(f"{term:18s} {src.get('access','?'):9s} {src.get('likes',''):>7s}  {l['title'][:60]}")
    (HERE / "corpus" / "manifest.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")
    print(f"corpus written to {HERE / 'corpus'}")


if __name__ == "__main__":
    main()
