#!/usr/bin/env bash
# TREE OF LIFE on an Oracle Cloud Always Free VM (Ubuntu 22.04 aarch64, 3 OCPU / 18 GB).
# One paste on the box: dependencies, Bend, the repo, the engine binary, the price cache, and the Discord bot under tmux.
# Time: ~40 min unattended (mostly the price download). Requires a Discord bot token and (optional) a TypeSafe API key.
# Usage:   curl -fsSL https://raw.githubusercontent.com/gagamisyaaah/yomamatrades/main/lab/oracle_setup.sh | bash
#   or:    bash oracle_setup.sh
# It prompts for the two tokens once; nothing is written to git.
set -euo pipefail

echo "== 1/6 · system packages =="
sudo apt-get update -qq
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq python3.11 python3.11-venv python3-pip build-essential clang git curl tmux jq
python3.11 -m venv ~/venv && source ~/venv/bin/activate
pip install -q -U pandas numpy pyarrow discord.py

echo "== 2/6 · Bend 2 (ARM Linux build) =="
curl -fsSL https://raw.githubusercontent.com/HigherOrderCO/bend/main/install.sh | bash   # installs into ~/.bend
export PATH="$HOME/.bend/bin:$PATH" BEND_NO_TELEMETRY=1
grep -q '.bend/bin' ~/.bashrc || echo 'export PATH="$HOME/.bend/bin:$PATH"; export BEND_NO_TELEMETRY=1' >> ~/.bashrc
bend --version || { echo "Bend install failed — check the URL"; exit 1; }

echo "== 3/6 · repo =="
[ -d ~/yomamatrades ] || git clone https://github.com/gagamisyaaah/yomamatrades.git ~/yomamatrades
cd ~/yomamatrades/lab

echo "== 4/6 · secrets (once; kept only in .env) =="
if [ ! -f .env ]; then
    read -rp "Discord bot token: " DTOK
    read -rp "TypeSafe API key (blank to skip): " JTOK
    printf 'DISCORD_TOKEN=%s\nTYPESAFE_API_KEY=%s\n' "$DTOK" "$JTOK" > .env
    chmod 600 .env
fi

echo "== 5/6 · price cache + engine =="
mkdir -p mine/cache bend/data/D
# Fetch every US common stock ≥ $100M via the Nasdaq screener + history API (parallel), then pack for the engine
python3.11 -m mine.data --universe broad 2>&1 | tail -3
python3.11 bend/pack.py --tf D 2>&1 | tail -1
python3.11 bend/order.py bend/dragon.bend
( cd bend && bend dragon.bend -o dragon ) 2>&1 | tail -1
./bend/dragon bend/data/D --threads 3 | tail -1
python3.11 bend/trades.py --dir bend/data/D --stats --out trades_D_full 2>&1 | tail -1
python3.11 bend/evo/build_matrix.py --trades trades_D_full --out matrix_D_full --procs 3 2>&1 | tail -1

echo "== 6/6 · first scan and the Discord bot under tmux =="
python3.11 bend/evo/today.py --tail 420 --quotes --procs 3 2>&1 | tail -2
tmux kill-session -t tree 2>/dev/null || true
tmux new -d -s tree "cd ~/yomamatrades/lab && source ~/venv/bin/activate && export PATH=\"$HOME/.bend/bin:\$PATH\" && python3.11 bend/evo/bot_discord.py 2>&1 | tee -a bot.log"
echo
echo "READY. In your Discord channel: !run · !lanes · !live · !jev TICKER · !tree · !help"
echo "reconnect the bot's screen:   tmux attach -t tree      (Ctrl-b then d to detach)"
echo "logs:                          tail -f ~/yomamatrades/lab/bot.log"
echo "restart after a reboot:        tmux new -d -s tree 'cd ~/yomamatrades/lab && source ~/venv/bin/activate && python3.11 bend/evo/bot_discord.py'"
