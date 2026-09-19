"""Jev scores the longlist (bend/evo/pro_longlist.txt): usefulness, portability, family, documented. → bend/data/models_jev3.md/.csv"""
import os, sys, time, json
sys.path.insert(0, ".")
for ln in open(".env"):
    if "=" in ln and not ln.startswith("#"):
        k, v = ln.strip().split("=", 1); os.environ.setdefault(k, v.strip('"'))
from typesafe_judge import ask
import pandas as pd
rows = []
for ln in open("bend/evo/pro_longlist.txt"):
    if ln.startswith("#") or "|" not in ln: continue
    era, author, name, desc = [x.strip() for x in ln.split("|", 3)]; dom = f"{era} {author}"
    try:
        a = ask({"domain": dom, "model": name, "measurement": desc, "setting": "daily bars, one symbol's OHLCV, thousands of US stocks; entries scored by a +2/−4 ATR bracket over 20 bars against random entries matched by year and volatility"},
                {"useful": {"type": "score", "instructions": "How useful is this measurement as a repeatable entry-timing signal in this setting?", "criteria": ["decorative analogy", "weak", "useful", "a known quant workhorse"]},
                 "portable": {"type": "noul", "instructions": "Can this measurement be computed from one symbol's OHLCV history alone, without lookahead?"},
                 "family": {"type": "choice", "instructions": "Which family does the measurement belong to?", "criteria": {"STRUCTURE": "shape, levels, swings", "FORCE": "momentum, flow, velocity", "ENERGY": "volatility, volume regime, liquidity"}},
                 "documented": {"type": "noul", "instructions": "Is there published quantitative-finance evidence that measurements of this kind carry information about future returns?"}})
        rows.append({"domain": dom, "model": name, "measurement": desc, "useful": a["useful"].get("score"), "portable": round(a["portable"].get("noul", 0), 2), "family": a["family"].get("choice"), "documented": round(a["documented"].get("noul", 0), 2)})
        if len(rows) % 25 == 0: print(len(rows), flush=True)
    except Exception as e:
        print("ERR", name, str(e)[:80], flush=True)
    time.sleep(0.15)
D = pd.DataFrame(rows); D["useful_n"] = pd.to_numeric(D.useful, errors="coerce"); D["rank"] = D.useful_n.fillna(0) * D.portable * (0.5 + 0.5 * D.documented)
D = D.sort_values("rank", ascending=False); D.to_csv("bend/data/models_jev3.csv", index=False)
md = lambda d: "| " + " | ".join(map(str, d.columns)) + " |\n|" + "---|" * len(d.columns) + "\n" + "\n".join("| " + " | ".join(str(v) for v in r) + " |" for r in d.itertuples(index=False))  # noqa: E731
open("bend/data/models_jev3.md", "w").write(f"# Jev over {len(D)} professional measurements 1962-2020s: useful × portable × (½ + ½ documented)\n\n" + md(D[["domain", "model", "family", "useful", "portable", "documented", "rank", "measurement"]]) + "\n")
pd.set_option("display.width", 220); print(D[["domain", "model", "family", "useful", "portable", "documented", "rank"]].head(60).to_string(index=False))
print("\nby domain (mean rank):"); print(D.groupby("domain")["rank"].mean().sort_values(ascending=False).round(2).to_string())
