"""scout_wrap.py — turn scouted Pine v5/v6 indicators that expose alertcondition() into long-only
strategy twins the lab can backtest.

For every scout/src/<id>.pine that is //@version=5 or 6, declares indicator(), and has at least one
alertcondition(): parse each alertcondition (condition expression, title, message), ask TypeSafe one
choice question per alert (enter_long / exit_long / enter_short / exit_short / none), and emit
scout/wrapped/<id>.pine = original script with indicator() swapped for strategy() plus the entry/exit
block and the lab's verbatim metrics block. Scripts with no enter_long alert are skipped; shorts are
never traded. Classifications are cached in scout/wrapped/<id>.json (delete to re-ask).

Then every wrapped file is syntax-checked with pynescript and scout/wrapped/manifest.csv +
scout/README.md are written.

Needs TYPESAFE_API_KEY:  set -a; . ./.env; set +a; python3.12 scout_wrap.py
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from typesafe_judge import ask  # noqa: E402

HERE = Path(__file__).parent
SCOUT = HERE / "scout"
SRC = SCOUT / "src"
WRAPPED = SCOUT / "wrapped"

ROLES = ["enter_long", "exit_long", "enter_short", "exit_short", "none"]

STRATEGY_ARGS = ("initial_capital=10000, default_qty_type=strategy.cash, default_qty_value=10000, "
                 "commission_type=strategy.commission.percent, commission_value=0.05, "
                 "process_orders_on_close=false, calc_on_every_tick=false")

METRICS_BLOCK = """var float c0 = na
var int t0 = na
if na(c0)
    c0 := close
    t0 := time
bhPct = na(c0) ? na : (close / c0 - 1.0) * 100.0
netPct = strategy.netprofit / strategy.initial_capital * 100.0
weeks = na(t0) ? 1.0 : math.max(1.0, (time - t0) / 604800000.0)
tpw = strategy.closedtrades / weeks
plot(bhPct, "BH", display=display.status_line)
plot(netPct - bhPct, "Alpha", display=display.status_line)
plot(tpw, "TPW", display=display.status_line)
plot(0, "Bear", display=display.status_line)
plot(0, "Bull", display=display.status_line)
plot(0, "Short", display=display.status_line)
plot(0, "Exp", display=display.status_line)
"""
METRICS_NAMES = ("c0", "t0", "bhPct", "netPct", "weeks", "tpw")


def log(msg: str) -> None:
    print(msg, flush=True)


# ── tiny Pine lexer helpers (strings and // comments aware) ─────────────────────────────────────
def _skip_string(code: str, i: int) -> int:
    q = code[i]
    i += 1
    while i < len(code):
        c = code[i]
        if c == "\\":
            i += 2
            continue
        if c == q:
            return i + 1
        if c == "\n":          # Pine strings never span lines; treat as unterminated
            return i
        i += 1
    return i


def strip_comments(text: str) -> str:
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c in "\"'":
            j = _skip_string(text, i)
            out.append(text[i:j])
            i = j
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def _match_paren(code: str, j: int) -> int | None:
    depth, i, n = 0, j, len(code)
    while i < n:
        c = code[i]
        if c in "\"'":
            i = _skip_string(code, i)
            continue
        if code.startswith("//", i):
            k = code.find("\n", i)
            i = n if k < 0 else k
            continue
        if c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return None


def find_calls(code: str, name: str) -> list[tuple[int, int, int]]:
    """(start, end, open_paren) for every `name(` outside strings/comments, not preceded by '.' or an
    identifier character. `end` is the index just after the matching ')'."""
    spans, i, n, ln = [], 0, len(code), len(name)
    while i < n:
        c = code[i]
        if c in "\"'":
            i = _skip_string(code, i)
            continue
        if code.startswith("//", i):
            j = code.find("\n", i)
            i = n if j < 0 else j
            continue
        if code.startswith(name, i) and (i == 0 or not (code[i - 1].isalnum() or code[i - 1] in "_.")):
            j = i + ln
            while j < n and code[j] in " \t":
                j += 1
            if j < n and code[j] == "(":
                end = _match_paren(code, j)
                if end is None:
                    break
                spans.append((i, end, j))
                i = end
                continue
            i = j
            continue
        i += 1
    return spans


def split_args(argtext: str) -> list[str]:
    args, depth, i, n, start = [], 0, 0, len(argtext), 0
    while i < n:
        c = argtext[i]
        if c in "\"'":
            i = _skip_string(argtext, i)
            continue
        if c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
        elif c == "," and depth == 0:
            args.append(argtext[start:i])
            start = i + 1
        i += 1
    tail = argtext[start:]
    if tail.strip() or args:
        args.append(tail)
    return args


_KW = re.compile(r"\s*([A-Za-z_]\w*)\s*=(?![=>])", re.S)


def parse_call_args(code: str, span: tuple[int, int, int]) -> tuple[list[str], dict[str, str]]:
    """Positional list + keyword dict of a call, comments stripped and whitespace collapsed."""
    _, end, op = span
    inner = strip_comments(code[op + 1:end - 1])
    positional, keywords = [], {}
    for raw in split_args(inner):
        m = _KW.match(raw)
        text = re.sub(r"\s+", " ", raw).strip()
        if m:
            keywords[m.group(1)] = re.sub(r"\s+", " ", raw[m.end():]).strip()
        elif text:
            positional.append(text)
    return positional, keywords


def unquote(s: str | None) -> str | None:
    if s is None:
        return None
    m = re.fullmatch(r"\s*([\"'])(.*)\1\s*", s, re.S)
    if not m:
        return s
    return re.sub(r"\\(.)", r"\1", m.group(2))


def at_line_start(code: str, i: int) -> bool:
    k = code.rfind("\n", 0, i) + 1
    return code[k:i].strip() == ""


# ── script analysis ─────────────────────────────────────────────────────────────────────────────
def analyse(code: str) -> dict:
    """Returns {version, kind, decl: span|None, decl_args, alerts:[{condition,title,message}]}."""
    m = re.search(r"^//\s*@version\s*=\s*(\d+)", code, re.M)
    version = int(m.group(1)) if m else None
    kind = None
    decl = None
    for name in ("strategy", "indicator", "library", "study"):
        spans = [s for s in find_calls(code, name) if at_line_start(code, s[0])]
        if spans:
            kind, decl = name, spans[0]
            break
    alerts = []
    for span in find_calls(code, "alertcondition"):
        pos, kw = parse_call_args(code, span)
        cond = kw.get("condition") or (pos[0] if pos else None)
        if not cond:
            continue
        title = kw.get("title") or (pos[1] if len(pos) > 1 else None)
        message = kw.get("message") or (pos[2] if len(pos) > 2 else None)
        line_prefix = code[code.rfind("\n", 0, span[0]) + 1:span[0]]
        alerts.append({"condition": cond, "title": unquote(title) or "", "message": unquote(message) or "",
                       "local_scope": line_prefix != ""})   # indented => inside if/function block
    return {"version": version, "kind": kind, "decl": decl, "alerts": alerts}


# ── TypeSafe classification ─────────────────────────────────────────────────────────────────────
CRITERIA = {
    "enter_long": "bullish: buy, long, breakout up, bull cross, oversold bounce, uptrend start, support hold",
    "exit_long": "bearish: sell, breakdown, bear cross, overbought, downtrend start — and the script reads as "
                 "long-only style (it never explicitly talks about shorting)",
    "enter_short": "bearish and the alert or script explicitly says short / short entry / go short",
    "exit_short": "explicitly covers or closes a short position",
    "none": "no single direction: fires for either side ('Bullish or Bearish', 'Buy or Sell', 'Any signal'), "
            "or informational (squeeze on/off, new level drawn, volatility regime, volume spike)",
}


DIRECTION = {
    "bullish": "only an upward / buy / long case",
    "bearish": "only a downward / sell / short case",
    "either_side": "one alert that fires for both sides ('Bullish or Bearish', 'Buy or Sell', 'Any signal', "
                   "'direction change')",
    "no_direction": "no market direction: a trade-management event (take profit, stop loss, timeout, invalidated) "
                    "or informational (volatility regime, new level, squeeze)",
}


def resolve_role(role_raw: str | None, role_conf: float | None, direction: str | None) -> str:
    """Compose the two judgments. `direction` is the sharp axis (it separates 'Buy or Sell' from 'TP hit'
    from 'Bullish shift' at ~1.0 confidence); `role` only refines it — the role question alone flipped a
    coin on 'Bullish Trend Shift' (0.50 enter_long / 0.49 exit_long)."""
    if direction == "either_side":
        return "none"                                   # fires both ways: neither entry nor exit
    if role_raw in ("exit_long", "exit_short"):
        if (role_conf or 0.0) >= 0.7 or direction == "no_direction":
            return role_raw                             # TP / SL / timeout exits carry no market direction
        return {"bullish": "enter_long", "bearish": "exit_long"}.get(direction or "", "none")
    if role_raw in ("enter_long", "enter_short"):
        if direction == "bullish":
            return "enter_long"
        if direction == "bearish":
            return "enter_short" if role_raw == "enter_short" else "exit_long"
        return "none"                                   # an entry needs a direction
    return "none"


def classify(script_name: str, alerts: list[dict]) -> list[dict]:
    titles = [a["title"] for a in alerts]
    out = []
    for i, a in enumerate(alerts):
        state = {
            "script_name": script_name,
            "alert_title": a["title"],
            "alert_message": a["message"][:600],
            "condition_snippet": a["condition"][:300],
            "other_alert_titles_in_script": [t for j, t in enumerate(titles) if j != i][:12],
        }
        q = {
            "role": {
                "type": "choice",
                "instructions": "This alert comes from a TradingView indicator that is being wrapped into a long-only "
                                "strategy. Which role should the alert's condition play?",
                "criteria": CRITERIA,
            },
            "direction": {
                "type": "choice",
                "instructions": "Which market direction does this alert announce?",
                "criteria": DIRECTION,
            },
        }
        ans = ask(state, q)
        role, direction = ans["role"], ans["direction"]
        out.append({**a, "role": resolve_role(role.get("choice"), role.get("confidence"), direction.get("choice")),
                    "role_raw": role.get("choice"), "confidence": role.get("confidence"),
                    "probabilities": role.get("probabilities"),
                    "direction": direction.get("choice"), "direction_confidence": direction.get("confidence")})
    return out


# ── emitter ─────────────────────────────────────────────────────────────────────────────────────
def build_strategy_decl(code: str, decl: tuple[int, int, int]) -> str:
    pos, kw = parse_call_args(code, decl)
    title = kw.get("title") or (pos[0] if pos else '"untitled"')
    short = kw.get("shorttitle") or (pos[1] if len(pos) > 1 else None)
    overlay = kw.get("overlay") or (pos[2] if len(pos) > 2 else None)
    parts = [f"title={title}"]
    if short:
        parts.append(f"shorttitle={short}")
    if overlay:
        parts.append(f"overlay={overlay}")
    parts.append(STRATEGY_ARGS)
    return "strategy(" + ", ".join(parts) + ")"


def or_join(conds: list[str]) -> str:
    return " or ".join(f"({c})" for c in conds) if conds else "false"


def emit(code: str, decl: tuple[int, int, int], alerts: list[dict]) -> str:
    by_role = {r: [a["condition"] for a in alerts if a["role"] == r] for r in ROLES}
    start, end, _ = decl
    body = code[:start] + build_strategy_decl(code, decl) + code[end:]
    if not body.endswith("\n"):
        body += "\n"
    tail = (
        "\n// ── auto-wrapped by scout_wrap.py ──\n"
        f"__enterL = {or_join(by_role['enter_long'])}\n"
        f"__exitL = {or_join(by_role['exit_long'])}\n"
        f"__enterS = {or_join(by_role['enter_short'])}\n"
        f"__exitS = {or_join(by_role['exit_short'])}\n"
        "if __enterL and strategy.position_size <= 0\n"
        '    strategy.entry("L", strategy.long)\n'
        "if __exitL and strategy.position_size > 0\n"
        '    strategy.close("L")\n'
        + METRICS_BLOCK
    )
    return body + tail


def metrics_name_collisions(code: str) -> list[str]:
    names = "|".join(METRICS_NAMES)
    return sorted(set(re.findall(rf"^\s*(?:var\s+|varip\s+)?(?:[A-Za-z_]\w*\s+)?({names})\s*(?:=|:=)(?!=)", code, re.M)))


# ── syntax check ────────────────────────────────────────────────────────────────────────────────
class SyntaxChecker:
    """One warm pine_syntax.py process fed paths on stdin; see that file for why sequential + warm."""

    def __init__(self, timeout: float):
        self.proc = subprocess.Popen([sys.executable, str(HERE / "pine_syntax.py"), "--timeout", str(int(timeout))],
                                     stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)

    def check(self, path: Path) -> tuple[str, str]:
        try:
            self.proc.stdin.write(f"{path}\n")
            self.proc.stdin.flush()
            line = self.proc.stdout.readline()
        except (BrokenPipeError, OSError):
            line = ""
        if not line:
            return "unchecked", "pine_syntax.py died"
        status, _, note = line.rstrip("\n").split("\t", 2)
        return status, note[:200]

    def close(self) -> None:
        try:
            self.proc.stdin.close()
        except OSError:
            pass
        self.proc.wait()


def syntax_check_all(rows: list[dict], timeout: float) -> None:
    """Fill rows[*]['syntax'|'note']. Smallest files go first so the parser cache warms cheaply; a
    failing wrapped file's original is checked too, so a parser gap is not blamed on the wrapper."""
    checker = SyntaxChecker(timeout)
    for r in sorted(rows, key=lambda r: (HERE / r["file"]).stat().st_size):
        r["syntax"], err = checker.check(HERE / r["file"])
        blame = ""
        if r["syntax"] == "fail":                       # a timed-out twin's original would time out too
            orig_status, _ = checker.check(r["_orig"])
            blame = " [wrapper]" if orig_status == "pass" else f" [original also {orig_status}]"
        if r["syntax"] != "pass":
            r["note"] = "; ".join(x for x in (err + blame, r["note"]) if x)
        log(f"  syntax {r['syntax']:9s} {r['name'][:48]:48s} {err[:70]}{blame}")
    checker.close()
    for r in rows:
        r.pop("_orig", None)


# ── README ──────────────────────────────────────────────────────────────────────────────────────
def write_readme(stats: dict, wrapped_rows: list[dict], skipped: list[dict], meta: dict[str, dict]) -> None:
    lines = ["# scout — community-library scout + strategy twins", ""]
    lines += ["## What was scouted", "",
              "Search endpoint: `https://www.tradingview.com/pubscripts-suggest-json/?search=<term>` (the JSON the "
              "site's own search box uses; 50 cards per page, page 2 via its `next` offset cursor). The HTML "
              "listing `/scripts/?text=<term>` was fetched too but ignored: it embeds the same 24 hot-feed cards "
              "for every term and for `&page=2`, so the term is only applied client-side.", "",
              "| term | cards (2 pages) |", "|---|---|"]
    lines += [f"| {t} | {n} |" for t, n in stats.get("terms", {}).items()]
    lines += ["", f"Cards found: **{stats.get('cards_found')}**, unique ids: **{stats.get('unique')}**, "
                  f"open-source: **{stats.get('open_source')}**, sources fetched (top {stats.get('top_targets')} "
                  f"open-source by likes): **{stats.get('fetched')}** → `scout/src/<id>.pine`, facade JSON cached in "
                  "`scout/raw/<id>.json` (file names use `PUB_…`, the `;` of the PUB id is replaced by `_`).", ""]
    lines += ["## How wrapping works (`scout_wrap.py`)", "",
              "1. Keep only `//@version=5` / `6` scripts that declare `indicator()` and contain `alertcondition()`; "
              "scripts that are already a `strategy()` (or a `library()`) are skipped.",
              "2. Each `alertcondition(condition, title=…, message=…)` (keyword or positional) is parsed with a "
              "string/comment-aware tokenizer; the condition expression text is kept verbatim.",
              "3. TypeSafe (jev) judges each alert in one call from the script name, alert title, message, condition "
              "snippet and the other alert titles in the script: a `role` choice (enter_long / exit_long / "
              "enter_short / exit_short / none) and a `direction` choice (bullish / bearish / either_side / "
              "no_direction). `direction` is the sharp axis (~1.0 confidence on 'Buy or Sell', 'TP1 Hit', "
              "'Bullish shift'); `role` refines it: either_side → none; a confident exit (≥0.7) or a "
              "no-direction exit (TP/SL/timeout) stays an exit; an entry must agree with its direction, else none. "
              "Both raw answers, probabilities and the resolved role are cached in `scout/wrapped/<id>.json`.",
              "4. `indicator(...)` becomes `strategy(title=…, shorttitle=…, overlay=…, initial_capital=10000, "
              "default_qty_type=strategy.cash, default_qty_value=10000, commission 0.05 %, "
              "process_orders_on_close=false, calc_on_every_tick=false)`; every other indicator() argument is dropped.",
              "5. Appended at the end: `__enterL` / `__exitL` (OR of the classified conditions, `false` when none), "
              "long-only `strategy.entry(\"L\")` / `strategy.close(\"L\")`, and the lab's verbatim metrics block "
              "(BH, Alpha, TPW, Bear, Bull, Short, Exp status-line plots). Shorts are never traded; a script with "
              "no enter_long alert is skipped.",
              "6. `pynescript` parses every wrapped file (column `syntax`: pass / fail / timeout); the original is "
              "parsed too when the wrapped one fails, so a parser limitation is not blamed on the wrapper. "
              "`pine_syntax.py` keeps one warm ANTLR process (its DFA cache makes a re-parse instant but every "
              "novel construct costs ~0.3 s), feeds files smallest-first and enforces a per-file time limit.", ""]
    lines += ["## Limits", "",
              "- alertcondition conditions are re-used verbatim at global scope; a condition that references a "
              "variable declared inside a local block (`if`, function, method) will not compile on TradingView.",
              "- The metrics block declares `c0 t0 bhPct netPct weeks tpw`; a script that already uses one of these "
              "names fails to compile (flagged in `note`).",
              "- Dropping `max_lines_count` / `max_labels_count` / `max_boxes_count` from the declaration only "
              "changes how many drawings stay on the chart, not the signals.",
              "- The alert classification is a judgment from title/message text; an alert that fires on both "
              "directions (\"any signal\") is `none` and contributes nothing.",
              "- `pynescript` is a parser only: a `pass` means the file lexes/parses, not that it compiles; a `fail` "
              "on a file whose original also fails is a parser gap, not a wrapper bug; a `timeout` is an unchecked "
              "file (its ANTLR grammar needs minutes per KB of novel code on CPython), not a failure.",
              "- Search results are TradingView's relevance ranking, two pages per term; likes are the "
              "`agreeCount` at scouting time.", ""]
    lines += ["## Wrapped scripts (sorted by likes)", "",
              "| likes | script | author | alerts | enter_long | exit_long | syntax | file |", "|---|---|---|---|---|---|---|---|"]
    for r in sorted(wrapped_rows, key=lambda r: -int(r["likes"])):
        m = meta.get(r["id"], {})
        lines.append(f"| {r['likes']} | {r['name']} | {m.get('author', '')} | {r['n_alerts']} | {r['enter_long_count']} | "
                     f"{r['exit_long_count']} | {r['syntax']} | `{Path(r['file']).name}` |")
    lines += ["", "## Skipped", "", "| script | reason |", "|---|---|"]
    lines += [f"| {s['name']} | {s['reason']} |" for s in skipped]
    (SCOUT / "README.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


# ── main ────────────────────────────────────────────────────────────────────────────────────────
def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--syntax-timeout", type=float, default=600.0, help="seconds per file for pynescript")
    ap.add_argument("--skip-syntax", action="store_true", help="classify + emit only; syntax column stays 'unchecked'")
    args = ap.parse_args()
    WRAPPED.mkdir(parents=True, exist_ok=True)
    meta: dict[str, dict] = {}
    with (SCOUT / "manifest.csv").open(encoding="utf-8") as f:
        for row in csv.DictReader(f):
            meta[row["id"]] = row
    stats = json.loads((SCOUT / "scout_stats.json").read_text(encoding="utf-8")) if (SCOUT / "scout_stats.json").exists() else {}

    rows, skipped = [], []
    n_v56_alerts = 0
    for path in sorted(SRC.glob("*.pine")):
        fid = path.stem
        sid = fid.replace("PUB_", "PUB;", 1)
        info = meta.get(sid, {})
        name = info.get("name") or fid
        likes = int(info.get("likes") or 0)
        try:
            code = path.read_text(encoding="utf-8")
            a = analyse(code)
            if a["version"] not in (5, 6):
                v = f"pine v{a['version']}" if a["version"] else "no //@version line (Pine v1)"
                skipped.append({"id": sid, "name": name, "reason": v})
                continue
            if a["kind"] == "strategy":
                skipped.append({"id": sid, "name": name, "reason": "already a strategy"})
                continue
            if a["kind"] != "indicator" or a["decl"] is None:
                skipped.append({"id": sid, "name": name, "reason": f"no indicator() declaration ({a['kind']})"})
                continue
            if not a["alerts"]:
                skipped.append({"id": sid, "name": name, "reason": "no alertcondition()"})
                continue
            n_v56_alerts += 1

            jpath = WRAPPED / f"{fid}.json"
            cached = json.loads(jpath.read_text(encoding="utf-8")) if jpath.exists() else None
            if cached and len(cached.get("alerts", [])) == len(a["alerts"]) and all("direction" in x for x in cached["alerts"]):
                alerts = cached["alerts"]
                for x in alerts:                        # re-derive: the rule may change without re-asking
                    x["role"] = resolve_role(x["role_raw"], x["confidence"], x["direction"])
            else:
                alerts = classify(name, a["alerts"])
            collisions = metrics_name_collisions(code)
            jpath.write_text(json.dumps({"id": sid, "name": name, "likes": likes, "version": a["version"],
                                         "alerts": alerts, "metrics_name_collisions": collisions},
                                        indent=1, ensure_ascii=False), encoding="utf-8")
            roles = [x["role"] for x in alerts]
            log(f"{name[:48]:48s} {likes:>6d}  alerts={len(alerts)}  " + " ".join(f"{r}:{roles.count(r)}" for r in ROLES if roles.count(r)))
            if roles.count("enter_long") == 0:
                skipped.append({"id": sid, "name": name, "reason": "no enter_long alert"})
                continue

            out = emit(code, a["decl"], alerts)
            opath = WRAPPED / f"{fid}.pine"
            opath.write_text(out, encoding="utf-8")
            notes = []
            if collisions:
                notes.append("metrics name collision: " + ",".join(collisions))
            if any(x.get("local_scope") for x in alerts):
                notes.append("an alertcondition sits in an indented block")
            rows.append({"id": sid, "name": name, "likes": likes, "n_alerts": len(alerts),
                         "enter_long_count": roles.count("enter_long"), "exit_long_count": roles.count("exit_long"),
                         "file": str(opath.relative_to(HERE)), "syntax": "unchecked", "note": "; ".join(notes),
                         "_orig": path})
        except Exception as e:  # noqa: BLE001 — never abort the batch on one script
            log(f"  {name[:48]}: FAILED {type(e).__name__}: {str(e)[:200]}")
            skipped.append({"id": sid, "name": name, "reason": f"error: {type(e).__name__}: {str(e)[:120]}"})

    if args.skip_syntax:
        for r in rows:
            r.pop("_orig", None)
    else:
        log(f"\nsyntax-checking {len(rows)} wrapped files (one warm pynescript process, {args.syntax_timeout:.0f}s each)")
        syntax_check_all(rows, args.syntax_timeout)
    rows.sort(key=lambda r: -r["likes"])
    with (WRAPPED / "manifest.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=["id", "name", "likes", "n_alerts", "enter_long_count", "exit_long_count",
                                          "file", "syntax", "note"])
        w.writeheader()
        w.writerows(rows)
    write_readme(stats, rows, skipped, meta)
    syntax = " ".join(f"{s}: {sum(r['syntax'] == s for r in rows)}" for s in ("pass", "fail", "timeout", "unchecked"))
    log(f"\nsources: {len(list(SRC.glob('*.pine')))}  v5/v6 with alerts: {n_v56_alerts}  wrapped: {len(rows)}  "
        f"syntax {syntax}  skipped: {len(skipped)}")
    for s in skipped:
        log(f"  skip {s['name'][:48]:48s} {s['reason']}")


if __name__ == "__main__":
    main()
