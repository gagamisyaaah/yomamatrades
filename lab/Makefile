PY := python3.12
export PATH := $(HOME)/Library/Python/3.12/bin:$(PATH)

.PHONY: tv-check harvest smoke sweep mine scout doctor

doctor:            ## is Chrome reachable?
	browser-harness --doctor

tv-check:          ## compile every clone on a live TradingView chart (idea 1 / 7)
	set -a; . ./.env; set +a; $(PY) compile_lab.py

smoke:             ## 6 backtests to prove the Strategy Tester parser works
	set -a; . ./.env; set +a; $(PY) harvest.py --suite vcp --tickers NASDAQ:IREN NASDAQ:MRNA --tfs D W --limit 4

harvest:           ## full grid: signals × 9 tickers × 4H/D/W (idea 2)
	set -a; . ./.env; set +a; $(PY) harvest.py --suite all

harvest-vcp:       ## volatility-contraction families only
	set -a; . ./.env; set +a; $(PY) harvest.py --suite vcp

sweep:             ## example knob sweep (idea 3)
	set -a; . ./.env; set +a; $(PY) sweep.py --suite homily --signal "Volatility hole" --knob "Hole k" --values 1.0 1.2 1.4 1.7 2.0 2.5

mine:              ## founders' posts → labeled claims (idea 4)
	set -a; . ./.env; set +a; $(PY) mine_x.py --scrolls 40

scout:             ## community scripts corpus (idea 6)
	$(PY) scout.py
