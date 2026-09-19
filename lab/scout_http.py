"""scout_http.py — community-library scout over plain HTTP (no browser).

For each search term: query TradingView's script search JSON (the endpoint the site's own search box
uses), page 1 + page 2, collect the cards into scout/manifest.csv (dedupe by id), then fetch the Pine
source of the top-N open-source scripts by likes through pine-facade and save scout/src/<id>.pine.
Every facade JSON is cached under scout/raw/<id>.json and never re-fetched.

Why not the HTML listing (`/scripts/?text=<term>`)? Verified 2026-09-19: the server-rendered page
embeds 24 cards under data.ideas.data.items, but it is the same "hot" feed for every term and for
`&page=2` (24/24 identical ids for "breakout", "squeeze", page 2). The term is applied client-side.
parse_listing_html() still parses those cards; the run fetches the page per term and merges the cards
only if the id sets actually differ between terms — otherwise it logs that the feed is term-agnostic.

usage: python3.12 scout_http.py [--top 60] [--terms ...]
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

HERE = Path(__file__).parent
SCOUT = HERE / "scout"
RAW = SCOUT / "raw"
SRC = SCOUT / "src"

TERMS = ["breakout", "squeeze", "volatility contraction", "momentum", "trend", "reversal", "volume",
         "smart money", "whale", "chip distribution", "diamond", "swing", "buy sell signal", "supertrend",
         "trend following"]

SEARCH_URL = "https://www.tradingview.com/pubscripts-suggest-json/?search={q}"
LISTING_URL = "https://www.tradingview.com/scripts/?text={q}"
FACADE_URL = "https://pine-facade.tradingview.com/pine-facade/get/{pid}/last?no_4xx=true"
SCRIPT_URL = "https://www.tradingview.com/script/{image}-{slug}/"

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) "
                  "Chrome/128.0.0.0 Safari/537.36",
    "Accept": "application/json,text/html;q=0.9,*/*;q=0.8",
    "Accept-Language": "en-US,en;q=0.9",
}
MIN_GAP = 1.1      # seconds between requests
BACKOFF_429 = 20.0

_last_request = 0.0


def log(msg: str) -> None:
    print(msg, flush=True)


def get(url: str, tries: int = 3) -> bytes:
    """Polite GET: >= MIN_GAP between requests, 20 s back-off on 429, raises on final failure."""
    global _last_request
    for attempt in range(tries):
        wait = MIN_GAP - (time.monotonic() - _last_request)
        if wait > 0:
            time.sleep(wait)
        _last_request = time.monotonic()
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=HEADERS), timeout=40) as r:
                return r.read()
        except urllib.error.HTTPError as e:
            if e.code == 429 and attempt < tries - 1:
                log(f"  429 on {url} — backing off {BACKOFF_429:.0f}s")
                time.sleep(BACKOFF_429)
                continue
            raise
    raise RuntimeError(f"unreachable: {url}")


def file_id(script_id: str) -> str:
    """'PUB;abc' -> 'PUB_abc' (the ';' is legal in file names but hostile to shells)."""
    return re.sub(r"[^A-Za-z0-9_.-]", "_", script_id)


def slugify(name: str) -> str:
    return re.sub(r"-+", "-", re.sub(r"[^A-Za-z0-9]+", "-", name)).strip("-") or "script"


# ── card sources ────────────────────────────────────────────────────────────────────────────────
def search_cards(term: str, pages: int = 2) -> list[dict]:
    """Cards from the search JSON. Keys seen: scriptIdPart, scriptName, agreeCount (likes), access
    (1 = open source, 3 = protected/invite-only), type (1 = indicator, 2 = strategy), author.username,
    imageUrl (script-page path prefix); `next` carries the offset cursor for the following page."""
    cards = []
    url = SEARCH_URL.format(q=urllib.parse.quote(term))
    for page in range(1, pages + 1):
        j = json.loads(get(url))
        for r in j.get("results", []):
            cards.append({
                "id": r["scriptIdPart"],
                "name": r.get("scriptName") or r.get("title") or "",
                "author": (r.get("author") or {}).get("username", ""),
                "likes": int(r.get("agreeCount") or 0),
                "open_source": r.get("access") == 1,
                "kind": (r.get("extra") or {}).get("kind") or ("strategy" if r.get("type") == 2 else "study"),
                "url": SCRIPT_URL.format(image=r.get("imageUrl", ""), slug=slugify(r.get("scriptName") or "")),
                "term": term,
            })
        nxt = j.get("next")
        if not nxt:
            break
        url = nxt if nxt.startswith("http") else "https://www.tradingview.com" + nxt
    return cards


_CARD_START = re.compile(r'\{"id":\d+,"image_url":"')


def parse_listing_html(html: str, term: str) -> list[dict]:
    """Cards embedded in the server-rendered /scripts/ page (data.ideas.data.items). Keys:
    script_id_part, name, likes_count, script_access (1 = open), script_type, user.username, chart_url."""
    dec = json.JSONDecoder()
    cards = []
    for m in _CARD_START.finditer(html):
        try:
            obj, _ = dec.raw_decode(html, m.start())
        except ValueError:
            continue
        if not obj.get("script_id_part"):
            continue
        cards.append({
            "id": obj["script_id_part"],
            "name": obj.get("name", ""),
            "author": (obj.get("user") or {}).get("username", ""),
            "likes": int(obj.get("likes_count") or 0),
            "open_source": obj.get("script_access") == 1,
            "kind": obj.get("script_type", "indicator"),
            "url": obj.get("chart_url", ""),
            "term": term,
        })
    return cards


def listing_cards_if_term_specific(terms: list[str]) -> list[dict]:
    """Fetch the HTML listing per term; return the cards only when the id sets differ across terms."""
    per_term: dict[str, list[dict]] = {}
    for term in terms:
        try:
            html = get(LISTING_URL.format(q=urllib.parse.quote(term))).decode("utf-8", "replace")
            per_term[term] = parse_listing_html(html, term)
        except Exception as e:  # noqa: BLE001 — any single fetch failure is logged and skipped
            log(f"  listing {term!r}: FAILED {e}")
    id_sets = {frozenset(c["id"] for c in cards) for cards in per_term.values()}
    if len(per_term) >= 2 and len(id_sets) == 1:
        n = len(next(iter(id_sets)))
        log(f"  HTML listing ignores text= ({n} identical ids for all {len(per_term)} terms) — not merged")
        return []
    return [c for cards in per_term.values() for c in cards]


# ── source fetch ────────────────────────────────────────────────────────────────────────────────
def fetch_source(script_id: str) -> dict | None:
    """Facade JSON (cached): scriptName, scriptAccess ('open_no_auth' = open), source."""
    cache = RAW / f"{file_id(script_id)}.json"
    if cache.exists():
        return json.loads(cache.read_text(encoding="utf-8"))
    raw = get(FACADE_URL.format(pid=urllib.parse.quote(script_id, safe="")))
    j = json.loads(raw)
    cache.write_text(json.dumps(j, ensure_ascii=False, indent=1), encoding="utf-8")
    return j


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--terms", nargs="*", default=TERMS)
    ap.add_argument("--top", type=int, default=60)
    ap.add_argument("--pages", type=int, default=2)
    args = ap.parse_args()
    for d in (RAW, SRC):
        d.mkdir(parents=True, exist_ok=True)

    cards: list[dict] = []
    per_term_found: dict[str, int] = {}
    for term in args.terms:
        try:
            found = search_cards(term, args.pages)
        except Exception as e:  # noqa: BLE001
            log(f"{term:24s} search FAILED: {e}")
            found = []
        per_term_found[term] = len(found)
        log(f"{term:24s} {len(found):3d} cards  ({sum(c['open_source'] for c in found)} open-source)")
        cards.extend(found)
    log("HTML listing check:")
    cards.extend(listing_cards_if_term_specific(args.terms))

    by_id: dict[str, dict] = {}
    for c in cards:
        prev = by_id.get(c["id"])
        if prev is None:
            by_id[c["id"]] = dict(c)
        elif c["likes"] > prev["likes"]:
            prev["likes"] = c["likes"]
    manifest = sorted(by_id.values(), key=lambda c: -c["likes"])
    with (SCOUT / "manifest.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=["id", "name", "author", "likes", "open_source", "url", "term"],
                           extrasaction="ignore")
        w.writeheader()
        w.writerows(manifest)
    n_open = sum(c["open_source"] for c in manifest)
    log(f"\n{len(cards)} cards found, {len(manifest)} unique ids, {n_open} open-source")

    targets = [c for c in manifest if c["open_source"]][:args.top]
    fetched = 0
    for i, c in enumerate(targets, 1):
        fid = file_id(c["id"])
        try:
            j = fetch_source(c["id"])
            src = (j or {}).get("source") or ""
            if (j or {}).get("scriptAccess") != "open_no_auth" or not src.strip():
                log(f"  [{i:2d}/{len(targets)}] {c['name'][:50]:50s} no open source ({(j or {}).get('scriptAccess')})")
                continue
            (SRC / f"{fid}.pine").write_text(src.replace("\r\n", "\n").replace("\r", "\n"), encoding="utf-8")
            fetched += 1
            log(f"  [{i:2d}/{len(targets)}] {c['name'][:50]:50s} {c['likes']:>6d} likes  {len(src):>6d} chars  {c['kind']}")
        except Exception as e:  # noqa: BLE001
            log(f"  [{i:2d}/{len(targets)}] {c['name'][:50]:50s} FAILED {e}")

    stats = {"terms": per_term_found, "cards_found": len(cards), "unique": len(manifest), "open_source": n_open,
             "top_targets": len(targets), "fetched": fetched}
    (SCOUT / "scout_stats.json").write_text(json.dumps(stats, indent=1), encoding="utf-8")
    log(f"\nfetched {fetched}/{len(targets)} sources -> {SRC}")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(130)
