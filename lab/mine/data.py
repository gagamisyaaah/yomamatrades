"""Daily OHLCV for a universe of US stocks, from Yahoo's chart endpoint (no key), cached as CSV.

usage: python3.12 -m mine.data [--range 10y] [--universe core|broad] [--tickers ...]
Cache: mine/cache/<TICKER>.csv   Universe list: mine/universe.txt (one ticker per line)
"""
from __future__ import annotations

import argparse
import json
import re
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

import pandas as pd

HERE = Path(__file__).parent
CACHE = HERE / "cache"
CACHE.mkdir(exist_ok=True)
UA = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128 Safari/537.36"

# the founder's nine + the momentum names their indicator sets are used on
CORE = ["MRNA", "PLTR", "NVDA", "INTC", "IREN", "ONDS", "SLNH", "BITF", "CLSK",
        "RIOT", "MARA", "CIFR", "WULF", "HUT", "BTDR", "CORZ", "APLD", "SOUN", "BBAI", "RGTI", "QUBT", "IONQ", "QBTS",
        "NBIS", "CRWV", "OKLO", "SMR", "NNE", "LUNR", "RKLB", "ASTS", "ACHR", "JOBY", "HIMS", "RDDT", "HOOD", "TSLA",
        "AMD", "MU", "SMCI", "ARM", "AVGO", "COIN", "MSTR", "SOFI", "UPST", "AFRM", "PLUG", "BE", "VRT", "CEG", "VST",
        "TLN", "AAPL", "MSFT", "AMZN", "GOOGL", "META", "NFLX", "CRM", "ORCL", "SHOP", "SQ", "PYPL", "UBER", "ABNB",
        "DKNG", "RBLX", "U", "SNOW", "NET", "DDOG", "CRWD", "ZS", "PANW", "MDB", "TTD", "ROKU", "ENPH", "FSLR", "RUN",
        "LCID", "RIVN", "NIO", "XPEV", "LI", "BABA", "PDD", "JD", "SE", "MELI", "NU", "GME", "AMC", "BB", "NOK",
        "SPY", "QQQ", "IWM"]


_SESSION: dict = {}


def _yahoo_session():
    """Yahoo needs a cookie + crumb pair now; anonymous bursts get 429."""
    if not _SESSION:
        import http.cookiejar
        jar = http.cookiejar.CookieJar()
        opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))
        opener.addheaders = [("User-Agent", UA)]
        try:
            opener.open("https://fc.yahoo.com", timeout=20)
        except urllib.error.HTTPError:
            pass                                                     # fc.yahoo.com answers 404 but sets the cookie
        crumb = opener.open("https://query2.finance.yahoo.com/v1/test/getcrumb", timeout=20).read().decode()
        _SESSION["opener"], _SESSION["crumb"] = opener, crumb
    return _SESSION["opener"], _SESSION["crumb"]


def _yahoo(ticker: str, rng: str) -> pd.DataFrame:
    opener, crumb = _yahoo_session()
    url = f"https://query2.finance.yahoo.com/v8/finance/chart/{ticker}?range={rng}&interval=1d&events=div,splits&crumb={crumb}"
    j = json.load(opener.open(url, timeout=30))
    res = j["chart"]["result"][0]
    q = res["indicators"]["quote"][0]
    return pd.DataFrame({"date": pd.to_datetime(res["timestamp"], unit="s").normalize(), "open": q["open"], "high": q["high"],
                         "low": q["low"], "close": q["close"], "volume": q["volume"]})


def _nasdaq(ticker: str) -> pd.DataFrame:
    rows = None
    for cls in ("stocks", "etf"):
        url = f"https://api.nasdaq.com/api/quote/{ticker}/historical?assetclass={cls}&fromdate=2015-01-01&todate=2030-01-01&limit=9999"
        req = urllib.request.Request(url, headers={"User-Agent": UA, "Accept": "application/json"})
        d = json.load(urllib.request.urlopen(req, timeout=30)).get("data")
        if d and d.get("tradesTable") and d["tradesTable"].get("rows"):
            rows = d["tradesTable"]["rows"]
            break
    if rows is None:
        raise ValueError("not found on Nasdaq")
    def num(s):
        try:
            return float(str(s).replace("$", "").replace(",", "").strip())
        except ValueError:
            return float("nan")
    df = pd.DataFrame({"date": pd.to_datetime([r["date"] for r in rows]), "open": [num(r["open"]) for r in rows], "high": [num(r["high"]) for r in rows],
                       "low": [num(r["low"]) for r in rows], "close": [num(r["close"]) for r in rows], "volume": [num(r["volume"]) for r in rows]})
    return df.sort_values("date")


def fetch(ticker: str, rng: str = "10y", force: bool = False) -> pd.DataFrame | None:
    f = CACHE / f"{ticker}.csv"
    if f.exists() and not force and (time.time() - f.stat().st_mtime) < 12 * 3600:
        return pd.read_csv(f, parse_dates=["date"])
    df = None
    for src in (_nasdaq, _yahoo):
        try:
            df = _yahoo(ticker, rng) if src is _yahoo else _nasdaq(ticker)
            break
        except Exception as e:  # noqa: BLE001 — try the other source
            print(f"  {ticker} via {src.__name__}: {type(e).__name__}: {str(e)[:70]}", file=sys.stderr)
            if src is _yahoo and "429" in str(e):
                time.sleep(10)
    if df is None:
        return None
    df = df.dropna()
    df = df[df["volume"] > 0].reset_index(drop=True)
    if len(df) < 260:
        return None
    df.to_csv(f, index=False)
    return df


def wikipedia_tickers() -> list[str]:
    """S&P 500 + NASDAQ-100 constituents from Wikipedia (plain HTTP)."""
    out = []
    for url, pat in [("https://en.wikipedia.org/wiki/List_of_S%26P_500_companies", r'<a rel="nofollow" class="external text" href="https://www\.nyse\.com/quote/[^"]+">([A-Z.\-]+)</a>|<a rel="nofollow" class="external text" href="https://www\.nasdaq\.com/market-activity/stocks/[^"]+">([A-Z.\-]+)</a>'),
                     ("https://en.wikipedia.org/wiki/Nasdaq-100", r'<td><a href="/wiki/[^"]+" title="[^"]+">[^<]+</a></td>\s*<td>([A-Z.\-]{1,6})</td>')]:
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            html = urllib.request.urlopen(req, timeout=30).read().decode("utf-8", "ignore")
            for m in re.finditer(pat, html):
                t = next(g for g in m.groups() if g)
                out.append(t.replace(".", "-"))
        except Exception as e:  # noqa: BLE001
            print(f"  wikipedia {url}: {e}", file=sys.stderr)
    return out


def universe(kind: str = "core") -> list[str]:
    f = HERE / "universe.txt"
    if kind == "broad":
        ticks = list(dict.fromkeys(CORE + wikipedia_tickers()))
        f.write_text("\n".join(ticks))
        return ticks
    return CORE


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--range", default="10y")
    ap.add_argument("--universe", default="core", choices=["core", "broad"])
    ap.add_argument("--tickers", nargs="*")
    args = ap.parse_args()
    ticks = args.tickers or universe(args.universe)
    ok = 0
    for i, t in enumerate(ticks):
        df = fetch(t, args.range)
        if df is not None:
            ok += 1
        if i % 25 == 0:
            print(f"{i}/{len(ticks)} fetched, {ok} usable", flush=True)
        time.sleep(0.7)
    print(f"done: {ok}/{len(ticks)} tickers with ≥ 1 year of daily bars in {CACHE}")


if __name__ == "__main__":
    main()
