"""pine_syntax.py — syntax-check Pine files with pynescript's ANTLR grammar, one warm process.

pynescript.ast.parse() runs the parser in full-LL prediction mode: on this grammar a 500-byte script
takes ~100 s cold because ANTLR builds its DFA prediction cache from scratch, while the same parse is
instant once the cache is warm (the cache lives on the parser class, i.e. per process). So this checks
files sequentially in ONE process — feed small files first — in SLL mode with a fall-back to full LL
only when SLL rejects, and enforces a per-file wall-clock limit with SIGALRM.

usage: python3.12 pine_syntax.py [--timeout S] [file.pine ...]
       with no files, reads one path per line from stdin (scout_wrap.py drives it that way)
prints one line per file:  <pass|fail|timeout>\\t<path>\\t<note>
"""
from __future__ import annotations

import argparse
import signal
import sys

from antlr4 import CommonTokenStream, InputStream
from antlr4.atn.PredictionMode import PredictionMode
from antlr4.error.ErrorStrategy import BailErrorStrategy
from pynescript.ast.grammar.antlr4.error_listener import PinescriptErrorListener
from pynescript.ast.grammar.antlr4.lexer import PinescriptLexer
from pynescript.ast.grammar.antlr4.parser import PinescriptParser


class Timeout(Exception):
    pass


def _alarm(_signum, _frame):
    raise Timeout()


def _parser(code: str, mode: PredictionMode, bail: bool) -> PinescriptParser:
    lexer = PinescriptLexer(InputStream(code))
    lexer.removeErrorListeners()
    lexer.addErrorListener(PinescriptErrorListener.INSTANCE)
    parser = PinescriptParser(CommonTokenStream(lexer))
    parser.removeErrorListeners()
    parser._interp.predictionMode = mode
    if bail:
        parser._errHandler = BailErrorStrategy()
    else:
        parser.addErrorListener(PinescriptErrorListener.INSTANCE)
    return parser


def _where(e: BaseException) -> str:
    tok = getattr(e, "offendingToken", None)          # RecognitionException
    if tok is None and e.args:                        # ParseCancellationException wraps one
        tok = getattr(e.args[0], "offendingToken", None)
    if tok is not None:
        return f"line {tok.line}:{tok.column} near {tok.text!r}"
    return f"{type(e).__name__}: {str(e)[:160]}".replace("\n", " ")


def check(code: str) -> tuple[bool, str]:
    try:
        _parser(code, PredictionMode.SLL, bail=True).start_script()
        return True, ""
    except Timeout:
        raise
    except Exception as sll_err:  # noqa: BLE001 — SLL rejected; full LL gives the real verdict
        sll_note = _where(sll_err)
    try:
        _parser(code, PredictionMode.LL, bail=False).start_script()
        return True, ""
    except Timeout:
        raise
    except Exception as e:  # noqa: BLE001 — pynescript raises SyntaxError / antlr exceptions
        return False, f"{type(e).__name__}: {str(e)[:160]}".replace("\n", " ") + f" (SLL: {sll_note})"


def check_file(path: str, timeout: int) -> tuple[str, str]:
    try:
        code = open(path, encoding="utf-8").read()
    except OSError as e:
        return "fail", f"cannot read: {e}"
    signal.alarm(timeout)
    try:
        ok, note = check(code)
        return ("pass" if ok else "fail"), note
    except Timeout:
        return "timeout", f"pynescript did not finish in {timeout}s"
    finally:
        signal.alarm(0)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("files", nargs="*")
    args = ap.parse_args()
    signal.signal(signal.SIGALRM, _alarm)
    paths = args.files or (line.rstrip("\n") for line in sys.stdin)
    for path in paths:
        if not path:
            continue
        status, note = check_file(path, args.timeout)
        print(f"{status}\t{path}\t{note}", flush=True)


if __name__ == "__main__":
    main()
