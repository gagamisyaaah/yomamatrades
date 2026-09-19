"""Pine-equivalent building blocks on pandas Series (same conventions as TradingView's ta.*)."""
from __future__ import annotations

import numpy as np
import pandas as pd


def sma(x: pd.Series, n: int) -> pd.Series:
    return x.rolling(n).mean()


def ema(x: pd.Series, n: int) -> pd.Series:
    return x.ewm(span=n, adjust=False).mean()


def rma(x: pd.Series, n: int) -> pd.Series:
    return x.ewm(alpha=1.0 / n, adjust=False).mean()


def sma_cn(x: pd.Series, n: int) -> pd.Series:
    """Chinese SMA(X, N, 1): s = (x + (n-1)*s[1]) / n  — the KDJ smoother."""
    return x.ewm(alpha=1.0 / n, adjust=False).mean()


def tr(df: pd.DataFrame) -> pd.Series:
    pc = df["close"].shift(1)
    return pd.concat([df["high"] - df["low"], (df["high"] - pc).abs(), (df["low"] - pc).abs()], axis=1).max(axis=1)


def atr(df: pd.DataFrame, n: int = 14) -> pd.Series:
    return rma(tr(df), n)


def rsi(x: pd.Series, n: int) -> pd.Series:
    d = x.diff()
    up = rma(d.clip(lower=0), n)
    dn = rma((-d).clip(lower=0), n)
    return 100 - 100 / (1 + up / dn.replace(0, np.nan))


def mfi(df: pd.DataFrame, n: int = 14) -> pd.Series:
    tp = (df["high"] + df["low"] + df["close"]) / 3
    raw = tp * df["volume"]
    pos = raw.where(tp > tp.shift(1), 0.0)
    neg = raw.where(tp < tp.shift(1), 0.0)
    mr = pos.rolling(n).sum() / neg.rolling(n).sum().replace(0, np.nan)
    return 100 - 100 / (1 + mr)


def linreg(x: pd.Series, n: int) -> pd.Series:
    """ta.linreg(x, n, 0): value of the least-squares line at the current bar."""
    idx = np.arange(n)
    xm = idx.mean()
    den = ((idx - xm) ** 2).sum()

    def f(w):
        slope = ((idx - xm) * (w - w.mean())).sum() / den
        return w.mean() + slope * (n - 1 - xm)
    return x.rolling(n).apply(f, raw=True)


def pivothigh(h: pd.Series, left: int, right: int) -> pd.Series:
    """Confirmed pivot high: returns the pivot value on the bar where it is confirmed (right bars later), else NaN."""
    out = pd.Series(np.nan, index=h.index)
    v = h.values
    for i in range(left, len(v) - right):
        w = v[i - left:i + right + 1]
        if v[i] == w.max() and (w == v[i]).sum() == 1:
            out.iloc[i + right] = v[i]
    return out


def pivotlow(l: pd.Series, left: int, right: int) -> pd.Series:
    out = pd.Series(np.nan, index=l.index)
    v = l.values
    for i in range(left, len(v) - right):
        w = v[i - left:i + right + 1]
        if v[i] == w.min() and (w == v[i]).sum() == 1:
            out.iloc[i + right] = v[i]
    return out


def crossover(a: pd.Series, b: pd.Series) -> pd.Series:
    return (a > b) & (a.shift(1) <= b.shift(1))


def crossunder(a: pd.Series, b: pd.Series) -> pd.Series:
    return (a < b) & (a.shift(1) >= b.shift(1))


def percentrank(x: pd.Series, n: int) -> pd.Series:
    def f(w):
        return 100.0 * (w[:-1] < w[-1]).sum() / (len(w) - 1)
    return x.rolling(n + 1).apply(f, raw=True)
