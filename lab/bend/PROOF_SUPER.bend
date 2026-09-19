import Base
import ./SUPER.bend as S
import ./LAWS_SUPER.bend as Laws

def pf.runner.fresh.0(+hole_up: Bool) -> {Bool.or(False{}, Bool.not(S.runner(hole_up, False{}))) == True{} : Bool}:
  match hole_up:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.structure.clv.0(+asym20: Bool, +d_pl: Bool, +lo50: Bool) -> {Bool.or(False{}, Bool.not(S.structure(asym20, False{}, d_pl, lo50))) == True{} : Bool}:
  match asym20:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.structure.d_pl.1(+clv: Bool, +lo50: Bool) -> {Bool.or(False{}, Bool.not(S.structure(True{}, clv, False{}, lo50))) == True{} : Bool}:
  match clv:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.structure.d_pl.0(+asym20: Bool, +clv: Bool, +lo50: Bool) -> {Bool.or(False{}, Bool.not(S.structure(asym20, clv, False{}, lo50))) == True{} : Bool}:
  match asym20:
    case False{}:
      {==}
    case True{}:
      pf.structure.d_pl.1(clv, lo50)

def pf.structure.lo50.2(+d_pl: Bool) -> {Bool.or(False{}, Bool.not(S.structure(True{}, True{}, d_pl, False{}))) == True{} : Bool}:
  match d_pl:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.structure.lo50.1(+clv: Bool, +d_pl: Bool) -> {Bool.or(False{}, Bool.not(S.structure(True{}, clv, d_pl, False{}))) == True{} : Bool}:
  match clv:
    case False{}:
      {==}
    case True{}:
      pf.structure.lo50.2(d_pl)

def pf.structure.lo50.0(+asym20: Bool, +clv: Bool, +d_pl: Bool) -> {Bool.or(False{}, Bool.not(S.structure(asym20, clv, d_pl, False{}))) == True{} : Bool}:
  match asym20:
    case False{}:
      {==}
    case True{}:
      pf.structure.lo50.1(clv, d_pl)

def pf.force.gold.0(+eff10: Bool, +lr20: Bool, +macd_hist: Bool) -> {Bool.or(False{}, Bool.not(S.force(eff10, False{}, lr20, macd_hist))) == True{} : Bool}:
  match eff10:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.force.lr20.1(+gold: Bool, +macd_hist: Bool) -> {Bool.or(False{}, Bool.not(S.force(True{}, gold, False{}, macd_hist))) == True{} : Bool}:
  match gold:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.force.lr20.0(+eff10: Bool, +gold: Bool, +macd_hist: Bool) -> {Bool.or(False{}, Bool.not(S.force(eff10, gold, False{}, macd_hist))) == True{} : Bool}:
  match eff10:
    case False{}:
      {==}
    case True{}:
      pf.force.lr20.1(gold, macd_hist)

def pf.force.macd_hist.2(+lr20: Bool) -> {Bool.or(False{}, Bool.not(S.force(True{}, True{}, lr20, False{}))) == True{} : Bool}:
  match lr20:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.force.macd_hist.1(+gold: Bool, +lr20: Bool) -> {Bool.or(False{}, Bool.not(S.force(True{}, gold, lr20, False{}))) == True{} : Bool}:
  match gold:
    case False{}:
      {==}
    case True{}:
      pf.force.macd_hist.2(lr20)

def pf.force.macd_hist.0(+eff10: Bool, +gold: Bool, +lr20: Bool) -> {Bool.or(False{}, Bool.not(S.force(eff10, gold, lr20, False{}))) == True{} : Bool}:
  match eff10:
    case False{}:
      {==}
    case True{}:
      pf.force.macd_hist.1(gold, lr20)

def pf.energy.metabolic.0(+gap: Bool, +range_atr: Bool, +st_dist: Bool) -> {Bool.or(False{}, Bool.not(S.energy(gap, False{}, range_atr, st_dist))) == True{} : Bool}:
  match gap:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.energy.range_atr.1(+metabolic: Bool, +st_dist: Bool) -> {Bool.or(False{}, Bool.not(S.energy(True{}, metabolic, False{}, st_dist))) == True{} : Bool}:
  match metabolic:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.energy.range_atr.0(+gap: Bool, +metabolic: Bool, +st_dist: Bool) -> {Bool.or(False{}, Bool.not(S.energy(gap, metabolic, False{}, st_dist))) == True{} : Bool}:
  match gap:
    case False{}:
      {==}
    case True{}:
      pf.energy.range_atr.1(metabolic, st_dist)

def pf.energy.st_dist.2(+range_atr: Bool) -> {Bool.or(False{}, Bool.not(S.energy(True{}, True{}, range_atr, False{}))) == True{} : Bool}:
  match range_atr:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.energy.st_dist.1(+metabolic: Bool, +range_atr: Bool) -> {Bool.or(False{}, Bool.not(S.energy(True{}, metabolic, range_atr, False{}))) == True{} : Bool}:
  match metabolic:
    case False{}:
      {==}
    case True{}:
      pf.energy.st_dist.2(range_atr)

def pf.energy.st_dist.0(+gap: Bool, +metabolic: Bool, +range_atr: Bool) -> {Bool.or(False{}, Bool.not(S.energy(gap, metabolic, range_atr, False{}))) == True{} : Bool}:
  match gap:
    case False{}:
      {==}
    case True{}:
      pf.energy.st_dist.1(metabolic, range_atr)

def Laws.runner_when_all_hold():
  {==}

def Laws.runner_needs_hole_up(hole_up, fresh):
  match hole_up:
    case True{}:
      {==}
    case False{}:
      {==}

def Laws.runner_needs_fresh(hole_up, fresh):
  match fresh:
    case True{}:
      {==}
    case False{}:
      pf.runner.fresh.0(hole_up)

def Laws.structure_when_all_hold():
  {==}

def Laws.structure_needs_asym20(asym20, clv, d_pl, lo50):
  match asym20:
    case True{}:
      {==}
    case False{}:
      {==}

def Laws.structure_needs_clv(asym20, clv, d_pl, lo50):
  match clv:
    case True{}:
      {==}
    case False{}:
      pf.structure.clv.0(asym20, d_pl, lo50)

def Laws.structure_needs_d_pl(asym20, clv, d_pl, lo50):
  match d_pl:
    case True{}:
      {==}
    case False{}:
      pf.structure.d_pl.0(asym20, clv, lo50)

def Laws.structure_needs_lo50(asym20, clv, d_pl, lo50):
  match lo50:
    case True{}:
      {==}
    case False{}:
      pf.structure.lo50.0(asym20, clv, d_pl)

def Laws.force_when_all_hold():
  {==}

def Laws.force_needs_eff10(eff10, gold, lr20, macd_hist):
  match eff10:
    case True{}:
      {==}
    case False{}:
      {==}

def Laws.force_needs_gold(eff10, gold, lr20, macd_hist):
  match gold:
    case True{}:
      {==}
    case False{}:
      pf.force.gold.0(eff10, lr20, macd_hist)

def Laws.force_needs_lr20(eff10, gold, lr20, macd_hist):
  match lr20:
    case True{}:
      {==}
    case False{}:
      pf.force.lr20.0(eff10, gold, macd_hist)

def Laws.force_needs_macd_hist(eff10, gold, lr20, macd_hist):
  match macd_hist:
    case True{}:
      {==}
    case False{}:
      pf.force.macd_hist.0(eff10, gold, lr20)

def Laws.energy_when_all_hold():
  {==}

def Laws.energy_needs_gap(gap, metabolic, range_atr, st_dist):
  match gap:
    case True{}:
      {==}
    case False{}:
      {==}

def Laws.energy_needs_metabolic(gap, metabolic, range_atr, st_dist):
  match metabolic:
    case True{}:
      {==}
    case False{}:
      pf.energy.metabolic.0(gap, range_atr, st_dist)

def Laws.energy_needs_range_atr(gap, metabolic, range_atr, st_dist):
  match range_atr:
    case True{}:
      {==}
    case False{}:
      pf.energy.range_atr.0(gap, metabolic, st_dist)

def Laws.energy_needs_st_dist(gap, metabolic, range_atr, st_dist):
  match st_dist:
    case True{}:
      {==}
    case False{}:
      pf.energy.st_dist.0(gap, metabolic, range_atr)
