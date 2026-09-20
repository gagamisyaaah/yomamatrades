"""TREE OF LIFE on Discord — say run from your phone.

Setup (once, by you — the bot token is a secret only you should hold):
  1. https://discord.com/developers/applications → New Application → Bot → Reset Token → copy it.
     Under Bot, enable "MESSAGE CONTENT INTENT". Under OAuth2 → URL Generator: scope "bot", permissions
     "Send Messages", "Attach Files", "Read Message History" → open the generated URL → add the bot to your server.
  2. In tv_lab/.env add:   DISCORD_TOKEN=...   and optionally   DISCORD_CHANNEL_ID=<channel id> (right-click a channel → Copy ID)
  3. pip install discord.py   (python3.12 -m pip install -U discord.py)
  4. Keep the Mac awake and run:   caffeinate -s python3.12 bend/evo/bot_discord.py

Commands in the channel:
  !run          rescan every name (≈ 25 min here) with live quotes and the guard, then post the lanes
  !lanes        post the latest lanes without rescanning
  !live         refresh live prices for the listed names and post again
  !jev TICKER   Jev's typed verdict on a listed name (setup quality · take full / half / wait / skip · event risk)
  !tree         the lineage page link
  !help         this list
"""
from __future__ import annotations

import asyncio
import io
import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT)); sys.path.insert(0, str(HERE))
for ln in (ROOT / ".env").read_text().splitlines():
    if "=" in ln and not ln.strip().startswith("#"):
        k, v = ln.split("=", 1); os.environ.setdefault(k.strip(), v.strip().strip('"'))

import discord  # noqa: E402  (pip install discord.py)
from PIL import Image, ImageDraw, ImageFont  # noqa: E402 (pip install pillow)

TREE_URL = "https://claude.ai/code/artifact/9de67f97-2444-4185-a830-e50458eb9caa"
intents = discord.Intents.default(); intents.message_content = True
client = discord.Client(intents=intents)
busy = asyncio.Lock()


def lanes_text(live: bool) -> str:
    args = [sys.executable, str(HERE / "tree_of_life.py"), "--dump"] + (["--live"] if live else [])
    return subprocess.run(args, capture_output=True, text=True, cwd=ROOT).stdout


async def post(channel, text: str, name: str):
    head = "\n".join(text.splitlines()[:6])
    await channel.send(f"```\n{head[:1800]}\n```", file=discord.File(io.BytesIO(text.encode()), filename=name))


def render_board(text: str) -> bytes:
    """Render the terminal text as a dark PNG the phone shows inline. Monospace, colored heads."""
    lines = text.splitlines() or ["(empty)"]
    for path in ("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", "/System/Library/Fonts/Menlo.ttc", "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf"):
        if Path(path).exists():
            font = ImageFont.truetype(path, 14); bold = ImageFont.truetype(path, 14); break
    else:
        font = ImageFont.load_default(); bold = font
    line_h = 18; w = max(min(1600, max(len(ln) for ln in lines) * 8 + 40), 720); h = 20 + len(lines[:80]) * line_h + 20
    img = Image.new("RGB", (w, h), (14, 20, 28)); d = ImageDraw.Draw(img)
    for i, ln in enumerate(lines[:80]):
        color = (230, 236, 242)
        if ln.startswith("TREE OF LIFE"):
            color = (176, 148, 74)
        elif "RUNNER lane" in ln or "TREE view" in ln:
            color = (79, 194, 172)
        elif "BASE-HIT" in ln:
            color = (232, 132, 95)
        elif ln.strip().startswith("ticker"):
            color = (154, 169, 184)
        elif " NEW " in ln:
            color = (102, 187, 106)
        elif ln.startswith("  "):
            color = (200, 210, 220)
        d.text((20, 20 + i * line_h), ln.rstrip()[:200], fill=color, font=font)
    buf = io.BytesIO(); img.save(buf, format="PNG"); return buf.getvalue()


async def post_board(channel, text: str, filename: str):
    png = await asyncio.to_thread(render_board, text)
    await channel.send(file=discord.File(io.BytesIO(png), filename=filename))


@client.event
async def on_ready():
    print(f"Tree of Life bot online as {client.user}")


@client.event
async def on_message(m: discord.Message):
    if m.author == client.user or not m.content.startswith("!"):
        return
    cid = os.environ.get("DISCORD_CHANNEL_ID")
    if cid and str(m.channel.id) != cid:
        return
    cmd, *rest = m.content[1:].split()
    if cmd == "help":
        await m.channel.send("`!board` visual board (image) · `!lanes` latest lanes (text) · `!live` live prices · `!run` full rescan (~8 min) · `!jev TICKER` verdict · `!tree` lineage page")
    elif cmd == "board":
        await post_board(m.channel, await asyncio.to_thread(lanes_text, False), "tree_of_life.png")
    elif cmd == "tree":
        await m.channel.send(TREE_URL)
    elif cmd == "lanes":
        await post(m.channel, await asyncio.to_thread(lanes_text, False), "tree_of_life.txt")
    elif cmd == "live":
        await m.channel.send("fetching live prices…")
        text = await asyncio.to_thread(lanes_text, True)
        await post_board(m.channel, text, "tree_of_life_live.png"); await post(m.channel, text, "tree_of_life_live.txt")
    elif cmd == "run":
        if busy.locked():
            await m.channel.send("a scan is already running"); return
        async with busy:
            await m.channel.send("scanning 3,600 names with live quotes and the market guard — about 25 minutes on this Mac; I'll post when done")
            proc = await asyncio.create_subprocess_exec(sys.executable, str(HERE / "today.py"), "--tail", "420", "--quotes", "--procs", "3", cwd=ROOT, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.STDOUT)
            out, _ = await proc.communicate()
            tail = out.decode(errors="ignore").strip().splitlines()[-1:] if out else ["(no output)"]
            await m.channel.send(f"scan done: {tail[0][:300]}")
            text = await asyncio.to_thread(lanes_text, True)
            await post_board(m.channel, text, "tree_of_life_live.png"); await post(m.channel, text, "tree_of_life_live.txt")
    elif cmd == "jev" and rest:
        t = rest[0].upper()
        today = json.load(open(ROOT / "bend/data/today.json")); sup = json.load(open(ROOT / "bend/data/supers.json"))
        cands = [c for c in today["candidates"] if c["ticker"] == t]
        if not cands:
            await m.channel.send(f"{t} is not in today's lanes"); return
        from tree_of_life import jev_verdict
        for c in cands[:2]:
            c["guard"] = today.get("guard")
            try:
                v = await asyncio.to_thread(jev_verdict, c, sup)
                sq, rf, er = v.get("setup_quality", {}), v.get("regime_fit", {}), v.get("event_risk", {})
                await m.channel.send(f"**{t} · {c['family']}** — setup quality {sq.get('score', sq)} / 3 · handling **{rf.get('choice')}** ({rf.get('confidence', 0):.0%}) · event risk {er.get('noul', 0):.0%}\nplan: {c['plan']}")
            except Exception as e:  # noqa: BLE001
                await m.channel.send(f"Jev unavailable: {str(e)[:120]}")
    else:
        await m.channel.send("unknown command — `!help`")


if __name__ == "__main__":
    tok = os.environ.get("DISCORD_TOKEN")
    if not tok:
        sys.exit("DISCORD_TOKEN missing in tv_lab/.env — see the setup notes at the top of this file")
    client.run(tok)
