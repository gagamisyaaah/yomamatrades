import Base
import ./SUPER.bend as S

law runner_when_all_hold:
  {S.runner(True{}, True{}) == True{} : Bool}

law runner_needs_hole_up:
  for +hole_up: Bool
  for +fresh: Bool
  {Bool.or(hole_up, Bool.not(S.runner(hole_up, fresh))) == True{} : Bool}

law runner_needs_fresh:
  for +hole_up: Bool
  for +fresh: Bool
  {Bool.or(fresh, Bool.not(S.runner(hole_up, fresh))) == True{} : Bool}

law structure_when_all_hold:
  {S.structure(True{}, True{}, True{}, True{}) == True{} : Bool}

law structure_needs_asym20:
  for +asym20: Bool
  for +clv: Bool
  for +d_pl: Bool
  for +lo50: Bool
  {Bool.or(asym20, Bool.not(S.structure(asym20, clv, d_pl, lo50))) == True{} : Bool}

law structure_needs_clv:
  for +asym20: Bool
  for +clv: Bool
  for +d_pl: Bool
  for +lo50: Bool
  {Bool.or(clv, Bool.not(S.structure(asym20, clv, d_pl, lo50))) == True{} : Bool}

law structure_needs_d_pl:
  for +asym20: Bool
  for +clv: Bool
  for +d_pl: Bool
  for +lo50: Bool
  {Bool.or(d_pl, Bool.not(S.structure(asym20, clv, d_pl, lo50))) == True{} : Bool}

law structure_needs_lo50:
  for +asym20: Bool
  for +clv: Bool
  for +d_pl: Bool
  for +lo50: Bool
  {Bool.or(lo50, Bool.not(S.structure(asym20, clv, d_pl, lo50))) == True{} : Bool}

law force_when_all_hold:
  {S.force(True{}, True{}, True{}, True{}) == True{} : Bool}

law force_needs_eff10:
  for +eff10: Bool
  for +gold: Bool
  for +lr20: Bool
  for +macd_hist: Bool
  {Bool.or(eff10, Bool.not(S.force(eff10, gold, lr20, macd_hist))) == True{} : Bool}

law force_needs_gold:
  for +eff10: Bool
  for +gold: Bool
  for +lr20: Bool
  for +macd_hist: Bool
  {Bool.or(gold, Bool.not(S.force(eff10, gold, lr20, macd_hist))) == True{} : Bool}

law force_needs_lr20:
  for +eff10: Bool
  for +gold: Bool
  for +lr20: Bool
  for +macd_hist: Bool
  {Bool.or(lr20, Bool.not(S.force(eff10, gold, lr20, macd_hist))) == True{} : Bool}

law force_needs_macd_hist:
  for +eff10: Bool
  for +gold: Bool
  for +lr20: Bool
  for +macd_hist: Bool
  {Bool.or(macd_hist, Bool.not(S.force(eff10, gold, lr20, macd_hist))) == True{} : Bool}

law energy_when_all_hold:
  {S.energy(True{}, True{}, True{}, True{}) == True{} : Bool}

law energy_needs_gap:
  for +gap: Bool
  for +metabolic: Bool
  for +range_atr: Bool
  for +st_dist: Bool
  {Bool.or(gap, Bool.not(S.energy(gap, metabolic, range_atr, st_dist))) == True{} : Bool}

law energy_needs_metabolic:
  for +gap: Bool
  for +metabolic: Bool
  for +range_atr: Bool
  for +st_dist: Bool
  {Bool.or(metabolic, Bool.not(S.energy(gap, metabolic, range_atr, st_dist))) == True{} : Bool}

law energy_needs_range_atr:
  for +gap: Bool
  for +metabolic: Bool
  for +range_atr: Bool
  for +st_dist: Bool
  {Bool.or(range_atr, Bool.not(S.energy(gap, metabolic, range_atr, st_dist))) == True{} : Bool}

law energy_needs_st_dist:
  for +gap: Bool
  for +metabolic: Bool
  for +range_atr: Bool
  for +st_dist: Bool
  {Bool.or(st_dist, Bool.not(S.energy(gap, metabolic, range_atr, st_dist))) == True{} : Bool}
