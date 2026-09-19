import Base

# RUNNER: hole_up=1 & fresh=1
def runner(hole_up: Bool, fresh: Bool) -> Bool:
  match hole_up:
    case False{}:
      False{}
    case True{}:
    match fresh:
      case False{}:
        False{}
      case True{}:
        True{}

# STRUCTURE: asym20≤p10(-0.874) & clv≤p10(0.08271) & d_pl≤p25(0.7581) & lo50≤p25(105.7)
def structure(asym20: Bool, clv: Bool, d_pl: Bool, lo50: Bool) -> Bool:
  match asym20:
    case False{}:
      False{}
    case True{}:
    match clv:
      case False{}:
        False{}
      case True{}:
      match d_pl:
        case False{}:
          False{}
        case True{}:
        match lo50:
          case False{}:
            False{}
          case True{}:
            True{}

# FORCE: eff10≤p25(0.134) & gold=0 & lr20≤p25(-0.09698) & macd_hist≤p25(-0.1326)
def force(eff10: Bool, gold: Bool, lr20: Bool, macd_hist: Bool) -> Bool:
  match eff10:
    case False{}:
      False{}
    case True{}:
    match gold:
      case False{}:
        False{}
      case True{}:
      match lr20:
        case False{}:
          False{}
        case True{}:
        match macd_hist:
          case False{}:
            False{}
          case True{}:
            True{}

# ENERGY: gap≥p75(0.1867) & metabolic≥p90(1.872) & range_atr≥p75(1.084) & st_dist≤p25(-2.226)
def energy(gap: Bool, metabolic: Bool, range_atr: Bool, st_dist: Bool) -> Bool:
  match gap:
    case False{}:
      False{}
    case True{}:
    match metabolic:
      case False{}:
        False{}
      case True{}:
      match range_atr:
        case False{}:
          False{}
        case True{}:
        match st_dist:
          case False{}:
            False{}
          case True{}:
            True{}
