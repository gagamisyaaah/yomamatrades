"""Jev over the public-script corpus (scout/raw/*.json): for each open-source script, typed judgments of which family its
core measurement belongs to, whether it is novel against our block vocabulary, and how plausible it is as a repeatable
effect. Output: bend/data/scout_blocks.csv/.md ranked by novelty × plausibility, for porting into blocks."""
import csv, glob, json, os, sys, time
sys.path.insert(0, ".")
for ln in open(".env"):
    if "=" in ln and not ln.startswith("#"):
        k, v = ln.strip().split("=", 1); os.environ.setdefault(k, v.strip('"'))
from typesafe_judge import ask
VOCAB = sorted(json.load(open("bend/evo/meaning.json")).keys())
man = {r["id"].replace(";", "_"): r for r in csv.DictReader(open("scout/manifest.csv"))}
rows = []
for f in sorted(glob.glob("scout/raw/*.json")):
    d = json.load(open(f)); src = d.get("source") or ""
    if len(src) < 200: continue
    pid = os.path.basename(f)[:-5]; m = man.get(pid, {})
    try:
        a = ask({"script_name": d.get("scriptName"), "likes": m.get("likes"), "source_head": src[:3500], "our_block_vocabulary": VOCAB},
                {"family": {"type": "choice", "instructions": "Which of our families does this script's CORE measurement belong to?", "criteria": {"STRUCTURE": "shape of the move: pivots, swings, ranges, levels, distances", "FORCE": "momentum, velocity, oscillators, flow, volume force", "ENERGY": "volatility, compression/expansion, volume regime, liquidity", "none": "drawing/utility/no measurement"}},
                 "novel": {"type": "noul", "instructions": "Does the core measurement compute something that is NOT already in our block vocabulary (a genuinely different formula, not a re-parameterisation)?"},
                 "plausible": {"type": "score", "instructions": "As a repeatable market effect on daily bars across thousands of stocks (not a drawing aid), how plausible is the core measurement?", "criteria": ["decorative or unfalsifiable", "weak", "plausible", "strong and well-founded"]},
                 "portable": {"type": "noul", "instructions": "Can the core measurement be computed from OHLCV of one symbol alone (no external symbols, no lookahead, no repainting)?"}})
        rows.append({"id": pid, "name": d.get("scriptName"), "likes": m.get("likes"), "family": a["family"].get("choice"), "novel": round(a["novel"].get("noul", 0), 2), "plausible": a["plausible"].get("score", a["plausible"]), "portable": round(a["portable"].get("noul", 0), 2), "url": m.get("url", "")})
        print(rows[-1]["name"], rows[-1]["family"], rows[-1]["novel"], rows[-1]["plausible"], flush=True)
    except Exception as e:
        print("ERR", pid, str(e)[:80], flush=True)
    time.sleep(0.3)
import pandas as pd
D = pd.DataFrame(rows)
D["plausible_n"] = pd.to_numeric(D["plausible"], errors="coerce")
D["rank"] = D.novel * D.portable * D.plausible_n.fillna(0)
D = D.sort_values("rank", ascending=False); D.to_csv("bend/data/scout_blocks.csv", index=False)
md = lambda d: "| " + " | ".join(map(str, d.columns)) + " |\n|" + "---|" * len(d.columns) + "\n" + "\n".join("| " + " | ".join(str(v) for v in r) + " |" for r in d.itertuples(index=False))  # noqa: E731
open("bend/data/scout_blocks.md", "w").write("# Jev over the public-script corpus: candidate blocks\n\n" + md(D[["name", "likes", "family", "novel", "plausible", "portable", "rank", "url"]]) + "\n")
print(D[["name", "family", "novel", "plausible", "portable", "rank"]].head(20).to_string(index=False))
