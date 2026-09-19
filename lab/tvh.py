"""TradingView helpers over browser-harness (your real, logged-in Chrome via CDP).

Deterministic building blocks the lab scripts compose:
  open_chart / open_pine_editor / set_pine_source / add_to_chart / read_console / legend_titles /
  remove_indicator / open_strategy_tester / performance_summary / open_settings / set_input / export_chart_data
Everything is DOM-based (Monaco, buttons, tables, dialogs) — the chart canvas is never read.
"""
from __future__ import annotations

import json
import re
import time

from browser_harness.admin import ensure_daemon
from browser_harness.helpers import (  # noqa: F401
    cdp, new_tab, goto_url, list_tabs, switch_tab, current_tab, wait_for_load, wait_for_element,
    page_info, click_at_xy, press_key, capture_screenshot, wait,
)
from browser_harness.helpers import js as _js


def js(expression, target_id=None):
    """helpers.js with retries: TradingView occasionally blocks the main thread past the 5 s evaluate budget."""
    last = None
    for attempt in range(8):                     # heavy scripts can block the page for tens of seconds
        try:
            return _js(expression, target_id)
        except RuntimeError as e:
            last = e
            time.sleep(1.0 + 1.5 * attempt)
    raise last

TV = "https://www.tradingview.com"


def connect():
    ensure_daemon()


def focus_emulation():
    """Background tabs don't lay out Monaco; focus emulation makes the editor render and accept input."""
    try:
        cdp("Emulation.setFocusEmulationEnabled", enabled=True)
    except Exception:
        pass


# ───────────────────────── tab / chart ─────────────────────────
TAB_FILE = __import__("pathlib").Path(__file__).with_name(".lab_tab")


def open_chart(symbol: str = "NASDAQ:MRNA", interval: str = "D") -> str:
    """Use the lab's own background tab (created once, id remembered in .lab_tab) — never a tab you use."""
    url = f"{TV}/chart/?symbol={symbol}&interval={interval}"
    tab = TAB_FILE.read_text().strip() if TAB_FILE.exists() else ""
    attached = False
    if tab:
        try:
            switch_tab(tab)
            attached = "tradingview.com" in (page_info() or {}).get("url", "")
        except Exception:
            attached = False
    if attached:
        goto_url(url)
    else:
        t = new_tab(url)
        tid = t.get("targetId") if isinstance(t, dict) else (t if isinstance(t, str) else (current_tab() or ""))
        if isinstance(tid, dict):
            tid = tid.get("targetId", "")
        TAB_FILE.write_text(str(tid))
    wait_for_load(30)
    wait_for_element('[class*="legend-"]', timeout=30)
    focus_emulation()
    return url


def set_symbol(symbol: str, interval: str | None = None):
    url = f"{TV}/chart/?symbol={symbol}" + (f"&interval={interval}" if interval else "")
    goto_url(url)
    wait_for_load(30)
    wait_for_element('[class*="legend-"]', timeout=30)
    focus_emulation()


def _rect(expr: str):
    """Evaluate a JS expression returning an element; return its centre {x,y} or None."""
    return js(f"""(() => {{ const el = ({expr}); if (!el) return null; const r = el.getBoundingClientRect();
        if (!r.width || !r.height) return null; return {{x: r.x + r.width/2, y: r.y + r.height/2}}; }})()""")


def click_el(expr: str) -> bool:
    """Trusted click (CDP mouse events) on the centre of the element returned by the JS expression.
    TradingView's React handlers ignore synthetic element.click()."""
    r = _rect(expr)
    if not r:
        return False
    click_at_xy(r["x"], r["y"])
    return True


def hover_el(expr: str) -> bool:
    r = _rect(expr)
    if not r:
        return False
    cdp("Input.dispatchMouseEvent", type="mouseMoved", x=r["x"], y=r["y"])
    return True


def click_by_text(text_regex: str, tag: str = "button", exact: bool = True) -> bool:
    """Trusted click on the first visible `tag` whose text / title / aria-label matches the regex."""
    pat = json.dumps(text_regex)
    return click_el(f"""[...document.querySelectorAll({json.dumps(tag)})].find(e => e.offsetParent !== null &&
        new RegExp({pat}, 'i').test(((e.innerText || '').trim() + ' ' + (e.title || '') + ' ' + (e.getAttribute('aria-label') || '')).trim()))""")


def click_data_name(name: str) -> bool:
    return click_el(f"document.querySelector('[data-name={json.dumps(name)}]')")


def modal_text() -> str:
    return js("""[...document.querySelectorAll('[class*="popupDialog"], [role="dialog"]')].map(e => (e.innerText||'').trim()).filter(Boolean).join(' || ').slice(0, 400)""") or ""


def modal_button(label_regex: str) -> bool:
    return click_el(f"""[...document.querySelectorAll('[class*="popupDialog"] button, [role="dialog"] button')].find(b => new RegExp({json.dumps(label_regex)}, 'i').test((b.innerText||'').trim()))""")


# ───────────────────────── Pine editor ─────────────────────────
def open_pine_editor():
    focus_emulation()
    if not js("!!document.querySelector('[data-name=\"pine-dialog\"] .monaco-editor textarea.inputarea')"):
        # two buttons carry this data-name: the bottom-dock one and the right-sidebar one. The sidebar one opens
        # the side panel ([data-name="pine-dialog"]) that carries the toolbar and console the lab relies on.
        if not click_el("""[...document.querySelectorAll('[data-name="pine-dialog-button"]')].find(b => b.offsetParent && b.getBoundingClientRect().x > 1300)""") and not click_data_name("pine-dialog-button"):
            if not click_data_name("scripteditor"):
                click_by_text(r"^pine( editor)?$")
        wait_for_element(_EDITOR_TA, timeout=15)
    wait(1.0)


LAB_SCRIPT = "LAB scratch"
_EDITOR_TA = '[data-name="pine-dialog"] .monaco-editor textarea.inputarea'


def _focus_editor():
    """Trusted focus on the (possibly freshly re-mounted) editor textarea."""
    for attempt in range(6):
        if js(f"!!document.querySelector({json.dumps(_EDITOR_TA)})"):
            try:
                nid = cdp("DOM.getDocument", depth=0)["root"]["nodeId"]
                q = cdp("DOM.querySelector", nodeId=nid, selector=_EDITOR_TA)
                if q.get("nodeId"):
                    cdp("DOM.focus", nodeId=q["nodeId"])
                    return True
            except RuntimeError:
                pass
        if attempt == 2:
            open_pine_editor()
        wait(1.0)
    return False


def set_pine_source(code: str) -> int:
    """Replace the whole editor buffer: trusted ⌘A, then a synthetic clipboard `paste` event on Monaco's textarea
    (the same path a real ⌘V takes; multi-line Input.insertText gets re-indented by Monaco and mangles line 1).
    Scripts above ~24 KB go through the real clipboard (pbcopy + trusted ⌘V) because the harness IPC drops
    oversized evaluate calls; the founder's clipboard is restored afterwards."""
    _focus_editor()
    press_key("a", 4)
    wait(0.3)
    if len(code) > 24000:
        import subprocess
        old = subprocess.run(["pbpaste"], capture_output=True).stdout
        subprocess.run(["pbcopy"], input=code.encode("utf-8"), check=True)
        press_key("v", 4)
        wait(2.5 + len(code) / 40000)
        subprocess.run(["pbcopy"], input=old)
    else:
        js(f"""(() => {{ const ta = document.querySelector({json.dumps(_EDITOR_TA)}); const dt = new DataTransfer();
            dt.setData('text/plain', {json.dumps(code)});
            ta.dispatchEvent(new ClipboardEvent('paste', {{clipboardData: dt, bubbles: true, cancelable: true}})); return 1; }})()""")
    wait(2.0)
    head = editor_text_head(14).replace("\xa0", " ")
    return 1 if head.startswith("//@version") else -1


def current_script_name() -> str:
    return js("""(document.querySelector('[data-name="pine-dialog"] [class*="nameButton"]')?.innerText || '').trim()""") or ""


def save_script():
    _focus_editor()
    press_key("s", 4)
    wait(2.0)


def open_script_menu():
    click_el("""document.querySelector('[data-name="pine-dialog"] [class*="nameButton"]')""")
    wait(1.0)


def ensure_lab_script(create_kind: str = "Indicator"):
    """Make sure the editor holds the lab's own scratch script (never one of the user's)."""
    if current_script_name().startswith(LAB_SCRIPT):
        return True
    open_script_menu()
    # recently-used list
    if click_by_text("^" + re.escape(LAB_SCRIPT), tag='[role="menuitem"], [class*="item" i]'):
        wait(2.0)
        return current_script_name().startswith(LAB_SCRIPT)
    # not in recents → Open script… dialog and search
    click_by_text(r"^open script", tag='[role="menuitem"], [class*="item" i]')
    wait(1.5)
    found = click_by_text("^" + re.escape(LAB_SCRIPT) + "$", tag='[role="dialog"] *, [class*="dialog"] *')
    if found:
        wait(2.0)
        return current_script_name().startswith(LAB_SCRIPT)
    press_key("Escape")
    return False


_MENU_ITEM = """[...document.querySelectorAll('[role="menuitem"], [class*="item" i], div, span')].find(e => e.children.length <= 2 && e.offsetParent !== null && new RegExp({pat}, 'i').test((e.innerText||'').trim()))"""


def new_script(kind: str = "indicator") -> bool:
    """Script menu → Create new → Indicator / Strategy. A brand-new (unsaved) script can be added to the chart
    without any save prompt, so the lab never writes to your script library."""
    press_key("Escape")
    wait(0.3)
    open_script_menu()
    hover_el(_MENU_ITEM.format(pat=json.dumps(r"^create new$")))
    wait(1.0)
    ok = click_el(_MENU_ITEM.format(pat=json.dumps(r"^indicator$" if kind == "indicator" else r"^strategy$")))
    wait(2.5)
    m = modal_text()
    if re.search(r"unsaved|discard|don.t save", m, re.I):   # leaving a dirty untitled script
        modal_button(r"don.?t save|discard|no")
        wait(1.5)
    wait_for_element(_EDITOR_TA, timeout=15)
    wait(1.0)
    return ok and current_script_name().lower().startswith("untitled")


def create_lab_script(code: str) -> bool:
    """Create the scratch script once: new indicator, paste, Add to chart, Save with the lab name."""
    new_script("indicator")
    set_pine_source(code)
    click_by_text(r"^(add to chart|update on chart)$", tag='[data-name="pine-dialog"] button')
    wait(1.5)
    if re.search(r"save this script", modal_text(), re.I):
        modal_button(r"^save$")
        wait(1.5)
    # name prompt
    if js("!!document.querySelector('[class*=\"popupDialog\"] input, [role=\"dialog\"] input')"):
        r = _rect("""document.querySelector('[class*="popupDialog"] input, [role="dialog"] input')""")
        click_at_xy(r["x"], r["y"])
        cdp("Input.dispatchKeyEvent", type="keyDown", key="a", code="KeyA", windowsVirtualKeyCode=65, modifiers=4, commands=["selectAll"])
        cdp("Input.insertText", text=LAB_SCRIPT)
        wait(0.3)
        modal_button(r"^save$")
        wait(3.0)
    return current_script_name().startswith(LAB_SCRIPT)


def editor_text_head(chars: int = 200) -> str:
    for _ in range(3):
        try:
            return js(f"(document.querySelector('.monaco-editor .view-line')?.textContent || '').slice(0, {chars})") or ""
        except RuntimeError:
            wait(1.5)
    return ""


def console_text() -> str:
    return js("(document.querySelector('[class*=\"consoleWrapper\"]')?.innerText || '')") or ""


def add_to_chart(timeout: float = 25.0) -> dict:
    """Click Add/Update on chart, then wait for the console to report the outcome of THIS compile.
    Returns {clicked, ok, tail}: ok=True when the new console lines contain 'Added to chart' / 'Updated on chart'."""
    before = len(console_text())
    clicked = click_by_text(r"^(add to chart|update on chart)$", tag='[data-name="pine-dialog"] button')
    if not clicked:                      # once the script is on the chart the toolbar drops the button; ⌘⏎ = Update on chart
        _focus_editor()
        press_key("Enter", 4)
        clicked = True
    tail = ""
    t0 = time.time()
    while time.time() - t0 < timeout:
        wait(1.0)
        if re.search(r"save this script", modal_text(), re.I):   # only happens on a saved (named) script
            modal_button(r"^cancel$")
            return {"clicked": clicked, "ok": False, "tail": "refused: editor holds a saved script; use new_script() first"}
        tail = console_text()[before:]
        if re.search(r"added to chart|updated on chart", tail, re.I) or re.search(r"line \d+|error|exception|failed", tail, re.I):
            break
    ok = bool(re.search(r"added to chart|updated on chart", tail, re.I)) and not re.search(r"line \d+:", tail, re.I)
    return {"clicked": clicked, "ok": ok, "tail": re.sub(r"\d\d:\d\d:\d\d", " ", tail).strip()}


def read_console() -> str:
    return console_text()[-1500:]


def _expand_legend():
    if js("""(() => { const t = document.querySelector('[class*="togglerWrapper"]'); return !!(t && /^\\d+$/.test((t.innerText||'').trim())); })()"""):
        click_el("""document.querySelector('[class*="togglerWrapper"]')""")
        wait(0.8)


def legend_items() -> list[dict]:
    """[{title, hidden, idx}] for every legend entry (main series first)."""
    _expand_legend()
    return js(f"""(() => {{ if (!document.querySelector('[class*="legend-"]')) return [];
        return {_ITEM}.map((e, i) => {{
            const btns = [...e.querySelectorAll('[role="button"], button')].map(b => (b.getAttribute('aria-label') || b.title || '').trim());
            return {{idx: i, title: {_TITLE}(e).slice(0, 80), hidden: btns.includes('Show')}}; }}); }})()""") or []


def legend_titles() -> list[str]:
    return [it["title"] for it in legend_items()]


_ITEM = """[...document.querySelector('[class*="legend-"]').querySelectorAll('[class*="item-"]')].filter(e => e.querySelector('[data-name="actions"]'))"""
_TITLE = """(e => ((e.querySelector('[class*="mainTi"] [class*="title-"]') || e.querySelector('[class*="mainTi"]') || {innerText: (e.innerText||'').split('\\n')[0]}).innerText || '').trim())"""


def _legend_action(title_regex: str, action: str) -> int:
    """Trusted hover + click of a per-item legend button ('Remove', 'Settings', 'Hide', 'Show') on the first matching title."""
    _expand_legend()
    pat = json.dumps(title_regex)
    item = f"""{_ITEM}.find(e => new RegExp({pat}, 'i').test({_TITLE}(e)))"""
    if not hover_el(item):
        return 0
    wait(0.4)
    btn = f"""(() => {{ const e = {item}; if (!e) return null;
        return [...e.querySelectorAll('[role="button"], button')].find(b => (b.getAttribute('aria-label') || b.title || '').trim() === {json.dumps(action)}); }})()"""
    return 1 if click_el(btn) else 0


_ALL_ITEMS = """[...document.querySelectorAll('[class*="legend-"]')].flatMap(l => [...l.querySelectorAll('[class*="item-"]')].filter(e => e.querySelector('[data-name="actions"]')))"""


def all_legend_titles() -> list[str]:
    """Titles across every pane's legend (main pane first)."""
    return js(f"""{_ALL_ITEMS}.map(e => {_TITLE}(e).slice(0, 80))""") or []


def remove_indicator(title_regex: str) -> int:
    """Select the indicator by clicking its legend title, press Delete (TradingView's own shortcut). Works on
    every pane; the collapsed '+N' group on the main pane is expanded first. The strategy report panel covers
    the legend rows, so it is closed first."""
    n = 0
    close_report()
    for _ in range(8):
        click_el("""[...document.querySelectorAll('[class*="togglerWrapper"], [class*="toggler"]')].find(e => /^\\+\\d+$/.test((e.innerText||'').trim()))""")
        wait(0.5)
        el = f"""(() => {{ const e = {_ALL_ITEMS}.find(e => new RegExp({json.dumps(title_regex)}, 'i').test({_TITLE}(e))); if (!e) return null;
            return e.querySelector('[class*="mainTi"] [class*="title-"]') || e.querySelector('[class*="mainTi"]') || e; }})()"""
        if not click_el(el):
            break
        wait(0.5)
        press_key("Delete")
        wait(1.5)
        n += 1
        if not any(re.search(title_regex, t, re.I) for t in all_legend_titles()):
            break
    wait(1.0)
    if any(re.search(title_regex, t, re.I) for t in all_legend_titles()):   # the report panel re-renders; one more pass
        close_report()
        wait(1.0)
        n += remove_indicator_once(title_regex)
    return n


def remove_indicator_once(title_regex: str) -> int:
    el = f"""(() => {{ const e = {_ALL_ITEMS}.find(e => new RegExp({json.dumps(title_regex)}, 'i').test({_TITLE}(e))); if (!e) return null;
        return e.querySelector('[class*="mainTi"] [class*="title-"]') || e.querySelector('[class*="mainTi"]') || e; }})()"""
    if not click_el(el):
        return 0
    wait(0.5)
    press_key("Delete")
    wait(1.5)
    return 1


# ───────────────────────── symbol / interval without a reload ─────────────────────────
def legend_head() -> str:
    """'M | Moderna, Inc. | 1D' — first lines of the main legend (symbol, name, interval)."""
    return js(r"""(() => { const l = document.querySelector('[class*="legend-"]'); return l ? (l.innerText||'').split('\n').slice(0,3).join(' | ') : ''; })()""") or ""


_TF_LABEL = {"240": "4h", "60": "1h", "D": "1D", "1D": "1D", "W": "1W", "1W": "1W", "M": "1M", "1M": "1M"}


def symbol_ok(symbol: str, interval: str | None = None) -> bool:
    """The legend shows the company name, not the ticker — the header symbol button carries the ticker."""
    tick = symbol.split(":")[-1]
    hdr = js("(document.getElementById('header-toolbar-symbol-search')?.innerText || '').trim()") or ""
    if hdr.upper() != tick.upper():
        return False
    return not interval or _TF_LABEL.get(interval, interval).lower() in legend_head().lower()


def set_symbol_ui(symbol: str, interval: str | None = None, retries: int = 2) -> bool:
    """Header symbol button → type → Enter; then type the interval on the chart ('240', '1D', '1W') → Enter.
    No page reload, so whatever is on the chart (an unsaved strategy twin) stays and recalculates.
    Returns True only when the legend confirms both symbol and interval."""
    for _ in range(retries + 1):
        press_key("Escape")
        wait(0.3)
        if not symbol_ok(symbol):
            if not click_el("""document.getElementById('header-toolbar-symbol-search')"""):
                raise RuntimeError("symbol button not found")
            wait_for_element('[data-name="symbol-search-items-dialog"] input', timeout=5)
            wait(0.3)
            cdp("Input.insertText", text=symbol)
            wait(1.0)
            press_key("Enter")
            wait(1.5)
        if interval and not symbol_ok(symbol, interval):
            tf = {"D": "1D", "W": "1W", "M": "1M"}.get(interval, interval)
            for ch in tf:
                press_key(ch)
                wait(0.12)
            wait(0.6)
            press_key("Enter")
            wait(1.5)
        for _ in range(10):
            if symbol_ok(symbol, interval):
                return True
            wait(0.5)
    return False


def legend_values(title_regex: str = r"twin") -> list[float | None]:
    """The status-line plot values of one legend item (in plot order), as floats."""
    raw = js(f"""(() => {{
        const it = {_ALL_ITEMS}.find(e => new RegExp({json.dumps(title_regex)}, 'i').test({_TITLE}(e)));
        if (!it) return null;
        return [...it.querySelectorAll('[class*="valueItem"]')].map(v => ((v.querySelector('[class*="valueValue"]') || v).innerText || '').trim()); }})()""") or []
    out = []
    for s in raw:
        s = s.replace("−", "-").replace(",", "").replace("∞", "inf")
        m = re.search(r"-?\d+\.?\d*|-?inf", s)
        out.append(float(m.group()) if m else None)
    return out


# ───────────────────────── Strategy report (opens by itself when a strategy is added) ─────────────────────────
def report_open() -> bool:
    return bool(js(r"""!![...document.querySelectorAll('div, span')].find(e => e.children.length === 0 && e.offsetParent && /^Profit factor$/i.test((e.innerText||'').trim()))"""))


def close_report():
    """The report panel sits over the pane legends, so close it before touching legend items."""
    if click_el("""[...document.querySelectorAll('button')].find(b => b.getAttribute('aria-label') === 'Close strategy report' && !(b.innerText||'').trim())"""):
        wait(0.8)


def open_strategy_tester(timeout: float = 12.0) -> bool:
    """The report opens by itself the first time a strategy is added. If it was closed since, find its tab."""
    if wait_report(None, timeout):
        return True
    click_by_text(r"^strategy (report|tester)$", tag="button, [role='tab'], div, span")
    return bool(wait_report(None, 8))


def key_stats() -> dict:
    """Read the 'Key stats' tiles: Total PnL, Max drawdown, Profitable trades, Profit factor (strings as rendered)."""
    return js(r"""(() => {
        const out = {};
        const labels = /^(Total PnL|Net profit|Max drawdown|Profitable trades|Profit factor|Total trades|Avg trade|Buy and hold return)$/i;
        for (const l of document.querySelectorAll('div, span')) {
            if (l.children.length || !l.offsetParent) continue;
            const t = (l.innerText||'').trim(); if (!labels.test(t)) continue;
            let p = l; for (let i = 0; i < 3 && p.parentElement; i++) { p = p.parentElement; if ((p.innerText||'').split('\n').length >= 2) break; }
            if (!(t in out)) out[t] = (p.innerText||'').replace(t, '').replace(/\n/g, ' ').trim().slice(0, 80);
        }
        return out; })()""") or {}


def performance_summary() -> dict:
    """Compatibility shim: {label: [value]} from the Key stats tiles."""
    return {k: [v] for k, v in key_stats().items()}


def metrics_from_key_stats(ks: dict) -> dict:
    def nums(s):
        return [float(x.replace(",", "")) for x in re.findall(r"-?\d[\d,]*\.?\d*", (s or "").replace("−", "-").replace("+", ""))]
    pnl = nums(ks.get("Total PnL") or ks.get("Net profit"))
    dd = nums(ks.get("Max drawdown"))
    pt = nums(ks.get("Profitable trades"))
    pf = nums(ks.get("Profit factor"))
    tt = nums(ks.get("Total trades"))
    return {
        "net_profit_pct": pnl[-1] if len(pnl) >= 2 else (pnl[0] if pnl else None),
        "profit_factor": pf[0] if pf else None,
        "win_rate": pt[0] if pt else None,
        "max_dd_pct": dd[-1] if len(dd) >= 2 else (dd[0] if dd else None),
        "trades": (tt[0] if tt else (pt[2] if len(pt) >= 3 else None)),
        "avg_trade_pct": None,
    }


def wait_report(prev: dict | None = None, timeout: float = 10.0) -> dict:
    """Poll the tiles until they differ from `prev` (if given) and hold still for two reads."""
    t0 = time.time()
    last = None
    while time.time() - t0 < timeout:
        ks = key_stats()
        if ks and (prev is None or ks != prev) and ks == last:
            return ks
        last = ks
        wait(0.7)
    return last or {}


def wait_cell(prev: tuple | None, timeout: float = 30.0, title_regex: str = r"twin") -> tuple[dict, list, bool]:
    """After a symbol/interval change: wait until BOTH the report tiles and the strategy's legend values differ from
    the previous cell and hold still for two consecutive reads. Returns (tiles, values, fresh); fresh=False means
    the report never changed within `timeout` (the row must not be trusted)."""
    t0 = time.time()
    last = None
    prev_ks, prev_vals = prev if prev else (None, None)
    while time.time() - t0 < timeout:
        ks = key_stats()
        vals = legend_values(title_regex)
        cur = (ks, vals)
        changed = prev is None or (ks != prev_ks and vals != prev_vals)
        if ks and vals and changed and cur == last:
            return ks, vals, True
        last = cur
        wait(0.7)
    ks, vals = last if last else ({}, [])
    return ks, vals, False


def metrics_from_summary(summary: dict) -> dict:
    """Pick the numbers we rank on. Tolerant to TradingView renaming rows."""
    def find(pattern):
        for k, v in summary.items():
            if re.search(pattern, k, re.I):
                return v[0] if v else ""
        return ""
    def num(s):
        m = re.search(r"-?\d[\d,]*\.?\d*", s.replace("−", "-"))
        return float(m.group().replace(",", "")) if m else None
    return {
        "net_profit_pct": num(find(r"^net profit")),
        "profit_factor": num(find(r"profit factor")),
        "win_rate": num(find(r"percent profitable|win rate")),
        "max_dd_pct": num(find(r"max(imum)? (equity )?drawdown")),
        "trades": num(find(r"total (closed )?trades")),
        "avg_trade_pct": num(find(r"^avg(erage)? trade")),
    }


# ───────────────────────── settings dialog ─────────────────────────
def open_settings(title_regex: str) -> bool:
    ok = _legend_action(title_regex, "Settings") > 0
    wait(1.2)
    return ok


def set_input(label_regex: str, value) -> bool:
    """In the open settings dialog, set the input whose label matches, then keep the dialog open."""
    ok = bool(js(f"""(() => {{
        const re = new RegExp({json.dumps(label_regex)}, 'i');
        const dlg = document.querySelector('[data-dialog-name], [role="dialog"]') || document;
        const cells = [...dlg.querySelectorAll('div, label, span')].filter(e => e.children.length === 0 && re.test(e.innerText || ''));
        for (const c of cells) {{
            let row = c; for (let i = 0; i < 4 && row; i++) {{ row = row.parentElement;
                const inp = row && row.querySelector('input'); if (inp) {{
                    inp.focus(); inp.select();
                    document.execCommand('insertText', false, String({json.dumps(value)}));
                    inp.dispatchEvent(new Event('input', {{bubbles:true}}));
                    inp.dispatchEvent(new Event('change', {{bubbles:true}}));
                    inp.blur(); return true; }} }}
        }}
        return false; }})()"""))
    wait(0.4)
    return ok


def settings_ok():
    click_by_text(r"^ok$")
    wait(2.5)


# ───────────────────────── export ─────────────────────────
def export_chart_data() -> bool:
    """Chart menu → Export chart data… → Export. Needs a plan that offers the feature."""
    if not click_by_text(r"open chart menu|chart menu|^menu$"):
        press_key("Escape")
    wait(0.8)
    if not click_by_text(r"export chart data", tag="div, span, button"):
        return False
    wait(1.2)
    click_by_text(r"^export$")
    wait(3.0)
    return True


def probe(limit: int = 60) -> dict:
    """Debug: list data-name attributes and button texts so selectors can be adapted quickly."""
    return js(f"""(() => {{
        const names = [...new Set([...document.querySelectorAll('[data-name]')].map(e => e.getAttribute('data-name')))].slice(0, {limit});
        const buttons = [...new Set([...document.querySelectorAll('button')].map(b => (b.innerText || b.getAttribute('aria-label') || '').trim()).filter(Boolean))].slice(0, {limit});
        return {{names, buttons, url: location.href, title: document.title}}; }})()""")
