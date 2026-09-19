"""TREE OF LIFE — the terminal between the trader and the black box.

TradingView is the main program (the three super indicators live there as Pine); this screen shows what they say today
across the whole universe, why (which building blocks hold, with their values), the evidence (the family's evolution
record and holdout), the plan (entry, target, stop, time stop, size), and Jev's typed verdicts on the selected name.

keys: ↑/↓ select · ←/→ lane (TREE / RUNNER / BASE-HIT / STRUCTURE / FORCE / ENERGY) · j ask Jev about the selected name · r rescan
      (runs today.py) · s size (cycle 0.5 % / 1 % / 2 % per ATR) · q quit
usage: python3.12 bend/evo/tree_of_life.py [--capital 100000]"""
from __future__ import annotations

import argparse
import curses
import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT))

LOGO = r"""
              &&&  &&&&&&                     T R E E   O F   L I F E   ·   RUNNER lane + BASE-HIT lane
           &&&&&&&&&&&&&&&&&&&                three bred indicators · every US stock · today
         &&&&&&&&&&&&&&&&&&&&&&&&
        &&&&&&&&&&&&&&&&&&&&&&&&&&&           STRUCTURE  the shape of the move
          &&&&&&&&&&&&\ /&&&&&&&&&            FORCE      the momentum of the move
             &&&&&&&&&&|&&&&&&&&              ENERGY     the volatility state
                     \ | /
                      \|/                     TradingView is the chart · Jev judges · you place the trade
                       |
                    ___|___
"""
TABS = ["TREE", "RUNNER", "BASE-HIT", "STRUCTURE", "FORCE", "ENERGY"]
SIZES = [0.005, 0.01, 0.02]


def load():
    sup = json.load(open(ROOT / "bend/data/supers.json"))
    today = json.load(open(ROOT / "bend/data/today.json")) if (ROOT / "bend/data/today.json").exists() else {"date": "—", "scanned": 0, "candidates": []}
    return sup, today


def jev_verdict(c: dict, sup: dict) -> dict:
    """typed judgments only: a quality score, a regime-fit choice, an event-risk probability — never prose."""
    for ln in (ROOT / ".env").read_text().splitlines():
        if "=" in ln and not ln.startswith("#"):
            k, v = ln.split("=", 1); os.environ.setdefault(k.strip(), v.strip().strip('"'))
    from typesafe_judge import ask
    ev = sup[c["family"]].get("evidence", {})
    state = {"ticker": c["ticker"], "family": c["family"], "rule": sup[c["family"]]["rule"], "blocks_now": c["blocks"], "context": c["context"], "plan": c["plan"], "family_evidence": ev, "guard": c.get("guard")}
    return ask(state, {
        "setup_quality": {"type": "score", "instructions": "How cleanly does this name express its family's rule right now (block values near their thresholds are weak, deep inside them are strong)?", "criteria": ["marginal", "ordinary", "clean", "textbook"]},
        "regime_fit": {"type": "choice", "instructions": "Given the market guard and the name's own structure, which is the better handling?", "criteria": {"take_full": "take at the plan size", "take_half": "take at half size", "wait_pullback": "wait for a pullback toward the last pivot low", "skip": "skip"}},
        "event_risk": {"type": "noul", "instructions": "Is there a visible reason (earnings within 7 days, thin liquidity, a data glitch such as a split) to distrust this signal?"},
    })


def draw(stdscr, sup, today, a):
    curses.curs_set(0); curses.use_default_colors()
    curses.init_pair(1, curses.COLOR_GREEN, -1); curses.init_pair(2, curses.COLOR_YELLOW, -1); curses.init_pair(3, curses.COLOR_CYAN, -1); curses.init_pair(4, curses.COLOR_RED, -1); curses.init_pair(5, curses.COLOR_MAGENTA, -1)
    fam_col = {"RUNNER": 1, "STRUCTURE": 3, "FORCE": 2, "ENERGY": 5}
    tab = 0; sel = 0; size_i = 1; verdicts = {}; msg = ""
    while True:
        stdscr.erase(); H, W = stdscr.getmaxyx()
        for i, ln in enumerate(LOGO.strip("\n").split("\n")):
            stdscr.addnstr(i, 0, ln, W - 1, curses.color_pair(1) if i < 10 else 0)
        top = LOGO.count("\n") + 1
        lane = TABS[tab]
        def in_lane(c):
            runner_ok = c["family"] == "RUNNER" and c.get("in_universe", True) and (c.get("cap_B") or 0) >= sup["RUNNER"]["universe"]["cap_min"] / 1e9
            liquid = (c["context"].get("dvol20_M") or 0) >= 3.0
            if lane == "TREE":   # exactly what DRAGON TREE shows: runner outside a long downtrend, washout inside one
                return (runner_ok and not c.get("downtrend")) or (c["family"] == "STRUCTURE" and liquid and c.get("downtrend"))
            if lane == "RUNNER":
                return runner_ok and not c.get("downtrend")
            if lane == "BASE-HIT":
                return c["family"] != "RUNNER" and liquid
            return c["family"] == lane and liquid
        cands = [c for c in today["candidates"] if in_lane(c)]
        cands.sort(key=lambda c: (not c["new"], -(c.get("break_vol_ratio") or 0) if lane in ("RUNNER", "TREE") else -c["atr_pct"]))
        sel = max(0, min(sel, len(cands) - 1))
        g = today.get("guard"); gtxt = "guard: not checked (rescan with quotes)" if not g else (f"guard ON  SPY {g['spy']:.0f} > 200d {g['sma200']:.0f}" if g["on"] else f"guard OFF SPY {g['spy']:.0f} < 200d {g['sma200']:.0f}")
        stdscr.addnstr(top, 0, f" {today['date']} · scanned {today['scanned']} · {gtxt} · size {SIZES[size_i]:.1%}/ATR of ${a.capital:,.0f} · " + "  ".join(("[" + t + "]") if i == tab else t for i, t in enumerate(TABS)), W - 1, curses.A_BOLD)
        if lane in ("RUNNER", "TREE"):
            stdscr.addnstr(top + 1, 0, f" {'ticker':7s} {'lane':8s} {'new':4s} {'close':>9s} {'ATR%':>6s} {'brk vol':>8s} {'bars':>5s} {'$M/day':>7s} {'shares':>7s}", W - 1, curses.A_UNDERLINE)
        else:
            stdscr.addnstr(top + 1, 0, f" {'ticker':7s} {'family':10s} {'new':4s} {'close':>9s} {'ATR%':>6s} {'target':>9s} {'stop':>9s} {'shares':>7s}", W - 1, curses.A_UNDERLINE)
        lw = 70; rows = H - top - 4
        for i, c in enumerate(cands[:rows]):
            shares = int(a.capital * SIZES[size_i] / c["atr"]) if c["atr"] else 0
            if lane in ("RUNNER", "TREE"):
                lane_name = "runner" if c["family"] == "RUNNER" else "washout"
                ln = f" {c['ticker']:7s} {lane_name:8s} {'NEW' if c['new'] else '':4s} {c['close']:9.2f} {c['atr_pct']:6.2f} {c.get('break_vol_ratio') or 0:8.2f} {c.get('bars_since_break', 0):5d} {(c.get('dvol20_M') or c['context'].get('dvol20_M') or 0):7.1f} {shares:7d}"
            else:
                ln = f" {c['ticker']:7s} {c['family']:10s} {'NEW' if c['new'] else '':4s} {c['close']:9.2f} {c['atr_pct']:6.2f} {c['plan']['target'] or 0:9.2f} {float(c['plan']['stop']) if isinstance(c['plan']['stop'], (int, float)) else 0:9.2f} {shares:7d}"
            stdscr.addnstr(top + 2 + i, 0, ln, lw, (curses.A_REVERSE if i == sel else 0) | curses.color_pair(fam_col.get(c["family"], 0)))
        if not cands:
            stdscr.addnstr(top + 2, 1, "no name meets a super indicator today — nothing to do is a valid day", W - 2, curses.color_pair(2))
        # detail
        x0 = lw + 2
        if cands:
            c = cands[sel]; s = sup[c["family"]]; y = top + 1
            def put(text, attr=0):
                nonlocal y
                if y < H - 1:
                    stdscr.addnstr(y, x0, text, W - x0 - 1, attr); y += 1
            put(f"{c['ticker']} · {s['name']}", curses.A_BOLD | curses.color_pair(fam_col.get(c["family"], 0)))
            put("WHY (every building block of the indicator holds):")
            for b in c["blocks"]:
                put(f"  {b['block']:14s} {b['value']:>10.3f}  {b['op']} {b['thr']:.4g}   {s.get('meaning', {}).get(b['block'], '')}")
            ctx = c["context"]
            put(f"  shape: 52w {ctx['hi252']}% · struct {ctx['struct'] if ctx['struct'] is not None else '—'} · pullback {ctx['retrace'] if ctx['retrace'] is not None else '—'} · efficiency20 {ctx['eff20'] if ctx['eff20'] is not None else '—'} · range20/50 {ctx['rng20'] if ctx['rng20'] is not None else '—'} · vol/20d {ctx['volr20']} · ${ctx['dvol20_M']}M/day")
            put("EVIDENCE (evolution record, excess over random entries with the same exit):")
            for k, v in s.get("evidence", {}).items():
                put(f"  {k:26s} {v}")
            put("PLAN:")
            put(f"  entry {c['plan']['entry']}")
            if c["family"] == "RUNNER":
                put(f"  exit: {c['plan']['stop']}   ·   hole level {c.get('hole_level')}   ·   break {c.get('bars_since_break')} bars ago on {c.get('break_vol_ratio')}× volume")
                put("  a runner: 42–46 % winners, big average, single-name drawdown ~40 % → many names, never one chart; skip if $M/day is thin for your size")
            else:
                put(f"  target {c['plan']['target']} (+2 ATR)   stop {c['plan']['stop']} (−4 ATR)   time stop {c['plan']['time_stop_bars']} bars   exit early if the family's rule breaks (a lower pivot low / range expansion against you)")
            put(f"  size: {int(a.capital * SIZES[size_i] / c['atr']) if c['atr'] else 0} shares = a 1-ATR move is {SIZES[size_i]:.1%} of capital")
            v = verdicts.get(c["ticker"] + c["family"])
            put("JEV:" if v else "JEV: press j for the typed verdict (setup quality · take full / half / wait / skip · event risk)", curses.A_BOLD)
            if v:
                sq, rf, er = v.get("setup_quality", {}), v.get("regime_fit", {}), v.get("event_risk", {})
                put(f"  setup quality: {sq.get('score', sq)}   handling: {rf.get('choice')} ({rf.get('confidence', 0):.0%})   event risk: {er.get('noul', 0):.0%}")
        stdscr.addnstr(H - 1, 0, (msg or " ↑↓ select  ←→ family  j Jev  r rescan  s size  q quit")[: W - 1], W - 1, curses.A_DIM)
        stdscr.refresh(); k = stdscr.getch()
        if k in (ord("q"), 27):
            break
        elif k == curses.KEY_UP:
            sel -= 1
        elif k == curses.KEY_DOWN:
            sel += 1
        elif k == curses.KEY_LEFT:
            tab = (tab - 1) % len(TABS); sel = 0
        elif k == curses.KEY_RIGHT:
            tab = (tab + 1) % len(TABS); sel = 0
        elif k == ord("s"):
            size_i = (size_i + 1) % len(SIZES)
        elif k == ord("j") and cands:
            c = cands[sel]; msg = f"asking Jev about {c['ticker']}…"; stdscr.addnstr(H - 1, 0, msg, W - 1); stdscr.refresh()
            try:
                c["guard"] = today.get("guard"); verdicts[c["ticker"] + c["family"]] = jev_verdict(c, sup); msg = ""
            except Exception as e:  # noqa: BLE001
                msg = f"Jev unavailable: {str(e)[:60]}"
        elif k == ord("r"):
            msg = "rescanning (2–3 min)…"; stdscr.addnstr(H - 1, 0, msg, W - 1); stdscr.refresh()
            subprocess.run([sys.executable, str(HERE / "today.py"), "--quotes"], capture_output=True)
            sup, today = load(); msg = ""


def dump(sup, today, a):
    """--dump: print both lanes as text (for a report or a non-interactive terminal)"""
    g = today.get("guard"); print(f"TREE OF LIFE · {today['date']} · scanned {today['scanned']} · " + ("guard ON" if g and g["on"] else "guard OFF" if g else "guard not checked") + (f" (SPY {g['spy']:.0f} vs 200d {g['sma200']:.0f})" if g else ""))
    run = [c for c in today["candidates"] if c["family"] == "RUNNER" and c.get("in_universe") and (c.get("cap_B") or 0) >= sup["RUNNER"]["universe"]["cap_min"] / 1e9 and not c.get("downtrend")]
    wash_dn = [c for c in today["candidates"] if c["family"] == "STRUCTURE" and c.get("downtrend") and (c["context"].get("dvol20_M") or 0) >= 3.0]
    print(f"\nTREE view = runner outside a long downtrend ({len(run)}) + washout inside one ({len(wash_dn)}): " + ", ".join(c["ticker"] for c in wash_dn[:20]))
    run.sort(key=lambda c: (not c["new"], -(c.get("break_vol_ratio") or 0)))
    print(f"\nRUNNER lane — hole broke up, in the universe (${sup['RUNNER']['universe']['cap_min']/1e9:.1f}B–5B, ATR ≥ 5 %): {len(run)} names, {sum(c['new'] for c in run)} fresh (≤ 3 bars)")
    print(f"  {'ticker':7s} {'cap$B':>6s} {'new':4s} {'close':>9s} {'ATR%':>5s} {'brk vol':>8s} {'bars':>5s} {'$M/day':>7s} {'52w%':>5s} {'earn7d':>6s}  plan")
    for c in run[:30]:
        print(f"  {c['ticker']:7s} {c.get('cap_B') or 0:6.2f} {'NEW' if c['new'] else '':4s} {c['close']:9.2f} {c['atr_pct']:5.1f} {c.get('break_vol_ratio') or 0:8.2f} {c.get('bars_since_break', 0):5d} {c.get('dvol20_M') or 0:7.1f} {c['context']['hi252']:5.1f} {str(c.get('earnings_7d', '')):>6s}  entry next open · exit hole-down · size {int(a.capital * 0.01 / c['atr']) if c['atr'] else 0} sh")
    base = [c for c in today["candidates"] if c["family"] != "RUNNER" and (c["context"].get("dvol20_M") or 0) >= 3.0]; base.sort(key=lambda c: (not c["new"], -(c["context"].get("dvol20_M") or 0)))
    print(f"\nBASE-HIT lane (≥ $3M/day only) — {len(base)} names ({', '.join(f'{k} {v}' for k, v in __import__('collections').Counter(c['family'] for c in base).items())}), {sum(c['new'] for c in base)} new")
    print(f"  {'ticker':7s} {'family':10s} {'new':4s} {'close':>9s} {'ATR%':>5s} {'target':>9s} {'stop':>9s} {'$M/day':>7s} {'earn7d':>6s}")
    for c in base[:30]:
        print(f"  {c['ticker']:7s} {c['family']:10s} {'NEW' if c['new'] else '':4s} {c['close']:9.2f} {c['atr_pct']:5.1f} {c['plan']['target'] or 0:9.2f} {c['plan']['stop']:9.2f} {c['context']['dvol20_M']:7.1f} {str(c.get('earnings_7d', '')):>6s}")


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--capital", type=float, default=100000); ap.add_argument("--dump", action="store_true")
    a = ap.parse_args(); sup, today = load()
    if a.dump:
        dump(sup, today, a); return
    curses.wrapper(draw, sup, today, a)


if __name__ == "__main__":
    main()
