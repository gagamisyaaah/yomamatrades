# DRAGON C engine v1 — the full total-signal reading in Bend, per ticker, per bar.
# Input: packed bars (6 U32 words per bar: date, o×10000, h×10000, l×10000, c×10000, v/100). Output per bar: 2 words —
#   word 0 = flags (bit 0 hole broke up, 1 hole broke down, 2 whales ≥ 50, 3 TE rising, 4 Mountain ≥ 3, 5 above gold,
#            6 bubble hot, 7 SS candle confirmed, 8 ribbon red, 9 closes above POC, 10 MACD>0 & RSI×3 up,
#            11 fresh break (≤ 3 bars), bits 16-23 whales %, bits 24-27 panels bullish (0-7))
#   word 1 = ATR as % of close × 1000.
# Per ticker the output is: n, 2n state words, m, 16m trade words (see the trades section).
# Idiom: every value handed back beside an array is consumed by the next def in a chain (a match may not
# inspect a local binder); no def cycles; loops recurse on a Nat.
import Base

# ═══════════════════════ bytes → words → arrays ═══════════════════════

def words(bs: List<&2, U32>, acc: List<&2, U32>) -> List<&2, U32>:
  match bs:
    case Con{b0, Con{b1, Con{b2, Con{b3, rest}}}}:
      w = (b0 .|. U32.shln(b1, 8n) .|. U32.shln(b2, 16n) .|. U32.shln(b3, 24n) : U32)
      words(rest, w <> acc)
    case _:
      acc

def fill(xs: List<&2, U32>, +i: U32, a: Array<U32>) -> Array<U32>:
  match xs:
    case Nil{}:
      a
    case Con{h, t}:
      fill(t, (i + 1 : U32), a[i] <- h)

def depth.go(fuel: Nat, small: Bool, +n: U32, d: Nat, +cap: U32) -> Nat:
  match fuel:
    case 0n:
      d
    case 1n+f:
      match small:
        case True{}:
          depth.go(f, U32.is_lt((cap * 2 : U32), n), n, 1n+d, (cap * 2 : U32))
        case False{}:
          d

def depth(+n: U32) -> Nat:
  depth.go(40n, U32.is_lt(1, n), n, 0n, 1)

def to_array(ws: List<&2, U32>, +n: U32) -> Array<U32>:
  fill(ws, 0, Array.new(U32, depth(n), 0))
# ═══════════════════════ helpers ═══════════════════════

def sel(b: Bool, +x: F32, +y: F32) -> F32:
  match b:
    case True{}:
      x
    case False{}:
      y

def sel_u(b: Bool, +x: U32, +y: U32) -> U32:
  match b:
    case True{}:
      x
    case False{}:
      y

def bit(b: Bool, +v: U32) -> U32:
  match b:
    case True{}:
      v
    case False{}:
      0

def one(b: Bool) -> U32:
  match b:
    case True{}:
      1
    case False{}:
      0

def px(w: U32) -> F32:
  (U32.to_f32(w) / 10000.0 : F32)

def ema(+prev: F32, +x: F32, +n: F32) -> F32:
  (prev + (x - prev) * (2.0 / (n + 1.0)) : F32)

def rma(+prev: F32, +x: F32, +n: F32) -> F32:
  ((prev * (n - 1.0) + x) / n : F32)

def win(+i: U32, +n: U32) -> Nat:
  U32.to_nat(sel_u(U32.is_lt(i, n), (i + 1 : U32), n))

def start(+i: U32, +n: U32) -> U32:
  sel_u(U32.is_lt(i, n), 0, (i + 1 - n : U32))

def winp(+i: U32, +n: U32) -> Nat:
  U32.to_nat(sel_u(U32.is_lt(i, n), i, n))

def startp(+i: U32, +n: U32) -> U32:
  sel_u(U32.is_lt(i, n), 0, (i - n : U32))

def at(bars: Array<U32>, +i: U32, +k: U32) -> Array<U32> & U32:
  bars[(i * 6 + k : U32)]
# ═══════════════════════ a bar as a record ═══════════════════════

type Bar is Data:
  Bar{o: F32, h: F32, l: F32, c: F32, v: F32}

def bar.v(r: Array<U32> & U32, +o: F32, +h: F32, +l: F32, +c: F32) -> Array<U32> & Bar:
  (bars, vw) = r
  (bars, Bar{o, h, l, c, U32.to_f32(vw)})

def bar.c(r: Array<U32> & U32, +o: F32, +h: F32, +l: F32, +i: U32) -> Array<U32> & Bar:
  (bars, cw) = r
  bar.v(at(bars, i, 5), o, h, l, px(cw))

def bar.l(r: Array<U32> & U32, +o: F32, +h: F32, +i: U32) -> Array<U32> & Bar:
  (bars, lw) = r
  bar.c(at(bars, i, 4), o, h, px(lw), i)

def bar.h(r: Array<U32> & U32, +o: F32, +i: U32) -> Array<U32> & Bar:
  (bars, hw) = r
  bar.l(at(bars, i, 3), o, px(hw), i)

def bar.o(r: Array<U32> & U32, +i: U32) -> Array<U32> & Bar:
  (bars, ow) = r
  bar.h(at(bars, i, 2), px(ow), i)

def bar(bars: Array<U32>, +i: U32) -> Array<U32> & Bar:
  bar.o(at(bars, i, 1), i)
# ═══════════════════════ rolling windows over the bars array ═══════════════════════
# generic fold of field k over [idx, idx+n): acc' = f(acc, value); f is a template

def roll.fin(~f: F32 -> F32 -> F32, r: Array<U32> & U32, +acc: F32) -> Array<U32> & F32:
  (bars, w) = r
  (bars, f(acc, px(w)))

def roll.step(~f: F32 -> F32 -> F32, r: Array<U32> & F32, +idx: U32, +k: U32) -> Array<U32> & F32:
  (bars, acc) = r
  roll.fin(~f, at(bars, idx, k), acc)

def roll(~f: F32 -> F32 -> F32, n: Nat, +idx: U32, +k: U32, r: Array<U32> & F32) -> Array<U32> & F32:
  match n:
    case 0n:
      r
    case 1n+p:
      roll(~f, p, (idx + 1 : U32), k, roll.step(~f, r, idx, k))

def fmax(a: F32, b: F32) -> F32:
  F32.max(a, b)

def fmin(a: F32, b: F32) -> F32:
  F32.min(a, b)

def fadd(a: F32, b: F32) -> F32:
  (a + b : F32)
# ═══════════════════════ the pure decisions (LAWS.bend states facts about these) ═══════════════════════
# The volatility hole: a level pair is set at each confirmed pivot; from the armed state two closes above break it up,
# two closes below break it down; a broken hole stays broken until the next pivot re-arms it.

type Hole is Data:
  InForce{}
  BrokeUp{}
  BrokeDown{}

def hole.arm(up: Bool, dn: Bool) -> Hole:
  match up:
    case True{}:
      BrokeUp{}
    case False{}:
      match dn:
        case True{}:
          BrokeDown{}
        case False{}:
          InForce{}

def hole.next(new_piv: Bool, up: Bool, dn: Bool, s: Hole) -> Hole:
  match new_piv:
    case True{}:
      hole.arm(up, dn)
    case False{}:
      match s:
        case InForce{}:
          hole.arm(up, dn)
        case BrokeUp{}:
          BrokeUp{}
        case BrokeDown{}:
          BrokeDown{}

def hole.is_up(s: Hole) -> Bool:
  match s:
    case BrokeUp{}:
      True{}
    case _:
      False{}

def hole.is_dn(s: Hole) -> Bool:
  match s:
    case BrokeDown{}:
      True{}
    case _:
      False{}

def bit_on(+f: U32, +m: U32) -> Bool:
  U32.is_eq((f .&. m : U32), m)
# the C reading: hole broke up, whales ≥ 50 %, TE rising, bubble not hot, Mountain ≥ 3, close above the gold band

def sig_c.b(up: Bool, wh: Bool, te: Bool, hot: Bool, mt: Bool, gd: Bool) -> Bool:
  match up:
    case False{}:
      False{}
    case True{}:
      match wh:
        case False{}:
          False{}
        case True{}:
          match te:
            case False{}:
              False{}
            case True{}:
              match hot:
                case True{}:
                  False{}
                case False{}:
                  match mt:
                    case False{}:
                      False{}
                    case True{}:
                      gd
# bullish panels: SS candle confirmed, ribbon, hole up, TE up, MACD & RSI up, closes above POC, whales ≥ 50 with retail shrinking

def panels.b(ss: Bool, rb: Bool, hu: Bool, te: Bool, mr: Bool, pc: Bool, wh: Bool) -> U32:
  (one(ss) + one(rb) + one(hu) + one(te) + one(mr) + one(pc) + one(wh) : U32)
# a bracket leg: once decided it stays decided; the stop is checked before the target; otherwise it keeps its running value

def bk.pick(done: Bool, hit_s: Bool, hit_t: Bool, +cur: F32, +stop_r: F32, +tgt_r: F32) -> F32:
  match done:
    case True{}:
      cur
    case False{}:
      match hit_s:
        case True{}:
          stop_r
        case False{}:
          match hit_t:
            case True{}:
              tgt_r
            case False{}:
              cur
# ═══════════════════════ the state ═══════════════════════

type St is Data:
  St{
    k: F32, d: F32, red_hi: F32, red_lo: F32,
    rib8: F32, rib20: F32, rib20_8: F32,
    atr: F32, prev_c: F32,
    h_up: F32, h_dn: F32, h_state: Hole, hs1: Hole, hs2: Hole, hs3: Hole,
    e21: F32, e34: F32, e68: F32, v7_prev: F32,
    e12: F32, e26: F32,
    u9: F32, d9: F32, u14: F32, d14: F32, u24: F32, d24: F32, r9p: F32, r14p: F32, r24p: F32,
    lo_bin: U32, hi_bin: U32, w1: F32, w2: F32, rt1: F32, rt2: F32, rt_a: F32, rt_b: F32, rt_c: F32,
    vol_e: F32, sv_e: F32, hype: F32, flow: F32, pos14: F32, neg14: F32, tp_prev: F32,
    c21: F32, c34: F32, c55: F32, c200: F32, c21_prev: F32,
    apoc: U32,
    started: Bool
  }

def st0() -> St:
  St{50.0, 50.0, 0.0, 0.0,
     0.0, 0.0, 0.0,
     0.0, 0.0,
     0.0, 0.0, InForce{}, InForce{}, InForce{}, InForce{},
     0.0, 0.0, 0.0, 0.0,
     0.0, 0.0,
     0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 50.0, 50.0, 50.0,
     4294967295, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
     0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
     0.0, 0.0, 0.0, 0.0, 0.0,
     0,
     False{}}
# the fold carries: bars, chips (F32 bins), aux (per-bar F32: stride 2 → 0 atr, 1 trip), out (U32, stride 2), state

type Env is Type:
  Env{bars: Array<U32>, chips: Array<F32>, aux: Array<F32>, out: Array<U32>, st: St}
# ═══════════════════════ chip distribution (MCDX / CD): log-price bins of width 0.5 % ═══════════════════════

def bin_of(+p: F32) -> U32:
  F32.to_u32((F32.log(p) / 0.0049875415 + 2048.0 : F32))

def bin_lo(+p: F32) -> U32:
  F32.to_u32((F32.log(p) / 0.0049875415 + 2048.5 : F32))

def bin_hi(+p: F32) -> U32:
  F32.to_u32((F32.log(p) / 0.0049875415 + 2047.5 : F32))

def chips.add1(r: Array<F32> & F32, +b: U32, +per: F32) -> Array<F32>:
  (chips, old) = r
  Array.set(F32, chips, b, (old + per : F32))

def chips.add(n: Nat, chips: Array<F32>, +b: U32, +per: F32) -> Array<F32>:
  match n:
    case 0n:
      chips
    case 1n+p:
      chips.add(p, chips.add1(Array.get(F32, chips, b), b, per), (b + 1 : U32), per)

def chips.range(+bl: F32, +bh: F32, +v: F32, chips: Array<F32>, +sign: F32) -> Array<F32>:
  +b0 = bin_of(bl)
  b1 = bin_of(bh)
  +cnt = (b1 - b0 + 1 : U32)
  chips.add(U32.to_nat(cnt), chips, b0, (sign * v / U32.to_f32(cnt) : F32))
# scan bins [b, b+n): totals, below-threshold, above-threshold, max bin

type Scan is Data:
  Scan{tot: F32, below: F32, above: F32, vmax: F32, bmax: U32}

def scan.one(r: Array<F32> & F32, +b: U32, +lo_t: U32, +hi_t: U32, s: Scan) -> Array<F32> & Scan:
  (chips, v) = r
  match s:
    case Scan{+tot, +below, +above, +vmax, +bmax}:
      +vv = F32.max(v, 0.0)
      +is_max = F32.is_gt(vv, vmax)
      (chips, Scan{(tot + vv : F32), (below + sel(U32.is_lt(b, lo_t), vv, 0.0) : F32), (above + sel(U32.is_gt(b, hi_t), vv, 0.0) : F32), sel(is_max, vv, vmax), sel_u(is_max, b, bmax)})

def scan.step(r: Array<F32> & Scan, +b: U32, +lo_t: U32, +hi_t: U32) -> Array<F32> & Scan:
  (chips, s) = r
  scan.one(Array.get(F32, chips, b), b, lo_t, hi_t, s)

def scan(n: Nat, +b: U32, +lo_t: U32, +hi_t: U32, r: Array<F32> & Scan) -> Array<F32> & Scan:
  match n:
    case 0n:
      r
    case 1n+p:
      scan(p, (b + 1 : U32), lo_t, hi_t, scan.step(r, b, lo_t, hi_t))
# ═══════════════════════ pivots: is bar (i-4) the strict highest / lowest of [i-8, i]? ═══════════════════════

def sel_b(b: Bool, +x: Bool, +y: Bool) -> Bool:
  match b:
    case True{}:
      x
    case False{}:
      y
# ═══════════════════════ one bar of the fold ═══════════════════════

def piv.fin(r: Array<U32> & F32, +center: F32, is_high: Bool) -> Array<U32> & Bool:
  (bars, +ext) = r
  (bars, sel_b(is_high, F32.is_ge(center, ext), F32.is_le(center, ext)))

def aux_trip.fin(r: Array<F32> & F32, acc: List<&2, F32>) -> Array<F32> & List<&2, F32>:
  (aux, t) = r
  (aux, t <> acc)

def aux_trip.step(r: Array<F32> & List<&2, F32>, +j: U32) -> Array<F32> & List<&2, F32>:
  (aux, acc) = r
  aux_trip.fin(Array.get(F32, aux, (j * 4 + 1 : U32)), acc)

def aux_trip(n: Nat, +j: U32, r: Array<F32> & List<&2, F32>) -> Array<F32> & List<&2, F32>:
  match n:
    case 0n:
      r
    case 1n+p:
      aux_trip(p, (j + 1 : U32), aux_trip.step(r, j))
# least-squares line value at the last point of 6 samples (newest first in the list)

def linreg6(xs: List<&2, F32>) -> F32:
  match xs:
    case Con{+y5, Con{+y4, Con{+y3, Con{+y2, Con{+y1, Con{+y0, _}}}}}}:
      +sy = (y0 + y1 + y2 + y3 + y4 + y5 : F32)
      +sxy = (y1 + 2.0 * y2 + 3.0 * y3 + 4.0 * y4 + 5.0 * y5 : F32)
      +slope = ((6.0 * sxy - 15.0 * sy) / 105.0 : F32)
      +mean = (sy / 6.0 : F32)
      (mean + slope * 2.5 : F32)
    case _:
      0.0

type Upd is Data:
  Upd{st: St, trip: F32, add_v: F32, add_l: F32, add_h: F32, sub_v: F32, sub_l: F32, sub_h: F32, flags: U32, atr_pct: F32, pos_in: F32, neg_in: F32}

def step.fin(r: Array<F32> & Scan, st: St, bars: Array<U32>, aux: Array<F32>, out: Array<U32>, +trip: F32, +flags: U32, +atr_pct: F32, +pos_in: F32, +neg_in: F32, +c: F32, +i: U32) -> Env:
  (chips, s) = r
  match s st:
    case Scan{+tot, +below, +above, +vmax, +bmax} St{+k, +d, +red_hi, +red_lo, +rib8, +rib20, +rib20_8, +atr, +prev_c, +h_up, +h_dn, +h_state, +hs1, +hs2, +hs3, +e21, +e34, +e68, +v7_prev, +e12, +e26, +u9, +d9, +u14, +d14, +u24, +d24, +r9p, +r14p, +r24p, +lo_bin, +hi_bin, +w1, +w2, +rt1, +rt2, +rt_a, +rt_b, +rt_c, +vol_e, +sv_e, +hype, +flow, +pos14, +neg14, +tp_prev, +c21, +c34, +c55, +c200, +c21_prev, +apoc, started}:
      +w_raw = sel(F32.is_gt(tot, 0.0), (below / tot * 100.0 : F32), 0.0)
      +r_raw = sel(F32.is_gt(tot, 0.0), (above / tot * 100.0 : F32), 0.0)
      +whales = ((w_raw + w1 + w2) / 3.0 : F32)
      +retail = ((r_raw + rt1 + rt2) / 3.0 : F32)
      +retail_shrinking = F32.is_lt(retail, rt_a)
      +poc = F32.exp(((U32.to_f32(bmax) - 2048.0 + 0.5) * 0.0049875415 : F32))
      +poc_bull = Bool.and(F32.is_gt(c, poc), U32.is_eq(apoc, 1))
      +mcdx = F32.is_ge(whales, 50.0)
      +flags2 = (flags .|. bit(mcdx, 4) .|. bit(poc_bull, 512) .|. U32.shln(F32.to_u32(F32.min((whales + 0.5 : F32), 100.0)), 16n) .|. U32.shln(panels.b(bit_on(flags, 128), bit_on(flags, 256), bit_on(flags, 1), bit_on(flags, 8), bit_on(flags, 1024), poc_bull, Bool.and(mcdx, retail_shrinking)), 24n) : U32)
      +st2 = {St{k, d, red_hi, red_lo, rib8, rib20, rib20_8, atr, prev_c, h_up, h_dn, h_state, hs1, hs2, hs3, e21, e34, e68, v7_prev, e12, e26, u9, d9, u14, d14, u24, d24, r9p, r14p, r24p, lo_bin, hi_bin, w_raw, w1, r_raw, rt1, rt_b, rt_c, retail, vol_e, sv_e, hype, flow, pos14, neg14, tp_prev, c21, c34, c55, c200, c21_prev, one(F32.is_gt(c, poc)), started} : St}
      aux2 = Array.set(F32, aux, (i * 4 : U32), atr)
      aux3 = Array.set(F32, aux2, (i * 4 + 1 : U32), trip)
      aux4 = Array.set(F32, aux3, (i * 4 + 2 : U32), pos_in)
      aux5 = Array.set(F32, aux4, (i * 4 + 3 : U32), neg_in)
      out2 = out[(i * 2 : U32)] <- flags2
      out3 = out2[(i * 2 + 1 : U32)] <- F32.to_u32((atr_pct * 1000.0 : F32))
      Env{bars, chips, aux5, out3, st2}

def step.scan(st: St, chips: Array<F32>, bars: Array<U32>, aux: Array<F32>, out: Array<U32>, +trip: F32, +flags: U32, +atr_pct: F32, +pos_in: F32, +neg_in: F32, +i: U32) -> Env:
  match st:
    case St{+k, +d, +red_hi, +red_lo, +rib8, +rib20, +rib20_8, +atr, +prev_c, +h_up, +h_dn, +h_state, +hs1, +hs2, +hs3, +e21, +e34, +e68, +v7_prev, +e12, +e26, +u9, +d9, +u14, +d14, +u24, +d24, +r9p, +r14p, +r24p, +lo_bin, +hi_bin, +w1, +w2, +rt1, +rt2, +rt_a, +rt_b, +rt_c, +vol_e, +sv_e, +hype, +flow, +pos14, +neg14, +tp_prev, +c21, +c34, +c55, +c200, +c21_prev, +apoc, started}:
      +bl = bin_lo((prev_c * 0.96 : F32))
      +bh = bin_hi((prev_c * 1.04 : F32))
      +lo2 = sel_u(U32.is_lt(bin_of((prev_c * 0.5 : F32)), lo_bin), bin_of((prev_c * 0.5 : F32)), lo_bin)
      +hi2 = sel_u(U32.is_gt(bin_of((prev_c * 2.0 : F32)), hi_bin), bin_of((prev_c * 2.0 : F32)), hi_bin)
      +st2 = {St{k, d, red_hi, red_lo, rib8, rib20, rib20_8, atr, prev_c, h_up, h_dn, h_state, hs1, hs2, hs3, e21, e34, e68, v7_prev, e12, e26, u9, d9, u14, d14, u24, d24, r9p, r14p, r24p, lo2, hi2, w1, w2, rt1, rt2, rt_a, rt_b, rt_c, vol_e, sv_e, hype, flow, pos14, neg14, tp_prev, c21, c34, c55, c200, c21_prev, apoc, started} : St}
      step.fin(scan(U32.to_nat((hi2 - lo2 + 1 : U32)), lo2, bl, bh, (chips, Scan{0.0, 0.0, 0.0, 0.0, 0})), st2, bars, aux, out, trip, flags, atr_pct, pos_in, neg_in, prev_c, i)

def step.chips(u: Upd, chips: Array<F32>, bars: Array<U32>, aux: Array<F32>, out: Array<U32>, +i: U32) -> Env:
  match u:
    case Upd{st, +trip, +add_v, +add_l, +add_h, +sub_v, +sub_l, +sub_h, +flags, +atr_pct, +pos_in, +neg_in}:
      chips2 = chips.range(add_l, add_h, add_v, chips, 1.0)
      chips3 = chips.range(sub_l, sub_h, sub_v, chips2, (0.0 - sel(F32.is_gt(sub_v, 0.0), 1.0, 0.0) : F32))
      step.scan(st, chips3, bars, aux, out, trip, flags, atr_pct, pos_in, neg_in, i)

def rsi_of(+u: F32, +d: F32) -> F32:
  sel(F32.is_gt(d, 0.0), (100.0 - 100.0 / (1.0 + u / d) : F32), sel(F32.is_gt(u, 0.0), 100.0, 50.0))
# ── chips: add this bar, remove the bar 120 ago, scan, then finish the flags and write outputs ──

type Tr is Type:
  Tr{bars: Array<U32>, out: Array<U32>, nxt: Array<U32>, acc: List<&2, U32>, prev_c: Bool, prev_r: Bool, cnt: U32}

def coin(+d: U32, +n: U32) -> Bool:
  +h = (d * 2654435761 + n * 40503 : U32)
  +h3 = (U32.xor(h, U32.shrn(h, 13n)) * 1274126177 : U32)
  U32.is_lt((U32.shrn(h3, 8n) .&. 255 : U32), 31)

def sig_c(+f: U32) -> Bool:
  U32.is_eq((f .&. 125 : U32), 61)

type Tq is Type:
  Tq{bars: Array<U32>, out: Array<U32>, nxt: Array<U32>, acc: List<&2, U32>}

type Bk is Data:
  Bk{done: U32, r24: F32, r34: F32, s53: F32, s54: F32, hh: F32, ll: F32, c10: F32, cl: F32}

def bk.upd(s: Bk, b: Bar, +ent: F32, +atr: F32, +k_rel: U32) -> Bk:
  match s b:
    case Bk{+done, +r24, +r34, +s53, +s54, +hh, +ll, +c10, +cl} Bar{+o, +h, +l, +c, +v}:
      +hit_s3 = F32.is_le(l, (ent - 3.0 * atr : F32))
      +hit_s4 = F32.is_le(l, (ent - 4.0 * atr : F32))
      +hit_th = F32.is_ge(h, (ent + 0.5 * atr : F32))
      +hit_t2 = F32.is_ge(h, (ent + 2.0 * atr : F32))
      +hit_t3 = F32.is_ge(h, (ent + 3.0 * atr : F32))
      +d0 = U32.is_eq((done .&. 1 : U32), 1)
      +d1 = U32.is_eq((done .&. 2 : U32), 2)
      +d2 = U32.is_eq((done .&. 4 : U32), 4)
      +d3 = U32.is_eq((done .&. 8 : U32), 8)
      +in10 = U32.is_le(k_rel, 10)
      +r24_2 = bk.pick(d0, hit_s4, hit_t2, r24, (0.0 - 4.0 : F32), 2.0)
      +r34_2 = bk.pick(d1, hit_s4, hit_t3, r34, (0.0 - 4.0 : F32), 3.0)
      +s53_2 = bk.pick(Bool.or(d2, Bool.not(in10)), hit_s3, hit_th, s53, (0.0 - 3.0 : F32), 0.5)
      +s54_2 = bk.pick(d3, hit_s4, hit_th, s54, (0.0 - 4.0 : F32), 0.5)
      +n0 = bit(Bool.or(d0, Bool.or(hit_s4, hit_t2)), 1)
      +n1 = bit(Bool.or(d1, Bool.or(hit_s4, hit_t3)), 2)
      +n2 = bit(Bool.or(d2, Bool.and(in10, Bool.or(hit_s3, hit_th))), 4)
      +n3 = bit(Bool.or(d3, Bool.or(hit_s4, hit_th)), 8)
      Bk{(n0 .|. n1 .|. n2 .|. n3 : U32), r24_2, r34_2, s53_2, s54_2, F32.max(hh, h), F32.min(ll, l), sel(in10, c, c10), c}

def bk.bar(r: Array<U32> & Bar, s: Bk, +ent: F32, +atr: F32, +k_rel: U32) -> Array<U32> & Bk:
  (bars, b) = r
  (bars, bk.upd(s, b, ent, atr, k_rel))

def bk.step(r: Array<U32> & Bk, +k: U32, +ent: F32, +atr: F32, +i: U32) -> Array<U32> & Bk:
  (bars, s) = r
  bk.bar(bar(bars, k), s, ent, atr, (k - i : U32))

def bk(fuel: Nat, +k: U32, r: Array<U32> & Bk, +ent: F32, +atr: F32, +i: U32) -> Array<U32> & Bk:
  match fuel:
    case 0n:
      r
    case 1n+p:
      bk(p, (k + 1 : U32), bk.step(r, k, ent, atr, i), ent, atr, i)

def enc(+x: F32) -> U32:
  F32.to_u32(((x + 10000.0) * 100.0 : F32))

def trade.rec(r: Array<U32> & Bk, out: Array<U32>, nxt: Array<U32>, acc: List<&2, U32>, +f: U32, +atrw: U32, +ow: U32, +ent: F32, +atr: F32, +cj: F32, +date: U32, +i: U32, +j: U32) -> Tq:
  (bars, s) = r
  match s:
    case Bk{+done, +r24, +r34, +s53, +s54, +hh, +ll, +c10, +cl}:
      +last = ((cl - ent) / atr : F32)
      +last10 = ((c10 - ent) / atr : F32)
      +r24f = sel(U32.is_eq((done .&. 1 : U32), 1), r24, last)
      +r34f = sel(U32.is_eq((done .&. 2 : U32), 2), r34, last)
      +s53f = sel(U32.is_eq((done .&. 4 : U32), 4), s53, last10)
      +s54f = sel(U32.is_eq((done .&. 8 : U32), 8), s54, last)
      +flip = ((cj / ent - 1.0) * 100.0 : F32)
      +t10 = ((c10 / ent - 1.0) * 100.0 : F32)
      +mfe = ((hh - ent) / atr : F32)
      +mae = ((ll - ent) / atr : F32)
      Tq{bars, out, nxt, 0 <> (j <> (enc(t10) <> (enc(mae) <> (enc(mfe) <> (enc(s54f) <> (enc(s53f) <> (enc(r34f) <> (enc(r24f) <> (enc(flip) <> ((j - i : U32) <> (ow <> (atrw <> (f <> (date <> (i <> acc)))))))))))))))}

def trade.close(r: Array<U32> & U32, out: Array<U32>, nxt: Array<U32>, acc: List<&2, U32>, +f: U32, +atrw: U32, +ow: U32, +date: U32, +i: U32, +j: U32, +n: U32) -> Tq:
  (bars, +cjw) = r
  +ent = px(ow)
  +atr = F32.max((ent * U32.to_f32(atrw) / 100000.0 : F32), (ent * 0.0001 : F32))
  +kmax = U32.min((i + 20 : U32), (n - 1 : U32))
  trade.rec(bk(U32.to_nat((kmax - i : U32)), (i + 1 : U32), (bars, Bk{0, 0.0, 0.0, 0.0, 0.0, 0.0, 1000000000.0, 0.0, 0.0}), ent, atr, i), out, nxt, acc, f, atrw, ow, ent, atr, px(cjw), date, i, j)

def trade.nxt(r: Array<U32> & U32, bars: Array<U32>, out: Array<U32>, acc: List<&2, U32>, +f: U32, +atrw: U32, +ow: U32, +date: U32, +i: U32, +n: U32) -> Tq:
  (nxt, +j) = r
  trade.close(at(bars, j, 4), out, nxt, acc, f, atrw, ow, date, i, j, n)

def trade.open(r: Array<U32> & U32, out: Array<U32>, nxt: Array<U32>, acc: List<&2, U32>, +f: U32, +atrw: U32, +date: U32, +i: U32, +n: U32) -> Tq:
  (bars, +ow) = r
  trade.nxt(nxt[(i + 1 : U32)], bars, out, acc, f, atrw, ow, date, i, n)

def trade(bars: Array<U32>, out: Array<U32>, nxt: Array<U32>, acc: List<&2, U32>, +f: U32, +atrw: U32, +date: U32, +i: U32, +n: U32) -> Tq:
  trade.open(at(bars, (i + 1 : U32), 1), out, nxt, acc, f, atrw, date, i, n)

def tr.go(go: Bool, q: Tq, +f: U32, +atrw: U32, +date: U32, +i: U32, +n: U32) -> Tq:
  match go:
    case True{}:
      match q:
        case Tq{bars, out, nxt, acc}:
          trade(bars, out, nxt, acc, f, atrw, date, i, n)
    case False{}:
      q

def tr.wrap(q: Tq, +c_on: Bool, +r_on: Bool, +cnt: U32) -> Tr:
  match q:
    case Tq{bars, out, nxt, acc}:
      Tr{bars, out, nxt, acc, c_on, r_on, cnt}

def tr.atr(r: Array<U32> & U32, bars: Array<U32>, nxt: Array<U32>, acc: List<&2, U32>, +f: U32, +go_c: Bool, +go_r: Bool, +c_on: Bool, +r_on: Bool, +cnt: U32, +date: U32, +i: U32, +n: U32) -> Tr:
  (out, +atrw) = r
  tr.wrap(tr.go(go_r, tr.go(go_c, Tq{bars, out, nxt, acc}, f, atrw, date, i, n), (f .|. 1073741824 : U32), atrw, date, i, n), c_on, r_on, (cnt + one(go_c) + one(go_r) : U32))

def tr.flags(r: Array<U32> & U32, bars: Array<U32>, nxt: Array<U32>, acc: List<&2, U32>, +prev_c: Bool, +prev_r: Bool, +cnt: U32, +date: U32, +i: U32, +n: U32) -> Tr:
  (out, +f) = r
  +c_on = sig_c(f)
  +r_on = coin(date, n)
  +room = U32.is_lt((i + 2 : U32), n)
  tr.atr(out[(i * 2 + 1 : U32)], bars, nxt, acc, f, Bool.and(room, Bool.and(c_on, Bool.not(prev_c))), Bool.and(room, Bool.and(r_on, Bool.not(prev_r))), c_on, r_on, cnt, date, i, n)

def tr.date(r: Array<U32> & U32, out: Array<U32>, nxt: Array<U32>, acc: List<&2, U32>, +prev_c: Bool, +prev_r: Bool, +cnt: U32, +i: U32, +n: U32) -> Tr:
  (bars, +date) = r
  tr.flags(out[(i * 2 : U32)], bars, nxt, acc, prev_c, prev_r, cnt, date, i, n)

def tr.step(t: Tr, +i: U32, +n: U32) -> Tr:
  match t:
    case Tr{bars, out, nxt, acc, +prev_c, +prev_r, +cnt}:
      tr.date(at(bars, i, 0), out, nxt, acc, prev_c, prev_r, cnt, i, n)

def tr(fuel: Nat, +i: U32, +n: U32, t: Tr) -> Tr:
  match fuel:
    case 0n:
      t
    case 1n+p:
      tr(p, (i + 1 : U32), n, tr.step(t, i, n))

def update(st: St, +o: F32, +h: F32, +l: F32, +c: F32, +v: F32, old: Bar, trips: List<&2, F32>, +atr_piv: F32, +pos_out: F32, +neg_out: F32, +is_ph: Bool, +is_pl: Bool, +hh9: F32, +ll9: F32, +sma10: F32, +i: U32) -> Upd:
  match st:
    case St{+k, +d, +red_hi, +red_lo, +rib8, +rib20, +rib20_8, +atr, +prev_c, +h_up, +h_dn, +h_state, +hs1, +hs2, +hs3, +e21, +e34, +e68, +v7_prev, +e12, +e26, +u9, +d9, +u14, +d14, +u24, +d24, +r9p, +r14p, +r24p, +lo_bin, +hi_bin, +w1, +w2, +rt1, +rt2, +rt_a, +rt_b, +rt_c, +vol_e, +sv_e, +hype, +flow, +pos14, +neg14, +tp_prev, +c21, +c34, +c55, +c200, +c21_prev, +apoc, started}:
      match old:
        case Bar{+oo, +ho, +lo, +co, +vo}:
          +first = Bool.not(started)
          +f = sel(first, 1.0, 0.0)
          # ── KDJ ──
          +rng = (hh9 - ll9 : F32)
          +rsv = sel(F32.is_gt(rng, 0.0), ((c - ll9) / rng * 100.0 : F32), 50.0)
          +k2 = sel(first, rsv, ((rsv + 2.0 * k) / 3.0 : F32))
          +d2 = sel(first, rsv, ((k2 + 2.0 * d) / 3.0 : F32))
          +j_prev = (3.0 * k - 2.0 * d : F32)
          +j = (3.0 * k2 - 2.0 * d2 : F32)
          +cross_up = Bool.and(F32.is_gt(j, 50.0), F32.is_le(j_prev, 50.0))
          +red_hi2 = sel(cross_up, h, red_hi)
          +red_lo2 = sel(cross_up, l, red_lo)
          +ss_confirm = Bool.and(F32.is_gt(j, 50.0), Bool.and(F32.is_gt(red_hi2, 0.0), Bool.and(F32.is_gt(c, red_hi2), Bool.not(F32.is_lt(c, red_lo2)))))
          # ── ribbon ──
          +src = ((c + l) / 2.0 : F32)
          +rib8_2 = sel(first, src, ema(rib8, src, 8.0))
          +rib20_2 = sel(first, src, ema(rib20, src, 20.0))
          +rib20_8_2 = sel(first, src, ema(rib20_8, rib20_2, 8.0))
          +ribbon = F32.is_gt(rib8_2, rib20_8_2)
          # ── ATR ──
          +trng = sel(first, (h - l : F32), F32.max((h - l : F32), F32.max(F32.abs((h - prev_c : F32)), F32.abs((l - prev_c : F32)))))
          +atr2 = sel(first, trng, rma(atr, trng, 14.0))
          # ── hole machine (pivot strength 4, k = 1.4) ──
          +new_piv = Bool.or(is_ph, is_pl)
          +h_up2 = sel(new_piv, (sma10 + 1.4 * atr_piv : F32), h_up)
          +h_dn2 = sel(new_piv, (sma10 - 1.4 * atr_piv : F32), h_dn)
          +has_level = F32.is_gt(h_up2, 0.0)
          +two_above = Bool.and(has_level, Bool.and(F32.is_gt(c, h_up2), F32.is_gt(prev_c, h_up2)))
          +two_below = Bool.and(has_level, Bool.and(F32.is_lt(c, h_dn2), F32.is_lt(prev_c, h_dn2)))
          +h_state2 = hole.next(new_piv, two_above, two_below, h_state)
          +hole_up = hole.is_up(h_state2)
          +hole_dn = hole.is_dn(h_state2)
          +fresh = Bool.and(hole_up, Bool.not(hole.is_up(hs3)))
          # ── TE line ──
          +ohlc4 = ((o + h + l + c) / 4.0 : F32)
          +e21_2 = sel(first, ohlc4, ema(e21, ohlc4, 21.0))
          +e34_2 = sel(first, ohlc4, ema(e34, ohlc4, 34.0))
          +e68_2 = sel(first, ohlc4, ema(e68, ohlc4, 68.0))
          +trip = ((e21_2 + e34_2 + e68_2) / 3.0 : F32)
          +v7_now = linreg6(trip <> trips)
          +te_bull = F32.is_gt(v7_now, v7_prev)
          # ── MACD & RSI×3 ──
          +e12_2 = sel(first, c, ema(e12, c, 12.0))
          +e26_2 = sel(first, c, ema(e26, c, 26.0))
          +macd_bull = F32.is_gt((e12_2 - e26_2 : F32), 0.0)
          +chg = (c - prev_c : F32)
          +up = F32.max(chg, 0.0)
          +dn = F32.max((0.0 - chg : F32), 0.0)
          +u9_2 = sel(first, 0.0, rma(u9, up, 9.0))
          +d9_2 = sel(first, 0.0, rma(d9, dn, 9.0))
          +u14_2 = sel(first, 0.0, rma(u14, up, 14.0))
          +d14_2 = sel(first, 0.0, rma(d14, dn, 14.0))
          +u24_2 = sel(first, 0.0, rma(u24, up, 24.0))
          +d24_2 = sel(first, 0.0, rma(d24, dn, 24.0))
          +r9 = rsi_of(u9_2, d9_2)
          +r14 = rsi_of(u14_2, d14_2)
          +r24 = rsi_of(u24_2, d24_2)
          +rsi_up = Bool.and(F32.is_gt(r9, r9p), Bool.and(F32.is_gt(r14, r14p), Bool.and(F32.is_gt(r24, r24p), F32.is_gt(r14, 50.0))))
          # ── Diamond vetoes: hype, money flow, gold band, mountain, bubble ──
          +signed = sel(F32.is_gt(c, prev_c), v, sel(F32.is_lt(c, prev_c), (0.0 - v : F32), 0.0))
          +vol_e2 = sel(first, v, ema(vol_e, v, 10.0))
          +sv_e2 = sel(first, signed, ema(sv_e, signed, 10.0))
          +hype_raw = sel(F32.is_gt(vol_e2, 0.0), (100.0 * sv_e2 / vol_e2 / 2.0 : F32), 0.0)
          +hype2 = sel(first, hype_raw, ema(hype, hype_raw, 3.0))
          +tp = ((h + l + c) / 3.0 : F32)
          +raw = (tp * v : F32)
          +pos_in = sel(F32.is_gt(tp, tp_prev), raw, 0.0)
          +neg_in = sel(F32.is_lt(tp, tp_prev), raw, 0.0)
          +pos14_2 = (pos14 + pos_in - pos_out : F32)
          +neg14_2 = (neg14 + neg_in - neg_out : F32)
          +mfi = sel(F32.is_gt(neg14_2, 0.0), (100.0 - 100.0 / (1.0 + pos14_2 / neg14_2) : F32), 100.0)
          +flow_raw = (mfi - 50.0 : F32)
          +flow2 = sel(first, flow_raw, ema(flow, flow_raw, 3.0))
          +c21_2 = sel(first, c, ema(c21, c, 21.0))
          +c34_2 = sel(first, c, ema(c34, c, 34.0))
          +c55_2 = sel(first, c, ema(c55, c, 55.0))
          +c200_2 = sel(first, c, ema(c200, c, 200.0))
          +gold_top = F32.max(c21_2, F32.max(c34_2, c55_2))
          +above_gold = F32.is_gt(c, gold_top)
          +band_up = Bool.and(F32.is_gt(c21_2, c55_2), F32.is_gt(c21_2, c21_prev))
          +mountain = (one(F32.is_gt(hype2, 0.0)) + one(F32.is_gt(flow2, 0.0)) + one(above_gold) + one(band_up) : U32)
          +bubble = (F32.max(0.0, ((c - c200_2) / atr2 : F32)) + sel(F32.is_gt(hype2, 25.0), 1.0, 0.0) + sel(F32.is_gt(flow2, 25.0), 1.0, 0.0) : F32)
          +hot = F32.is_ge(bubble, 6.0)
          # ── flags (chips-dependent bits are filled in step.chips) ──
          +flags = (bit(hole_up, 1) .|. bit(hole_dn, 2) .|. bit(te_bull, 8) .|. bit(U32.is_ge(mountain, 3), 16) .|. bit(above_gold, 32) .|. bit(hot, 64) .|. bit(ss_confirm, 128) .|. bit(ribbon, 256) .|. bit(Bool.and(macd_bull, rsi_up), 1024) .|. bit(fresh, 2048) .|. bit(is_ph, 4096) .|. bit(is_pl, 8192) : U32)
          +st2 = {St{k2, d2, red_hi2, red_lo2, rib8_2, rib20_2, rib20_8_2, atr2, c, h_up2, h_dn2, h_state2, h_state2, hs1, hs2, e21_2, e34_2, e68_2, v7_now, e12_2, e26_2, u9_2, d9_2, u14_2, d14_2, u24_2, d24_2, r9, r14, r24, lo_bin, hi_bin, w1, w2, rt1, rt2, rt_a, rt_b, rt_c, vol_e2, sv_e2, hype2, flow2, pos14_2, neg14_2, tp, c21_2, c34_2, c55_2, c200_2, c21_2, apoc, True{}} : St}
          Upd{st2, trip, v, l, h, sel(U32.is_ge(i, 120), vo, 0.0), lo, ho, flags, (atr2 / c * 100.0 : F32), pos_in, neg_in}

def step.upd(st: St, b: Bar, old: Bar, trips: List<&2, F32>, +atr_piv: F32, +pos_out: F32, +neg_out: F32, is_ph: Bool, is_pl: Bool, +hh9: F32, +ll9: F32, +sma10: F32, chips: Array<F32>, bars: Array<U32>, aux: Array<F32>, out: Array<U32>, +i: U32) -> Env:
  match b:
    case Bar{+o, +h, +l, +c, +v}:
      step.chips(update(st, o, h, l, c, v, old, trips, atr_piv, pos_out, neg_out, is_ph, is_pl, hh9, ll9, sma10, i), chips, bars, aux, out, i)

def step.trip(r: Array<F32> & List<&2, F32>, +pos_out: F32, +neg_out: F32, +atr_piv: F32, old: Bar, is_ph: Bool, is_pl: Bool, +hh9: F32, +ll9: F32, +sma10: F32, b: Bar, chips: Array<F32>, bars: Array<U32>, out: Array<U32>, st: St, +i: U32) -> Env:
  (aux, trips) = r
  step.upd(st, b, old, trips, atr_piv, pos_out, neg_out, is_ph, is_pl, hh9, ll9, sma10, chips, bars, aux, out, i)
# ── the state update: everything that does not touch the chips array ──

def step.neg(r: Array<F32> & F32, +pos_out: F32, +atr_piv: F32, old: Bar, is_ph: Bool, is_pl: Bool, +hh9: F32, +ll9: F32, +sma10: F32, b: Bar, chips: Array<F32>, bars: Array<U32>, out: Array<U32>, st: St, +i: U32) -> Env:
  (aux, +neg_out) = r
  step.trip(aux_trip(winp(i, 5), startp(i, 5), (aux, Nil{})), pos_out, sel(U32.is_ge(i, 14), neg_out, 0.0), atr_piv, old, is_ph, is_pl, hh9, ll9, sma10, b, chips, bars, out, st, i)
# ATR at the pivot bar (i-4) from aux, then the six trip values for the TE regression

def step.pos(r: Array<F32> & F32, +atr_piv: F32, old: Bar, is_ph: Bool, is_pl: Bool, +hh9: F32, +ll9: F32, +sma10: F32, b: Bar, chips: Array<F32>, bars: Array<U32>, out: Array<U32>, st: St, +i: U32) -> Env:
  (aux, +pos_out) = r
  step.neg(Array.get(F32, aux, (sel_u(U32.is_lt(i, 14), 0, (i - 14 : U32)) * 4 + 3 : U32)), sel(U32.is_ge(i, 14), pos_out, 0.0), atr_piv, old, is_ph, is_pl, hh9, ll9, sma10, b, chips, bars, out, st, i)

def step.aux(r: Array<F32> & F32, old: Bar, is_ph: Bool, is_pl: Bool, +hh9: F32, +ll9: F32, +sma10: F32, b: Bar, chips: Array<F32>, bars: Array<U32>, out: Array<U32>, st: St, +i: U32) -> Env:
  (aux, +atr_piv) = r
  step.pos(Array.get(F32, aux, (sel_u(U32.is_lt(i, 14), 0, (i - 14 : U32)) * 4 + 2 : U32)), atr_piv, old, is_ph, is_pl, hh9, ll9, sma10, b, chips, bars, out, st, i)

def step.old(r: Array<U32> & Bar, is_ph: Bool, is_pl: Bool, +hh9: F32, +ll9: F32, +sma10: F32, b: Bar, chips: Array<F32>, aux: Array<F32>, out: Array<U32>, st: St, +i: U32) -> Env:
  (bars, old) = r
  step.aux(Array.get(F32, aux, (sel_u(U32.is_lt(i, 4), 0, (i - 4 : U32)) * 4 : U32)), old, is_ph, is_pl, hh9, ll9, sma10, b, chips, bars, out, st, i)

def step.cl(r: Array<U32> & U32, +ch: F32, +wh: F32, +wl: F32, +hh9: F32, +ll9: F32, +sma10: F32, b: Bar, chips: Array<F32>, aux: Array<F32>, out: Array<U32>, st: St, +i: U32) -> Env:
  (bars, clw) = r
  +cl = px(clw)
  +piv_ok = U32.is_ge(i, 8)
  +is_ph = Bool.and(piv_ok, F32.is_gt(ch, wh))
  +is_pl = Bool.and(piv_ok, F32.is_lt(cl, wl))
  # the bar 120 ago (for the chip window) and 14 ago (for MFI): read l, h, v of bar i-120 and tp, v of bar i-14
  step.old(bar(bars, (i - sel_u(U32.is_lt(i, 120), i, 120) : U32)), is_ph, is_pl, hh9, ll9, sma10, b, chips, aux, out, st, i)

def step.ch(r: Array<U32> & U32, +wh: F32, +wl: F32, +hh9: F32, +ll9: F32, +sma10: F32, b: Bar, chips: Array<F32>, aux: Array<F32>, out: Array<U32>, st: St, +i: U32) -> Env:
  (bars, chw) = r
  step.cl(at(bars, (i - sel_u(U32.is_lt(i, 8), i, 4) : U32), 3), px(chw), wh, wl, hh9, ll9, sma10, b, chips, aux, out, st, i)

def step.pl(r: Array<U32> & F32, +wh: F32, +hh9: F32, +ll9: F32, +sma10: F32, b: Bar, chips: Array<F32>, aux: Array<F32>, out: Array<U32>, st: St, +i: U32) -> Env:
  (bars, +wl) = r
  # the candidate pivot bar is i-4: read its high and low
  step.ch(at(bars, (i - sel_u(U32.is_lt(i, 8), i, 4) : U32), 2), wh, wl, hh9, ll9, sma10, b, chips, aux, out, st, i)

def step.ph(r: Array<U32> & F32, +hh9: F32, +ll9: F32, +sma10: F32, b: Bar, chips: Array<F32>, aux: Array<F32>, out: Array<U32>, st: St, +i: U32) -> Env:
  (bars, +wh) = r
  +p8 = sel_u(U32.is_lt(i, 8), 0, (i - 8 : U32))
  step.pl(roll(~fmin, 4n, (p8 + 5 : U32), 3, roll(~fmin, 4n, p8, 3, (bars, 1000000000.0))), wh, hh9, ll9, sma10, b, chips, aux, out, st, i)

def step.sma(r: Array<U32> & F32, +hh9: F32, +ll9: F32, b: Bar, chips: Array<F32>, aux: Array<F32>, out: Array<U32>, st: St, +i: U32) -> Env:
  (bars, +sum10) = r
  +p4 = sel_u(U32.is_lt(i, 4), 0, (i - 4 : U32))
  +sma10 = (sum10 / U32.to_f32(sel_u(U32.is_lt(p4, 10), (p4 + 1 : U32), 10)) : F32)
  # pivot windows around bar i-4 (only meaningful when i ≥ 8)
  +p8 = sel_u(U32.is_lt(i, 8), 0, (i - 8 : U32))
  step.ph(roll(~fmax, 4n, (p8 + 5 : U32), 2, roll(~fmax, 4n, p8, 2, (bars, 0.0))), hh9, ll9, sma10, b, chips, aux, out, st, i)

def step.ll(r: Array<U32> & F32, +hh9: F32, b: Bar, chips: Array<F32>, aux: Array<F32>, out: Array<U32>, st: St, +i: U32) -> Env:
  (bars, +ll9) = r
  +p4 = sel_u(U32.is_lt(i, 4), 0, (i - 4 : U32))
  step.sma(roll(~fadd, win(p4, 10), start(p4, 10), 4, (bars, 0.0)), hh9, ll9, b, chips, aux, out, st, i)

def step.hh(r: Array<U32> & F32, b: Bar, chips: Array<F32>, aux: Array<F32>, out: Array<U32>, st: St, +i: U32) -> Env:
  (bars, +hh9) = r
  step.ll(roll(~fmin, win(i, 9), start(i, 9), 3, (bars, 1000000000.0)), hh9, b, chips, aux, out, st, i)

def step.bar(r: Array<U32> & Bar, chips: Array<F32>, aux: Array<F32>, out: Array<U32>, st: St, +i: U32) -> Env:
  (bars, b) = r
  step.hh(roll(~fmax, win(i, 9), start(i, 9), 2, (bars, 0.0)), b, chips, aux, out, st, i)

def step(e: Env, +i: U32) -> Env:
  match e:
    case Env{bars, chips, aux, out, st}:
      step.bar(bar(bars, i), chips, aux, out, st, i)

def fold(fuel: Nat, +i: U32, e: Env) -> Env:
  match fuel:
    case 0n:
      e
    case 1n+p:
      fold(p, (i + 1 : U32), step(e, i))
# ═══════════════════════ per ticker ═══════════════════════

def out_list.fin(r: Array<U32> & U32, acc: List<&2, U32>) -> Array<U32> & List<&2, U32>:
  (out, w) = r
  (out, w <> acc)

def out_list.step(r: Array<U32> & List<&2, U32>, +i: U32) -> Array<U32> & List<&2, U32>:
  (out, acc) = r
  out_list.fin(out[i], acc)

def out_list(n: Nat, +i: U32, r: Array<U32> & List<&2, U32>) -> Array<U32> & List<&2, U32>:
  match n:
    case 0n:
      r
    case 1n+p:
      out_list(p, (i + 1 : U32), out_list.step(r, i))

def out_done(r: Array<U32> & List<&2, U32>) -> List<&2, U32>:
  (out, acc) = r
  List.reverse(&2, U32, acc)
# ═══════════════════════ trades: every rising edge of the C reading, scored under each exit ═══════════════════════
# record = 16 words: 0 signal bar, 1 date, 2 flags (bit 30 = random control), 3 ATR%×1000, 4 entry (next open ×10000),
#   5 bars to the hole-down exit, 6 hole exit %, 7 bracket +2/−4 ATR 20b, 8 +3/−4 20b, 9 scalp +0.5/−3 10b, 10 +0.5/−4 20b,
#   11 MFE 20b, 12 MAE 20b, 13 close-after-10-bars %, 14 hole exit bar, 15 reserved.  7–13 are in ATR units (%, for 6 and 13),
#   encoded (x + 10000) × 100.

type Nx is Type:
  Nx{out: Array<U32>, nxt: Array<U32>, prev: U32}

def nx.fin(r: Array<U32> & U32, nxt: Array<U32>, +prev: U32, +i: U32) -> Nx:
  (out, +f) = r
  +here = sel_u(U32.is_eq((f .&. 2 : U32), 2), i, prev)
  Nx{out, nxt[i] <- here, here}

def nx.step(x: Nx, +i: U32) -> Nx:
  match x:
    case Nx{out, nxt, +prev}:
      nx.fin(out[(i * 2 : U32)], nxt, prev, i)

def nx(fuel: Nat, +i: U32, x: Nx) -> Nx:
  match fuel:
    case 0n:
      x
    case 1n+p:
      nx(p, (i - 1 : U32), nx.step(x, i))

def out_done2(r: Array<U32> & List<&2, U32>, +cnt: U32, tr_acc: List<&2, U32>) -> List<&2, U32>:
  (out, acc) = r
  List.append(&2, U32, List.reverse(&2, U32, acc), cnt <> List.reverse(&2, U32, tr_acc))

def run_tr(t: Tr, +n: U32) -> List<&2, U32>:
  match t:
    case Tr{bars, out, nxt, acc, +prev_c, +prev_r, +cnt}:
      n <> out_done2(out_list(U32.to_nat((n * 2 : U32)), 0, (out, Nil{})), cnt, acc)

def run_nx(x: Nx, bars: Array<U32>, +n: U32) -> List<&2, U32>:
  match x:
    case Nx{out, nxt, +prev}:
      run_tr(tr(U32.to_nat(n), 0, n, Tr{bars, out, nxt, Nil{}, False{}, False{}, 0}), n)

def run_fin(e: Env, +n: U32) -> List<&2, U32>:
  match e:
    case Env{bars, chips, aux, out, st}:
      run_nx(nx(U32.to_nat(n), (n - 1 : U32), Nx{out, Array.new(U32, depth(n), 0), (n - 1 : U32)}), bars, n)

def run_one(t: Array<U32> & U32) -> List<&2, U32>:
  (bars, +n) = t
  run_fin(fold(U32.to_nat(n), 0, Env{bars, Array.new(F32, 12n, 0.0), Array.new(F32, depth((n * 4 : U32)), 0.0), Array.new(U32, depth((n * 2 : U32)), 0), st0()}), n)

def run_last(xs: List<(Array<U32> & U32)>) -> List<&2, List<&2, U32>>:
  match xs:
    case Nil{}:
      Nil{}
    case Con{h, t}:
      [run_one(h)]

type Job is Type:
  One{n: Nat, xs: List<(Array<U32> & U32)>}
  Two{nl: Nat, left: List<(Array<U32> & U32)>, nr: Nat, right: List<(Array<U32> & U32)>}

def split.go(k: Nat, xs: List<(Array<U32> & U32)>, acc: List<(Array<U32> & U32)>, +nl: Nat, +nr: Nat) -> Job:
  match k:
    case 0n:
      Two{nl, List.reverse(&1, (Array<U32> & U32), acc), nr, xs}
    case 1n+p:
      match xs:
        case Nil{}:
          Two{nl, List.reverse(&1, (Array<U32> & U32), acc), nr, Nil{}}
        case Con{h, t}:
          split.go(p, t, h <> acc, nl, nr)

def run_all(+fuel: Nat, job: Job) -> List<&2, List<&2, U32>>:
  match fuel:
    case 0n:
      Nil{}
    case 1n+f:
      match job:
        case Two{nl, left, nr, right}:
          a b = run_all(f, One{nl, left}) run_all(f, One{nr, right})
          List.append(&2, List<&2, U32>, a, b)
        case One{+n, xs}:
          match n:
            case 0n:
              Nil{}
            case 1n+q:
              match q:
                case 0n:
                  run_last(xs)
                case 1n+_:
                  +half = Nat.div(n, 2n)
                  run_all(f, split.go(half, xs, Nil{}, half, Nat.sub(n, half)))
# ═══════════════════════ IO ═══════════════════════

def unwrap_u32(r: Result<&1, &1, U32 & String, U32>) -> U32:
  match r:
    case Done{v}:
      v
    case Fail{e}:
      0

def unwrap_bytes(r: Result<&1, &1, U32 & String, List<&2, U32>>) -> List<&2, U32>:
  match r:
    case Done{v}:
      v
    case Fail{e}:
      Nil{}

def unwrap_str(r: Result<&1, &1, U32 & String, String>) -> String:
  match r:
    case Done{v}:
      v
    case Fail{e}:
      ""

def read_ticker.bytes(fb: File & Result<&1, &1, U32 & String, List<&2, U32>>, +sz: U32) -> IO(Array<U32> & U32):
  (f, br) = fb
  do IO<(Array<U32> & U32)>:
    File.close(f)
    return (to_array(List.reverse(&2, U32, words(unwrap_bytes(br), Nil{})), (sz / 4 : U32)), (sz / 24 : U32))

def read_ticker.sized(fs: File & Result<&1, &1, U32 & String, U32>) -> IO(Array<U32> & U32):
  (f, sr) = fs
  +sz = unwrap_u32(sr)
  do IO<(Array<U32> & U32)>:
    fb : File & Result<&1, &1, U32 & String, List<&2, U32>> <- File.read_bytes(f, sz)
    read_ticker.bytes(fb, sz)

def read_ticker(path: String) -> IO(Array<U32> & U32):
  do IO<(Array<U32> & U32)>:
    f : File <- IO.try(File, File.open(path, "r"))
    fs : File & Result<&1, &1, U32 & String, U32> <- File.size(f)
    read_ticker.sized(fs)

def read_all(paths: List<&2, String>) -> IO(List<(Array<U32> & U32)>):
  match paths:
    case Nil{}:
      IO.pure(List<(Array<U32> & U32)>, Nil{})
    case Con{p, rest}:
      do IO<List<(Array<U32> & U32)>>:
        t : Array<U32> & U32 <- read_ticker(p)
        ts : List<(Array<U32> & U32)> <- read_all(rest)
        return t <> ts

def to_bytes(ws: List<&2, U32>, acc: List<&2, U32>) -> List<&2, U32>:
  match ws:
    case Nil{}:
      List.reverse(&2, U32, acc)
    case Con{+w, rest}:
      b0 = (w .&. 255 : U32)
      b1 = (U32.shrn(w, 8n) .&. 255 : U32)
      b2 = (U32.shrn(w, 16n) .&. 255 : U32)
      b3 = (U32.shrn(w, 24n) .&. 255 : U32)
      to_bytes(rest, b3 <> (b2 <> (b1 <> (b0 <> acc))))

def first_word(parts: List<&2, String>) -> String:
  match parts:
    case Con{name, _}:
      name
    case Nil{}:
      ""

def push_if(b: Bool, item: String, acc: List<&2, String>) -> List<&2, String>:
  match b:
    case True{}:
      item <> acc
    case False{}:
      acc

def paths_of(lines: List<&2, String>, +dir: String, acc: List<&2, String>) -> List<&2, String>:
  match lines:
    case Nil{}:
      List.reverse(&2, String, acc)
    case Con{ln, rest}:
      +name = first_word(String.split(ln, ' '))
      paths_of(rest, dir, push_if(Nat.is_lt(0n, String.length(name)), dir ++ "/" ++ name ++ ".bin", acc))

def main.written(ow: File & Result<&1, &1, U32 & String, Unit>) -> IO(Unit):
  (of, r) = ow
  do IO<Unit>:
    File.close(of)
    IO.print("wrote states.bin")

def main.run(ts: List<(Array<U32> & U32)>, +n: Nat, +t0: Nat, +dir: String) -> IO(Unit):
  flat = List.concat(&2, U32, run_all(Nat.add(n, 64n), One{n, ts}))
  do IO<Unit>:
    t1 : Nat <- IO.now()
    IO.print("fold ms: " ++ Nat.show(Nat.sub(t1, t0)))
    of : File <- IO.try(File, File.open(dir ++ "/states.bin", "w"))
    ow : File & Result<&1, &1, U32 & String, Unit> <- File.write_bytes(of, to_bytes(flat, Nil{}))
    main.written(ow)

def main.listed(lt: File & Result<&1, &1, U32 & String, String>, +dir: String) -> IO(Unit):
  (lf, ltr) = lt
  +paths = paths_of(String.lines(unwrap_str(ltr)), dir, Nil{})
  +n = List.length(&2, String, paths)
  do IO<Unit>:
    File.close(lf)
    IO.print("tickers: " ++ Nat.show(n))
    ts : List<(Array<U32> & U32)> <- read_all(paths)
    t0 : Nat <- IO.now()
    main.run(ts, n, t0, dir)

def main.sized(ls: File & Result<&1, &1, U32 & String, U32>, +dir: String) -> IO(Unit):
  (lf, lsr) = ls
  do IO<Unit>:
    lt : File & Result<&1, &1, U32 & String, String> <- File.read(lf, unwrap_u32(lsr))
    main.listed(lt, dir)

def main.dir(args: List<&1, String>) -> String:
  match args:
    case Con{d, _}:
      d
    case Nil{}:
      "bend/data/D"

def main.start(+dir: String) -> IO(Unit):
  do IO<Unit>:
    lf : File <- IO.try(File, File.open(dir ++ "/list.txt", "r"))
    ls : File & Result<&1, &1, U32 & String, U32> <- File.size(lf)
    main.sized(ls, dir)

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<&1, String> <- IO.args
    main.start(main.dir(args))
