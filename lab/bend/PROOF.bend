# Proofs of LAWS.bend — finite case analyses (one match per def; deeper cases are their own defs); `bend PROOF.bend` is the gate.
import Base
import ./dragon.bend as D
import ./LAWS.bend as Laws

def pf.sig_needs_wh.0(+up: Bool, +te: Bool, +hot: Bool, +mt: Bool, +gd: Bool) -> {Bool.or(False{}, Bool.not(D.sig_c.b(up, False{}, te, hot, mt, gd))) == True{} : Bool}:
  match up:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.sig_needs_te.1(+wh: Bool, +hot: Bool, +mt: Bool, +gd: Bool) -> {Bool.or(False{}, Bool.not(D.sig_c.b(True{}, wh, False{}, hot, mt, gd))) == True{} : Bool}:
  match wh:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.sig_needs_te.0(+up: Bool, +wh: Bool, +hot: Bool, +mt: Bool, +gd: Bool) -> {Bool.or(False{}, Bool.not(D.sig_c.b(up, wh, False{}, hot, mt, gd))) == True{} : Bool}:
  match up:
    case False{}:
      {==}
    case True{}:
      pf.sig_needs_te.1(wh, hot, mt, gd)

def pf.sig_vetoed_when_hot.2(+te: Bool, +mt: Bool, +gd: Bool) -> {Bool.or(Bool.not(True{}), Bool.not(D.sig_c.b(True{}, True{}, te, True{}, mt, gd))) == True{} : Bool}:
  match te:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.sig_vetoed_when_hot.1(+wh: Bool, +te: Bool, +mt: Bool, +gd: Bool) -> {Bool.or(Bool.not(True{}), Bool.not(D.sig_c.b(True{}, wh, te, True{}, mt, gd))) == True{} : Bool}:
  match wh:
    case False{}:
      {==}
    case True{}:
      pf.sig_vetoed_when_hot.2(te, mt, gd)

def pf.sig_vetoed_when_hot.0(+up: Bool, +wh: Bool, +te: Bool, +mt: Bool, +gd: Bool) -> {Bool.or(Bool.not(True{}), Bool.not(D.sig_c.b(up, wh, te, True{}, mt, gd))) == True{} : Bool}:
  match up:
    case False{}:
      {==}
    case True{}:
      pf.sig_vetoed_when_hot.1(wh, te, mt, gd)

def pf.sig_needs_mt.3(+hot: Bool, +gd: Bool) -> {Bool.or(False{}, Bool.not(D.sig_c.b(True{}, True{}, True{}, hot, False{}, gd))) == True{} : Bool}:
  match hot:
    case True{}:
      {==}
    case False{}:
      {==}

def pf.sig_needs_mt.2(+te: Bool, +hot: Bool, +gd: Bool) -> {Bool.or(False{}, Bool.not(D.sig_c.b(True{}, True{}, te, hot, False{}, gd))) == True{} : Bool}:
  match te:
    case False{}:
      {==}
    case True{}:
      pf.sig_needs_mt.3(hot, gd)

def pf.sig_needs_mt.1(+wh: Bool, +te: Bool, +hot: Bool, +gd: Bool) -> {Bool.or(False{}, Bool.not(D.sig_c.b(True{}, wh, te, hot, False{}, gd))) == True{} : Bool}:
  match wh:
    case False{}:
      {==}
    case True{}:
      pf.sig_needs_mt.2(te, hot, gd)

def pf.sig_needs_mt.0(+up: Bool, +wh: Bool, +te: Bool, +hot: Bool, +gd: Bool) -> {Bool.or(False{}, Bool.not(D.sig_c.b(up, wh, te, hot, False{}, gd))) == True{} : Bool}:
  match up:
    case False{}:
      {==}
    case True{}:
      pf.sig_needs_mt.1(wh, te, hot, gd)

def pf.sig_needs_gd.4(+mt: Bool) -> {Bool.or(False{}, Bool.not(D.sig_c.b(True{}, True{}, True{}, False{}, mt, False{}))) == True{} : Bool}:
  match mt:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.sig_needs_gd.3(+hot: Bool, +mt: Bool) -> {Bool.or(False{}, Bool.not(D.sig_c.b(True{}, True{}, True{}, hot, mt, False{}))) == True{} : Bool}:
  match hot:
    case True{}:
      {==}
    case False{}:
      pf.sig_needs_gd.4(mt)

def pf.sig_needs_gd.2(+te: Bool, +hot: Bool, +mt: Bool) -> {Bool.or(False{}, Bool.not(D.sig_c.b(True{}, True{}, te, hot, mt, False{}))) == True{} : Bool}:
  match te:
    case False{}:
      {==}
    case True{}:
      pf.sig_needs_gd.3(hot, mt)

def pf.sig_needs_gd.1(+wh: Bool, +te: Bool, +hot: Bool, +mt: Bool) -> {Bool.or(False{}, Bool.not(D.sig_c.b(True{}, wh, te, hot, mt, False{}))) == True{} : Bool}:
  match wh:
    case False{}:
      {==}
    case True{}:
      pf.sig_needs_gd.2(te, hot, mt)

def pf.sig_needs_gd.0(+up: Bool, +wh: Bool, +te: Bool, +hot: Bool, +mt: Bool) -> {Bool.or(False{}, Bool.not(D.sig_c.b(up, wh, te, hot, mt, False{}))) == True{} : Bool}:
  match up:
    case False{}:
      {==}
    case True{}:
      pf.sig_needs_gd.1(wh, te, hot, mt)

def pf.panels_at_most_seven.FFFFFF(+g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, False{}, False{}, False{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FFFFFT(+g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, False{}, False{}, False{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FFFFF(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, False{}, False{}, False{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.FFFFFF(g)
    case True{}:
      pf.panels_at_most_seven.FFFFFT(g)

def pf.panels_at_most_seven.FFFFTF(+g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, False{}, False{}, True{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FFFFTT(+g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, False{}, False{}, True{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FFFFT(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, False{}, False{}, True{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.FFFFTF(g)
    case True{}:
      pf.panels_at_most_seven.FFFFTT(g)

def pf.panels_at_most_seven.FFFF(+e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, False{}, False{}, e, f, g), 7) == True{} : Bool}:
  match e:
    case False{}:
      pf.panels_at_most_seven.FFFFF(f, g)
    case True{}:
      pf.panels_at_most_seven.FFFFT(f, g)

def pf.panels_at_most_seven.FFFTFF(+g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, False{}, True{}, False{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FFFTFT(+g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, False{}, True{}, False{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FFFTF(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, False{}, True{}, False{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.FFFTFF(g)
    case True{}:
      pf.panels_at_most_seven.FFFTFT(g)

def pf.panels_at_most_seven.FFFTTF(+g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, False{}, True{}, True{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FFFTTT(+g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, False{}, True{}, True{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FFFTT(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, False{}, True{}, True{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.FFFTTF(g)
    case True{}:
      pf.panels_at_most_seven.FFFTTT(g)

def pf.panels_at_most_seven.FFFT(+e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, False{}, True{}, e, f, g), 7) == True{} : Bool}:
  match e:
    case False{}:
      pf.panels_at_most_seven.FFFTF(f, g)
    case True{}:
      pf.panels_at_most_seven.FFFTT(f, g)

def pf.panels_at_most_seven.FFF(+d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, False{}, d, e, f, g), 7) == True{} : Bool}:
  match d:
    case False{}:
      pf.panels_at_most_seven.FFFF(e, f, g)
    case True{}:
      pf.panels_at_most_seven.FFFT(e, f, g)

def pf.panels_at_most_seven.FFTFFF(+g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, True{}, False{}, False{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FFTFFT(+g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, True{}, False{}, False{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FFTFF(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, True{}, False{}, False{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.FFTFFF(g)
    case True{}:
      pf.panels_at_most_seven.FFTFFT(g)

def pf.panels_at_most_seven.FFTFTF(+g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, True{}, False{}, True{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FFTFTT(+g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, True{}, False{}, True{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FFTFT(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, True{}, False{}, True{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.FFTFTF(g)
    case True{}:
      pf.panels_at_most_seven.FFTFTT(g)

def pf.panels_at_most_seven.FFTF(+e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, True{}, False{}, e, f, g), 7) == True{} : Bool}:
  match e:
    case False{}:
      pf.panels_at_most_seven.FFTFF(f, g)
    case True{}:
      pf.panels_at_most_seven.FFTFT(f, g)

def pf.panels_at_most_seven.FFTTFF(+g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, True{}, True{}, False{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FFTTFT(+g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, True{}, True{}, False{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FFTTF(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, True{}, True{}, False{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.FFTTFF(g)
    case True{}:
      pf.panels_at_most_seven.FFTTFT(g)

def pf.panels_at_most_seven.FFTTTF(+g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, True{}, True{}, True{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FFTTTT(+g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, True{}, True{}, True{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FFTTT(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, True{}, True{}, True{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.FFTTTF(g)
    case True{}:
      pf.panels_at_most_seven.FFTTTT(g)

def pf.panels_at_most_seven.FFTT(+e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, True{}, True{}, e, f, g), 7) == True{} : Bool}:
  match e:
    case False{}:
      pf.panels_at_most_seven.FFTTF(f, g)
    case True{}:
      pf.panels_at_most_seven.FFTTT(f, g)

def pf.panels_at_most_seven.FFT(+d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, True{}, d, e, f, g), 7) == True{} : Bool}:
  match d:
    case False{}:
      pf.panels_at_most_seven.FFTF(e, f, g)
    case True{}:
      pf.panels_at_most_seven.FFTT(e, f, g)

def pf.panels_at_most_seven.FF(+c: Bool, +d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, False{}, c, d, e, f, g), 7) == True{} : Bool}:
  match c:
    case False{}:
      pf.panels_at_most_seven.FFF(d, e, f, g)
    case True{}:
      pf.panels_at_most_seven.FFT(d, e, f, g)

def pf.panels_at_most_seven.FTFFFF(+g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, False{}, False{}, False{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FTFFFT(+g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, False{}, False{}, False{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FTFFF(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, False{}, False{}, False{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.FTFFFF(g)
    case True{}:
      pf.panels_at_most_seven.FTFFFT(g)

def pf.panels_at_most_seven.FTFFTF(+g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, False{}, False{}, True{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FTFFTT(+g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, False{}, False{}, True{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FTFFT(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, False{}, False{}, True{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.FTFFTF(g)
    case True{}:
      pf.panels_at_most_seven.FTFFTT(g)

def pf.panels_at_most_seven.FTFF(+e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, False{}, False{}, e, f, g), 7) == True{} : Bool}:
  match e:
    case False{}:
      pf.panels_at_most_seven.FTFFF(f, g)
    case True{}:
      pf.panels_at_most_seven.FTFFT(f, g)

def pf.panels_at_most_seven.FTFTFF(+g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, False{}, True{}, False{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FTFTFT(+g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, False{}, True{}, False{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FTFTF(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, False{}, True{}, False{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.FTFTFF(g)
    case True{}:
      pf.panels_at_most_seven.FTFTFT(g)

def pf.panels_at_most_seven.FTFTTF(+g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, False{}, True{}, True{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FTFTTT(+g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, False{}, True{}, True{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FTFTT(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, False{}, True{}, True{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.FTFTTF(g)
    case True{}:
      pf.panels_at_most_seven.FTFTTT(g)

def pf.panels_at_most_seven.FTFT(+e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, False{}, True{}, e, f, g), 7) == True{} : Bool}:
  match e:
    case False{}:
      pf.panels_at_most_seven.FTFTF(f, g)
    case True{}:
      pf.panels_at_most_seven.FTFTT(f, g)

def pf.panels_at_most_seven.FTF(+d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, False{}, d, e, f, g), 7) == True{} : Bool}:
  match d:
    case False{}:
      pf.panels_at_most_seven.FTFF(e, f, g)
    case True{}:
      pf.panels_at_most_seven.FTFT(e, f, g)

def pf.panels_at_most_seven.FTTFFF(+g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, True{}, False{}, False{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FTTFFT(+g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, True{}, False{}, False{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FTTFF(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, True{}, False{}, False{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.FTTFFF(g)
    case True{}:
      pf.panels_at_most_seven.FTTFFT(g)

def pf.panels_at_most_seven.FTTFTF(+g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, True{}, False{}, True{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FTTFTT(+g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, True{}, False{}, True{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FTTFT(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, True{}, False{}, True{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.FTTFTF(g)
    case True{}:
      pf.panels_at_most_seven.FTTFTT(g)

def pf.panels_at_most_seven.FTTF(+e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, True{}, False{}, e, f, g), 7) == True{} : Bool}:
  match e:
    case False{}:
      pf.panels_at_most_seven.FTTFF(f, g)
    case True{}:
      pf.panels_at_most_seven.FTTFT(f, g)

def pf.panels_at_most_seven.FTTTFF(+g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, True{}, True{}, False{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FTTTFT(+g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, True{}, True{}, False{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FTTTF(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, True{}, True{}, False{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.FTTTFF(g)
    case True{}:
      pf.panels_at_most_seven.FTTTFT(g)

def pf.panels_at_most_seven.FTTTTF(+g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, True{}, True{}, True{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FTTTTT(+g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, True{}, True{}, True{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.FTTTT(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, True{}, True{}, True{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.FTTTTF(g)
    case True{}:
      pf.panels_at_most_seven.FTTTTT(g)

def pf.panels_at_most_seven.FTTT(+e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, True{}, True{}, e, f, g), 7) == True{} : Bool}:
  match e:
    case False{}:
      pf.panels_at_most_seven.FTTTF(f, g)
    case True{}:
      pf.panels_at_most_seven.FTTTT(f, g)

def pf.panels_at_most_seven.FTT(+d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, True{}, d, e, f, g), 7) == True{} : Bool}:
  match d:
    case False{}:
      pf.panels_at_most_seven.FTTF(e, f, g)
    case True{}:
      pf.panels_at_most_seven.FTTT(e, f, g)

def pf.panels_at_most_seven.FT(+c: Bool, +d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, True{}, c, d, e, f, g), 7) == True{} : Bool}:
  match c:
    case False{}:
      pf.panels_at_most_seven.FTF(d, e, f, g)
    case True{}:
      pf.panels_at_most_seven.FTT(d, e, f, g)

def pf.panels_at_most_seven.F(+b: Bool, +c: Bool, +d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(False{}, b, c, d, e, f, g), 7) == True{} : Bool}:
  match b:
    case False{}:
      pf.panels_at_most_seven.FF(c, d, e, f, g)
    case True{}:
      pf.panels_at_most_seven.FT(c, d, e, f, g)

def pf.panels_at_most_seven.TFFFFF(+g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, False{}, False{}, False{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TFFFFT(+g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, False{}, False{}, False{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TFFFF(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, False{}, False{}, False{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.TFFFFF(g)
    case True{}:
      pf.panels_at_most_seven.TFFFFT(g)

def pf.panels_at_most_seven.TFFFTF(+g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, False{}, False{}, True{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TFFFTT(+g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, False{}, False{}, True{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TFFFT(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, False{}, False{}, True{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.TFFFTF(g)
    case True{}:
      pf.panels_at_most_seven.TFFFTT(g)

def pf.panels_at_most_seven.TFFF(+e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, False{}, False{}, e, f, g), 7) == True{} : Bool}:
  match e:
    case False{}:
      pf.panels_at_most_seven.TFFFF(f, g)
    case True{}:
      pf.panels_at_most_seven.TFFFT(f, g)

def pf.panels_at_most_seven.TFFTFF(+g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, False{}, True{}, False{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TFFTFT(+g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, False{}, True{}, False{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TFFTF(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, False{}, True{}, False{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.TFFTFF(g)
    case True{}:
      pf.panels_at_most_seven.TFFTFT(g)

def pf.panels_at_most_seven.TFFTTF(+g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, False{}, True{}, True{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TFFTTT(+g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, False{}, True{}, True{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TFFTT(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, False{}, True{}, True{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.TFFTTF(g)
    case True{}:
      pf.panels_at_most_seven.TFFTTT(g)

def pf.panels_at_most_seven.TFFT(+e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, False{}, True{}, e, f, g), 7) == True{} : Bool}:
  match e:
    case False{}:
      pf.panels_at_most_seven.TFFTF(f, g)
    case True{}:
      pf.panels_at_most_seven.TFFTT(f, g)

def pf.panels_at_most_seven.TFF(+d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, False{}, d, e, f, g), 7) == True{} : Bool}:
  match d:
    case False{}:
      pf.panels_at_most_seven.TFFF(e, f, g)
    case True{}:
      pf.panels_at_most_seven.TFFT(e, f, g)

def pf.panels_at_most_seven.TFTFFF(+g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, True{}, False{}, False{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TFTFFT(+g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, True{}, False{}, False{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TFTFF(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, True{}, False{}, False{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.TFTFFF(g)
    case True{}:
      pf.panels_at_most_seven.TFTFFT(g)

def pf.panels_at_most_seven.TFTFTF(+g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, True{}, False{}, True{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TFTFTT(+g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, True{}, False{}, True{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TFTFT(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, True{}, False{}, True{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.TFTFTF(g)
    case True{}:
      pf.panels_at_most_seven.TFTFTT(g)

def pf.panels_at_most_seven.TFTF(+e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, True{}, False{}, e, f, g), 7) == True{} : Bool}:
  match e:
    case False{}:
      pf.panels_at_most_seven.TFTFF(f, g)
    case True{}:
      pf.panels_at_most_seven.TFTFT(f, g)

def pf.panels_at_most_seven.TFTTFF(+g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, True{}, True{}, False{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TFTTFT(+g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, True{}, True{}, False{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TFTTF(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, True{}, True{}, False{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.TFTTFF(g)
    case True{}:
      pf.panels_at_most_seven.TFTTFT(g)

def pf.panels_at_most_seven.TFTTTF(+g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, True{}, True{}, True{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TFTTTT(+g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, True{}, True{}, True{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TFTTT(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, True{}, True{}, True{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.TFTTTF(g)
    case True{}:
      pf.panels_at_most_seven.TFTTTT(g)

def pf.panels_at_most_seven.TFTT(+e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, True{}, True{}, e, f, g), 7) == True{} : Bool}:
  match e:
    case False{}:
      pf.panels_at_most_seven.TFTTF(f, g)
    case True{}:
      pf.panels_at_most_seven.TFTTT(f, g)

def pf.panels_at_most_seven.TFT(+d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, True{}, d, e, f, g), 7) == True{} : Bool}:
  match d:
    case False{}:
      pf.panels_at_most_seven.TFTF(e, f, g)
    case True{}:
      pf.panels_at_most_seven.TFTT(e, f, g)

def pf.panels_at_most_seven.TF(+c: Bool, +d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, False{}, c, d, e, f, g), 7) == True{} : Bool}:
  match c:
    case False{}:
      pf.panels_at_most_seven.TFF(d, e, f, g)
    case True{}:
      pf.panels_at_most_seven.TFT(d, e, f, g)

def pf.panels_at_most_seven.TTFFFF(+g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, False{}, False{}, False{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TTFFFT(+g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, False{}, False{}, False{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TTFFF(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, False{}, False{}, False{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.TTFFFF(g)
    case True{}:
      pf.panels_at_most_seven.TTFFFT(g)

def pf.panels_at_most_seven.TTFFTF(+g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, False{}, False{}, True{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TTFFTT(+g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, False{}, False{}, True{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TTFFT(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, False{}, False{}, True{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.TTFFTF(g)
    case True{}:
      pf.panels_at_most_seven.TTFFTT(g)

def pf.panels_at_most_seven.TTFF(+e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, False{}, False{}, e, f, g), 7) == True{} : Bool}:
  match e:
    case False{}:
      pf.panels_at_most_seven.TTFFF(f, g)
    case True{}:
      pf.panels_at_most_seven.TTFFT(f, g)

def pf.panels_at_most_seven.TTFTFF(+g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, False{}, True{}, False{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TTFTFT(+g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, False{}, True{}, False{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TTFTF(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, False{}, True{}, False{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.TTFTFF(g)
    case True{}:
      pf.panels_at_most_seven.TTFTFT(g)

def pf.panels_at_most_seven.TTFTTF(+g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, False{}, True{}, True{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TTFTTT(+g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, False{}, True{}, True{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TTFTT(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, False{}, True{}, True{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.TTFTTF(g)
    case True{}:
      pf.panels_at_most_seven.TTFTTT(g)

def pf.panels_at_most_seven.TTFT(+e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, False{}, True{}, e, f, g), 7) == True{} : Bool}:
  match e:
    case False{}:
      pf.panels_at_most_seven.TTFTF(f, g)
    case True{}:
      pf.panels_at_most_seven.TTFTT(f, g)

def pf.panels_at_most_seven.TTF(+d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, False{}, d, e, f, g), 7) == True{} : Bool}:
  match d:
    case False{}:
      pf.panels_at_most_seven.TTFF(e, f, g)
    case True{}:
      pf.panels_at_most_seven.TTFT(e, f, g)

def pf.panels_at_most_seven.TTTFFF(+g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, True{}, False{}, False{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TTTFFT(+g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, True{}, False{}, False{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TTTFF(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, True{}, False{}, False{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.TTTFFF(g)
    case True{}:
      pf.panels_at_most_seven.TTTFFT(g)

def pf.panels_at_most_seven.TTTFTF(+g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, True{}, False{}, True{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TTTFTT(+g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, True{}, False{}, True{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TTTFT(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, True{}, False{}, True{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.TTTFTF(g)
    case True{}:
      pf.panels_at_most_seven.TTTFTT(g)

def pf.panels_at_most_seven.TTTF(+e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, True{}, False{}, e, f, g), 7) == True{} : Bool}:
  match e:
    case False{}:
      pf.panels_at_most_seven.TTTFF(f, g)
    case True{}:
      pf.panels_at_most_seven.TTTFT(f, g)

def pf.panels_at_most_seven.TTTTFF(+g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, True{}, True{}, False{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TTTTFT(+g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, True{}, True{}, False{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TTTTF(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, True{}, True{}, False{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.TTTTFF(g)
    case True{}:
      pf.panels_at_most_seven.TTTTFT(g)

def pf.panels_at_most_seven.TTTTTF(+g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, True{}, True{}, True{}, False{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TTTTTT(+g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, True{}, True{}, True{}, True{}, g), 7) == True{} : Bool}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_at_most_seven.TTTTT(+f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, True{}, True{}, True{}, f, g), 7) == True{} : Bool}:
  match f:
    case False{}:
      pf.panels_at_most_seven.TTTTTF(g)
    case True{}:
      pf.panels_at_most_seven.TTTTTT(g)

def pf.panels_at_most_seven.TTTT(+e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, True{}, True{}, e, f, g), 7) == True{} : Bool}:
  match e:
    case False{}:
      pf.panels_at_most_seven.TTTTF(f, g)
    case True{}:
      pf.panels_at_most_seven.TTTTT(f, g)

def pf.panels_at_most_seven.TTT(+d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, True{}, d, e, f, g), 7) == True{} : Bool}:
  match d:
    case False{}:
      pf.panels_at_most_seven.TTTF(e, f, g)
    case True{}:
      pf.panels_at_most_seven.TTTT(e, f, g)

def pf.panels_at_most_seven.TT(+c: Bool, +d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, True{}, c, d, e, f, g), 7) == True{} : Bool}:
  match c:
    case False{}:
      pf.panels_at_most_seven.TTF(d, e, f, g)
    case True{}:
      pf.panels_at_most_seven.TTT(d, e, f, g)

def pf.panels_at_most_seven.T(+b: Bool, +c: Bool, +d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {U32.is_le(D.panels.b(True{}, b, c, d, e, f, g), 7) == True{} : Bool}:
  match b:
    case False{}:
      pf.panels_at_most_seven.TF(c, d, e, f, g)
    case True{}:
      pf.panels_at_most_seven.TT(c, d, e, f, g)

def pf.panels_survive_packing.FFFFFF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, False{}, False{}, False{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, False{}, False{}, False{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FFFFFT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, False{}, False{}, False{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, False{}, False{}, False{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FFFFF(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, False{}, False{}, False{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, False{}, False{}, False{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.FFFFFF(g)
    case True{}:
      pf.panels_survive_packing.FFFFFT(g)

def pf.panels_survive_packing.FFFFTF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, False{}, False{}, True{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, False{}, False{}, True{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FFFFTT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, False{}, False{}, True{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, False{}, False{}, True{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FFFFT(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, False{}, False{}, True{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, False{}, False{}, True{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.FFFFTF(g)
    case True{}:
      pf.panels_survive_packing.FFFFTT(g)

def pf.panels_survive_packing.FFFF(+e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, False{}, False{}, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, False{}, False{}, e, f, g) : U32}:
  match e:
    case False{}:
      pf.panels_survive_packing.FFFFF(f, g)
    case True{}:
      pf.panels_survive_packing.FFFFT(f, g)

def pf.panels_survive_packing.FFFTFF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, False{}, True{}, False{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, False{}, True{}, False{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FFFTFT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, False{}, True{}, False{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, False{}, True{}, False{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FFFTF(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, False{}, True{}, False{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, False{}, True{}, False{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.FFFTFF(g)
    case True{}:
      pf.panels_survive_packing.FFFTFT(g)

def pf.panels_survive_packing.FFFTTF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, False{}, True{}, True{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, False{}, True{}, True{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FFFTTT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, False{}, True{}, True{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, False{}, True{}, True{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FFFTT(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, False{}, True{}, True{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, False{}, True{}, True{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.FFFTTF(g)
    case True{}:
      pf.panels_survive_packing.FFFTTT(g)

def pf.panels_survive_packing.FFFT(+e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, False{}, True{}, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, False{}, True{}, e, f, g) : U32}:
  match e:
    case False{}:
      pf.panels_survive_packing.FFFTF(f, g)
    case True{}:
      pf.panels_survive_packing.FFFTT(f, g)

def pf.panels_survive_packing.FFF(+d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, False{}, d, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, False{}, d, e, f, g) : U32}:
  match d:
    case False{}:
      pf.panels_survive_packing.FFFF(e, f, g)
    case True{}:
      pf.panels_survive_packing.FFFT(e, f, g)

def pf.panels_survive_packing.FFTFFF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, True{}, False{}, False{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, True{}, False{}, False{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FFTFFT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, True{}, False{}, False{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, True{}, False{}, False{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FFTFF(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, True{}, False{}, False{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, True{}, False{}, False{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.FFTFFF(g)
    case True{}:
      pf.panels_survive_packing.FFTFFT(g)

def pf.panels_survive_packing.FFTFTF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, True{}, False{}, True{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, True{}, False{}, True{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FFTFTT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, True{}, False{}, True{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, True{}, False{}, True{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FFTFT(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, True{}, False{}, True{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, True{}, False{}, True{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.FFTFTF(g)
    case True{}:
      pf.panels_survive_packing.FFTFTT(g)

def pf.panels_survive_packing.FFTF(+e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, True{}, False{}, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, True{}, False{}, e, f, g) : U32}:
  match e:
    case False{}:
      pf.panels_survive_packing.FFTFF(f, g)
    case True{}:
      pf.panels_survive_packing.FFTFT(f, g)

def pf.panels_survive_packing.FFTTFF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, True{}, True{}, False{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, True{}, True{}, False{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FFTTFT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, True{}, True{}, False{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, True{}, True{}, False{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FFTTF(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, True{}, True{}, False{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, True{}, True{}, False{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.FFTTFF(g)
    case True{}:
      pf.panels_survive_packing.FFTTFT(g)

def pf.panels_survive_packing.FFTTTF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, True{}, True{}, True{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, True{}, True{}, True{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FFTTTT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, True{}, True{}, True{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, True{}, True{}, True{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FFTTT(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, True{}, True{}, True{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, True{}, True{}, True{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.FFTTTF(g)
    case True{}:
      pf.panels_survive_packing.FFTTTT(g)

def pf.panels_survive_packing.FFTT(+e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, True{}, True{}, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, True{}, True{}, e, f, g) : U32}:
  match e:
    case False{}:
      pf.panels_survive_packing.FFTTF(f, g)
    case True{}:
      pf.panels_survive_packing.FFTTT(f, g)

def pf.panels_survive_packing.FFT(+d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, True{}, d, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, True{}, d, e, f, g) : U32}:
  match d:
    case False{}:
      pf.panels_survive_packing.FFTF(e, f, g)
    case True{}:
      pf.panels_survive_packing.FFTT(e, f, g)

def pf.panels_survive_packing.FF(+c: Bool, +d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, False{}, c, d, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, False{}, c, d, e, f, g) : U32}:
  match c:
    case False{}:
      pf.panels_survive_packing.FFF(d, e, f, g)
    case True{}:
      pf.panels_survive_packing.FFT(d, e, f, g)

def pf.panels_survive_packing.FTFFFF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, False{}, False{}, False{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, False{}, False{}, False{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FTFFFT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, False{}, False{}, False{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, False{}, False{}, False{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FTFFF(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, False{}, False{}, False{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, False{}, False{}, False{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.FTFFFF(g)
    case True{}:
      pf.panels_survive_packing.FTFFFT(g)

def pf.panels_survive_packing.FTFFTF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, False{}, False{}, True{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, False{}, False{}, True{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FTFFTT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, False{}, False{}, True{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, False{}, False{}, True{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FTFFT(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, False{}, False{}, True{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, False{}, False{}, True{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.FTFFTF(g)
    case True{}:
      pf.panels_survive_packing.FTFFTT(g)

def pf.panels_survive_packing.FTFF(+e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, False{}, False{}, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, False{}, False{}, e, f, g) : U32}:
  match e:
    case False{}:
      pf.panels_survive_packing.FTFFF(f, g)
    case True{}:
      pf.panels_survive_packing.FTFFT(f, g)

def pf.panels_survive_packing.FTFTFF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, False{}, True{}, False{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, False{}, True{}, False{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FTFTFT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, False{}, True{}, False{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, False{}, True{}, False{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FTFTF(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, False{}, True{}, False{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, False{}, True{}, False{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.FTFTFF(g)
    case True{}:
      pf.panels_survive_packing.FTFTFT(g)

def pf.panels_survive_packing.FTFTTF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, False{}, True{}, True{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, False{}, True{}, True{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FTFTTT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, False{}, True{}, True{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, False{}, True{}, True{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FTFTT(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, False{}, True{}, True{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, False{}, True{}, True{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.FTFTTF(g)
    case True{}:
      pf.panels_survive_packing.FTFTTT(g)

def pf.panels_survive_packing.FTFT(+e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, False{}, True{}, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, False{}, True{}, e, f, g) : U32}:
  match e:
    case False{}:
      pf.panels_survive_packing.FTFTF(f, g)
    case True{}:
      pf.panels_survive_packing.FTFTT(f, g)

def pf.panels_survive_packing.FTF(+d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, False{}, d, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, False{}, d, e, f, g) : U32}:
  match d:
    case False{}:
      pf.panels_survive_packing.FTFF(e, f, g)
    case True{}:
      pf.panels_survive_packing.FTFT(e, f, g)

def pf.panels_survive_packing.FTTFFF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, True{}, False{}, False{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, True{}, False{}, False{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FTTFFT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, True{}, False{}, False{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, True{}, False{}, False{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FTTFF(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, True{}, False{}, False{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, True{}, False{}, False{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.FTTFFF(g)
    case True{}:
      pf.panels_survive_packing.FTTFFT(g)

def pf.panels_survive_packing.FTTFTF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, True{}, False{}, True{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, True{}, False{}, True{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FTTFTT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, True{}, False{}, True{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, True{}, False{}, True{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FTTFT(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, True{}, False{}, True{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, True{}, False{}, True{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.FTTFTF(g)
    case True{}:
      pf.panels_survive_packing.FTTFTT(g)

def pf.panels_survive_packing.FTTF(+e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, True{}, False{}, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, True{}, False{}, e, f, g) : U32}:
  match e:
    case False{}:
      pf.panels_survive_packing.FTTFF(f, g)
    case True{}:
      pf.panels_survive_packing.FTTFT(f, g)

def pf.panels_survive_packing.FTTTFF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, True{}, True{}, False{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, True{}, True{}, False{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FTTTFT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, True{}, True{}, False{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, True{}, True{}, False{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FTTTF(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, True{}, True{}, False{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, True{}, True{}, False{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.FTTTFF(g)
    case True{}:
      pf.panels_survive_packing.FTTTFT(g)

def pf.panels_survive_packing.FTTTTF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, True{}, True{}, True{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, True{}, True{}, True{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FTTTTT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, True{}, True{}, True{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, True{}, True{}, True{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.FTTTT(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, True{}, True{}, True{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, True{}, True{}, True{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.FTTTTF(g)
    case True{}:
      pf.panels_survive_packing.FTTTTT(g)

def pf.panels_survive_packing.FTTT(+e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, True{}, True{}, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, True{}, True{}, e, f, g) : U32}:
  match e:
    case False{}:
      pf.panels_survive_packing.FTTTF(f, g)
    case True{}:
      pf.panels_survive_packing.FTTTT(f, g)

def pf.panels_survive_packing.FTT(+d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, True{}, d, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, True{}, d, e, f, g) : U32}:
  match d:
    case False{}:
      pf.panels_survive_packing.FTTF(e, f, g)
    case True{}:
      pf.panels_survive_packing.FTTT(e, f, g)

def pf.panels_survive_packing.FT(+c: Bool, +d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, True{}, c, d, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, True{}, c, d, e, f, g) : U32}:
  match c:
    case False{}:
      pf.panels_survive_packing.FTF(d, e, f, g)
    case True{}:
      pf.panels_survive_packing.FTT(d, e, f, g)

def pf.panels_survive_packing.F(+b: Bool, +c: Bool, +d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(False{}, b, c, d, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(False{}, b, c, d, e, f, g) : U32}:
  match b:
    case False{}:
      pf.panels_survive_packing.FF(c, d, e, f, g)
    case True{}:
      pf.panels_survive_packing.FT(c, d, e, f, g)

def pf.panels_survive_packing.TFFFFF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, False{}, False{}, False{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, False{}, False{}, False{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TFFFFT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, False{}, False{}, False{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, False{}, False{}, False{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TFFFF(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, False{}, False{}, False{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, False{}, False{}, False{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.TFFFFF(g)
    case True{}:
      pf.panels_survive_packing.TFFFFT(g)

def pf.panels_survive_packing.TFFFTF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, False{}, False{}, True{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, False{}, False{}, True{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TFFFTT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, False{}, False{}, True{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, False{}, False{}, True{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TFFFT(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, False{}, False{}, True{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, False{}, False{}, True{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.TFFFTF(g)
    case True{}:
      pf.panels_survive_packing.TFFFTT(g)

def pf.panels_survive_packing.TFFF(+e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, False{}, False{}, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, False{}, False{}, e, f, g) : U32}:
  match e:
    case False{}:
      pf.panels_survive_packing.TFFFF(f, g)
    case True{}:
      pf.panels_survive_packing.TFFFT(f, g)

def pf.panels_survive_packing.TFFTFF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, False{}, True{}, False{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, False{}, True{}, False{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TFFTFT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, False{}, True{}, False{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, False{}, True{}, False{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TFFTF(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, False{}, True{}, False{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, False{}, True{}, False{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.TFFTFF(g)
    case True{}:
      pf.panels_survive_packing.TFFTFT(g)

def pf.panels_survive_packing.TFFTTF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, False{}, True{}, True{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, False{}, True{}, True{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TFFTTT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, False{}, True{}, True{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, False{}, True{}, True{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TFFTT(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, False{}, True{}, True{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, False{}, True{}, True{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.TFFTTF(g)
    case True{}:
      pf.panels_survive_packing.TFFTTT(g)

def pf.panels_survive_packing.TFFT(+e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, False{}, True{}, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, False{}, True{}, e, f, g) : U32}:
  match e:
    case False{}:
      pf.panels_survive_packing.TFFTF(f, g)
    case True{}:
      pf.panels_survive_packing.TFFTT(f, g)

def pf.panels_survive_packing.TFF(+d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, False{}, d, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, False{}, d, e, f, g) : U32}:
  match d:
    case False{}:
      pf.panels_survive_packing.TFFF(e, f, g)
    case True{}:
      pf.panels_survive_packing.TFFT(e, f, g)

def pf.panels_survive_packing.TFTFFF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, True{}, False{}, False{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, True{}, False{}, False{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TFTFFT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, True{}, False{}, False{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, True{}, False{}, False{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TFTFF(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, True{}, False{}, False{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, True{}, False{}, False{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.TFTFFF(g)
    case True{}:
      pf.panels_survive_packing.TFTFFT(g)

def pf.panels_survive_packing.TFTFTF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, True{}, False{}, True{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, True{}, False{}, True{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TFTFTT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, True{}, False{}, True{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, True{}, False{}, True{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TFTFT(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, True{}, False{}, True{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, True{}, False{}, True{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.TFTFTF(g)
    case True{}:
      pf.panels_survive_packing.TFTFTT(g)

def pf.panels_survive_packing.TFTF(+e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, True{}, False{}, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, True{}, False{}, e, f, g) : U32}:
  match e:
    case False{}:
      pf.panels_survive_packing.TFTFF(f, g)
    case True{}:
      pf.panels_survive_packing.TFTFT(f, g)

def pf.panels_survive_packing.TFTTFF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, True{}, True{}, False{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, True{}, True{}, False{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TFTTFT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, True{}, True{}, False{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, True{}, True{}, False{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TFTTF(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, True{}, True{}, False{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, True{}, True{}, False{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.TFTTFF(g)
    case True{}:
      pf.panels_survive_packing.TFTTFT(g)

def pf.panels_survive_packing.TFTTTF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, True{}, True{}, True{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, True{}, True{}, True{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TFTTTT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, True{}, True{}, True{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, True{}, True{}, True{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TFTTT(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, True{}, True{}, True{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, True{}, True{}, True{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.TFTTTF(g)
    case True{}:
      pf.panels_survive_packing.TFTTTT(g)

def pf.panels_survive_packing.TFTT(+e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, True{}, True{}, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, True{}, True{}, e, f, g) : U32}:
  match e:
    case False{}:
      pf.panels_survive_packing.TFTTF(f, g)
    case True{}:
      pf.panels_survive_packing.TFTTT(f, g)

def pf.panels_survive_packing.TFT(+d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, True{}, d, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, True{}, d, e, f, g) : U32}:
  match d:
    case False{}:
      pf.panels_survive_packing.TFTF(e, f, g)
    case True{}:
      pf.panels_survive_packing.TFTT(e, f, g)

def pf.panels_survive_packing.TF(+c: Bool, +d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, False{}, c, d, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, False{}, c, d, e, f, g) : U32}:
  match c:
    case False{}:
      pf.panels_survive_packing.TFF(d, e, f, g)
    case True{}:
      pf.panels_survive_packing.TFT(d, e, f, g)

def pf.panels_survive_packing.TTFFFF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, False{}, False{}, False{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, False{}, False{}, False{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TTFFFT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, False{}, False{}, False{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, False{}, False{}, False{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TTFFF(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, False{}, False{}, False{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, False{}, False{}, False{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.TTFFFF(g)
    case True{}:
      pf.panels_survive_packing.TTFFFT(g)

def pf.panels_survive_packing.TTFFTF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, False{}, False{}, True{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, False{}, False{}, True{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TTFFTT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, False{}, False{}, True{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, False{}, False{}, True{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TTFFT(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, False{}, False{}, True{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, False{}, False{}, True{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.TTFFTF(g)
    case True{}:
      pf.panels_survive_packing.TTFFTT(g)

def pf.panels_survive_packing.TTFF(+e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, False{}, False{}, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, False{}, False{}, e, f, g) : U32}:
  match e:
    case False{}:
      pf.panels_survive_packing.TTFFF(f, g)
    case True{}:
      pf.panels_survive_packing.TTFFT(f, g)

def pf.panels_survive_packing.TTFTFF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, False{}, True{}, False{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, False{}, True{}, False{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TTFTFT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, False{}, True{}, False{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, False{}, True{}, False{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TTFTF(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, False{}, True{}, False{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, False{}, True{}, False{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.TTFTFF(g)
    case True{}:
      pf.panels_survive_packing.TTFTFT(g)

def pf.panels_survive_packing.TTFTTF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, False{}, True{}, True{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, False{}, True{}, True{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TTFTTT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, False{}, True{}, True{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, False{}, True{}, True{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TTFTT(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, False{}, True{}, True{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, False{}, True{}, True{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.TTFTTF(g)
    case True{}:
      pf.panels_survive_packing.TTFTTT(g)

def pf.panels_survive_packing.TTFT(+e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, False{}, True{}, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, False{}, True{}, e, f, g) : U32}:
  match e:
    case False{}:
      pf.panels_survive_packing.TTFTF(f, g)
    case True{}:
      pf.panels_survive_packing.TTFTT(f, g)

def pf.panels_survive_packing.TTF(+d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, False{}, d, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, False{}, d, e, f, g) : U32}:
  match d:
    case False{}:
      pf.panels_survive_packing.TTFF(e, f, g)
    case True{}:
      pf.panels_survive_packing.TTFT(e, f, g)

def pf.panels_survive_packing.TTTFFF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, True{}, False{}, False{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, True{}, False{}, False{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TTTFFT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, True{}, False{}, False{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, True{}, False{}, False{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TTTFF(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, True{}, False{}, False{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, True{}, False{}, False{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.TTTFFF(g)
    case True{}:
      pf.panels_survive_packing.TTTFFT(g)

def pf.panels_survive_packing.TTTFTF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, True{}, False{}, True{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, True{}, False{}, True{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TTTFTT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, True{}, False{}, True{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, True{}, False{}, True{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TTTFT(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, True{}, False{}, True{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, True{}, False{}, True{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.TTTFTF(g)
    case True{}:
      pf.panels_survive_packing.TTTFTT(g)

def pf.panels_survive_packing.TTTF(+e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, True{}, False{}, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, True{}, False{}, e, f, g) : U32}:
  match e:
    case False{}:
      pf.panels_survive_packing.TTTFF(f, g)
    case True{}:
      pf.panels_survive_packing.TTTFT(f, g)

def pf.panels_survive_packing.TTTTFF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, True{}, True{}, False{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, True{}, True{}, False{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TTTTFT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, True{}, True{}, False{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, True{}, True{}, False{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TTTTF(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, True{}, True{}, False{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, True{}, True{}, False{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.TTTTFF(g)
    case True{}:
      pf.panels_survive_packing.TTTTFT(g)

def pf.panels_survive_packing.TTTTTF(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, True{}, True{}, True{}, False{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, True{}, True{}, True{}, False{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TTTTTT(+g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, True{}, True{}, True{}, True{}, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, True{}, True{}, True{}, True{}, g) : U32}:
  match g:
    case False{}:
      {==}
    case True{}:
      {==}

def pf.panels_survive_packing.TTTTT(+f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, True{}, True{}, True{}, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, True{}, True{}, True{}, f, g) : U32}:
  match f:
    case False{}:
      pf.panels_survive_packing.TTTTTF(g)
    case True{}:
      pf.panels_survive_packing.TTTTTT(g)

def pf.panels_survive_packing.TTTT(+e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, True{}, True{}, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, True{}, True{}, e, f, g) : U32}:
  match e:
    case False{}:
      pf.panels_survive_packing.TTTTF(f, g)
    case True{}:
      pf.panels_survive_packing.TTTTT(f, g)

def pf.panels_survive_packing.TTT(+d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, True{}, d, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, True{}, d, e, f, g) : U32}:
  match d:
    case False{}:
      pf.panels_survive_packing.TTTF(e, f, g)
    case True{}:
      pf.panels_survive_packing.TTTT(e, f, g)

def pf.panels_survive_packing.TT(+c: Bool, +d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, True{}, c, d, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, True{}, c, d, e, f, g) : U32}:
  match c:
    case False{}:
      pf.panels_survive_packing.TTF(d, e, f, g)
    case True{}:
      pf.panels_survive_packing.TTT(d, e, f, g)

def pf.panels_survive_packing.T(+b: Bool, +c: Bool, +d: Bool, +e: Bool, +f: Bool, +g: Bool) -> {(U32.shrn(U32.shln(D.panels.b(True{}, b, c, d, e, f, g), 24n), 24n) .&. 15 : U32) == D.panels.b(True{}, b, c, d, e, f, g) : U32}:
  match b:
    case False{}:
      pf.panels_survive_packing.TF(c, d, e, f, g)
    case True{}:
      pf.panels_survive_packing.TT(c, d, e, f, g)

def Laws.hole_pivot_rearms(up, dn, s):
  {==}

def Laws.hole_up_sticks(up, dn):
  {==}

def Laws.hole_down_sticks(up, dn):
  {==}

def Laws.hole_armed_checks_break(up, dn):
  {==}

def Laws.hole_breaks_up_first(dn):
  {==}

def Laws.hole_breaks_down():
  {==}

def Laws.hole_holds():
  {==}

def Laws.hole_never_both(s):
  match s:
    case D.InForce{}:
      {==}
    case D.BrokeUp{}:
      {==}
    case D.BrokeDown{}:
      {==}

def Laws.sig_needs_up(up, wh, te, hot, mt, gd):
  match up:
    case True{}:
      {==}
    case False{}:
      {==}

def Laws.sig_needs_wh(up, wh, te, hot, mt, gd):
  match wh:
    case True{}:
      {==}
    case False{}:
      pf.sig_needs_wh.0(up, te, hot, mt, gd)

def Laws.sig_needs_te(up, wh, te, hot, mt, gd):
  match te:
    case True{}:
      {==}
    case False{}:
      pf.sig_needs_te.0(up, wh, hot, mt, gd)

def Laws.sig_vetoed_when_hot(up, wh, te, hot, mt, gd):
  match hot:
    case False{}:
      {==}
    case True{}:
      pf.sig_vetoed_when_hot.0(up, wh, te, mt, gd)

def Laws.sig_needs_mt(up, wh, te, hot, mt, gd):
  match mt:
    case True{}:
      {==}
    case False{}:
      pf.sig_needs_mt.0(up, wh, te, hot, gd)

def Laws.sig_needs_gd(up, wh, te, hot, mt, gd):
  match gd:
    case True{}:
      {==}
    case False{}:
      pf.sig_needs_gd.0(up, wh, te, hot, mt)

def Laws.sig_when_all_hold():
  {==}

def Laws.panels_at_most_seven(a, b, c, d, e, f, g):
  match a:
    case False{}:
      pf.panels_at_most_seven.F(b, c, d, e, f, g)
    case True{}:
      pf.panels_at_most_seven.T(b, c, d, e, f, g)

def Laws.panels_all_seven():
  {==}

def Laws.panels_none():
  {==}

def Laws.panels_survive_packing(a, b, c, d, e, f, g):
  match a:
    case False{}:
      pf.panels_survive_packing.F(b, c, d, e, f, g)
    case True{}:
      pf.panels_survive_packing.T(b, c, d, e, f, g)

def Laws.bracket_decided_stays(hs, ht, cur, s, t):
  {==}

def Laws.bracket_stop_before_target(ht, cur, s, t):
  {==}

def Laws.bracket_target(cur, s, t):
  {==}

def Laws.bracket_untouched_keeps_running_value(cur, s, t):
  {==}
