# The claims. Human-owned: states what the engine's decisions must satisfy; PROOF.bend must prove every one.
import Base
import ./dragon.bend as D

law hole_pivot_rearms:
  for +up: Bool
  for +dn: Bool
  for s: D.Hole
  {D.hole.next(True{}, up, dn, s) == D.hole.arm(up, dn) : D.Hole}

law hole_up_sticks:
  for up: Bool
  for dn: Bool
  {D.hole.next(False{}, up, dn, D.BrokeUp{}) == D.BrokeUp{} : D.Hole}

law hole_down_sticks:
  for up: Bool
  for dn: Bool
  {D.hole.next(False{}, up, dn, D.BrokeDown{}) == D.BrokeDown{} : D.Hole}

law hole_armed_checks_break:
  for +up: Bool
  for +dn: Bool
  {D.hole.next(False{}, up, dn, D.InForce{}) == D.hole.arm(up, dn) : D.Hole}

law hole_breaks_up_first:
  for dn: Bool
  {D.hole.arm(True{}, dn) == D.BrokeUp{} : D.Hole}

law hole_breaks_down:
  {D.hole.arm(False{}, True{}) == D.BrokeDown{} : D.Hole}

law hole_holds:
  {D.hole.arm(False{}, False{}) == D.InForce{} : D.Hole}

law hole_never_both:
  for +s: D.Hole
  {Bool.and(D.hole.is_up(s), D.hole.is_dn(s)) == False{} : Bool}

law sig_needs_up:
  for +up: Bool
  for +wh: Bool
  for +te: Bool
  for +hot: Bool
  for +mt: Bool
  for +gd: Bool
  {Bool.or(up, Bool.not(D.sig_c.b(up, wh, te, hot, mt, gd))) == True{} : Bool}

law sig_needs_wh:
  for +up: Bool
  for +wh: Bool
  for +te: Bool
  for +hot: Bool
  for +mt: Bool
  for +gd: Bool
  {Bool.or(wh, Bool.not(D.sig_c.b(up, wh, te, hot, mt, gd))) == True{} : Bool}

law sig_needs_te:
  for +up: Bool
  for +wh: Bool
  for +te: Bool
  for +hot: Bool
  for +mt: Bool
  for +gd: Bool
  {Bool.or(te, Bool.not(D.sig_c.b(up, wh, te, hot, mt, gd))) == True{} : Bool}

law sig_vetoed_when_hot:
  for +up: Bool
  for +wh: Bool
  for +te: Bool
  for +hot: Bool
  for +mt: Bool
  for +gd: Bool
  {Bool.or(Bool.not(hot), Bool.not(D.sig_c.b(up, wh, te, hot, mt, gd))) == True{} : Bool}

law sig_needs_mt:
  for +up: Bool
  for +wh: Bool
  for +te: Bool
  for +hot: Bool
  for +mt: Bool
  for +gd: Bool
  {Bool.or(mt, Bool.not(D.sig_c.b(up, wh, te, hot, mt, gd))) == True{} : Bool}

law sig_needs_gd:
  for +up: Bool
  for +wh: Bool
  for +te: Bool
  for +hot: Bool
  for +mt: Bool
  for +gd: Bool
  {Bool.or(gd, Bool.not(D.sig_c.b(up, wh, te, hot, mt, gd))) == True{} : Bool}

law sig_when_all_hold:
  {D.sig_c.b(True{}, True{}, True{}, False{}, True{}, True{}) == True{} : Bool}

law panels_at_most_seven:
  for +a: Bool
  for +b: Bool
  for +c: Bool
  for +d: Bool
  for +e: Bool
  for +f: Bool
  for +g: Bool
  {U32.is_le(D.panels.b(a, b, c, d, e, f, g), 7) == True{} : Bool}

law panels_all_seven:
  {D.panels.b(True{}, True{}, True{}, True{}, True{}, True{}, True{}) == 7 : U32}

law panels_none:
  {D.panels.b(False{}, False{}, False{}, False{}, False{}, False{}, False{}) == 0 : U32}

law panels_survive_packing:
  for +a: Bool
  for +b: Bool
  for +c: Bool
  for +d: Bool
  for +e: Bool
  for +f: Bool
  for +g: Bool
  {(U32.shrn(U32.shln(D.panels.b(a, b, c, d, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(a, b, c, d, e, f, g) : U32}

law bracket_decided_stays:
  for hs: Bool
  for ht: Bool
  for +cur: F32
  for s: F32
  for t: F32
  {D.bk.pick(True{}, hs, ht, cur, s, t) == cur : F32}

law bracket_stop_before_target:
  for ht: Bool
  for cur: F32
  for +s: F32
  for t: F32
  {D.bk.pick(False{}, True{}, ht, cur, s, t) == s : F32}

law bracket_target:
  for cur: F32
  for s: F32
  for +t: F32
  {D.bk.pick(False{}, False{}, True{}, cur, s, t) == t : F32}

law bracket_untouched_keeps_running_value:
  for +cur: F32
  for s: F32
  for t: F32
  {D.bk.pick(False{}, False{}, False{}, cur, s, t) == cur : F32}
