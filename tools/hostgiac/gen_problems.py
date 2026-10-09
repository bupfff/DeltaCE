#!/usr/bin/env python3
"""Random textbook-style calculus problems in giac syntax, for testing a step-by-step engine.
usage: python -I gen_problems.py --seed N --kind integrate|diff [--n COUNT] [--tags] > out.txt
Families follow standard textbooks (Stewart, APEX Calculus, OpenStax) with small random parameters, printed the way a
textbook prints them (not expanded or simplified), in shuffled rounds (equal shares). Deterministic per seed, no
duplicates. --tags prefixes "family<TAB>": def-* = definite version, nat-* = family 9 (natural bounds or improper)."""
import argparse
import math
import random
import sys
from fractions import Fraction as F
from math import isqrt

P_DEF, P_IMPROPER = 0.15, 0.4   # definite share of the technique families; improper share of family 9

# ---------------- formatting: giac syntax, textbook layout ----------------
def par(s, ops, lead=False):
    """Parenthesize s if it has an operator from ops outside all parentheses, or a leading sign (unless lead)."""
    d, top = 0, False
    for i, ch in enumerate(s):
        d += (ch == "(") - (ch == ")")
        top = top or d == 0 and i > 0 and ch in ops
    return f"({s})" if top or (s[0] == "-" and not lead) else s

# term: c*body (sign carried by c, empty body = constant); poly: coefficients, highest degree first; lf(a): x-a
def term(c, body=""): return str(c) if not body else body if c == 1 else "-" + body if c == -1 else f"{c}*{body}"
def xp(k): return "x" if k == 1 else f"x^{k}" if F(k).denominator == 1 else f"x^({k})"
def join(ts): return "".join(t if i == 0 or t[0] == "-" else "+" + t for i, t in enumerate(ts))
def poly(cs): return join([term(c, xp(len(cs) - 1 - i) if i < len(cs) - 1 else "") for i, c in enumerate(cs) if c])
def frac(n, d): return n if d in ("", "1") else f"{par(n, '+-', True)}/{par(d, '+-*/')}"
def pw(b, n): return b if n == 1 else f"{par(b, '+-*/^')}^" + (str(n) if F(n).denominator == 1 else f"({n})")
def pif(q): return ("pi" if q.numerator == 1 else f"{q.numerator}*pi") + (f"/{q.denominator}" if q.denominator > 1 else "")
def lf(a): return "x" if a == 0 else f"x-{a}" if a > 0 else f"x+{-a}"
def pval(p, v): return sum(c * v ** (len(p) - 1 - i) for i, c in enumerate(p))
def pmul(p, q): return [sum(p[i] * q[k - i] for i in range(len(p)) if 0 <= k - i < len(q)) for k in range(len(p) + len(q) - 1)]
def coef(r, p1=0.5, hi=9): return 1 if r.random() < p1 else r.randint(2, hi)   # a positive coefficient
def sgn(r, p=0.3): return -1 if r.random() < p else 1
def times(*fs): return "*".join(par(f, "+-", i == 0) for i, f in enumerate(f for f in fs if f and f != "1")) or "1"

def lin(r, pb=0.6):
    """Linear inside a*x+b with gcd(a, b) = 1 -> (text, a, b); sometimes printed b-a*x."""
    a = r.choice([1, 2, 2, 3, 3, 4, 5])
    b = sgn(r, .4) * r.choice([v for v in range(1, 10) if math.gcd(a, v) == 1]) if r.random() < pb else 0
    flip = b > 0 and r.random() < .25
    return (f"{b}-{term(a, 'x')}" if flip else join([term(a, "x")] + [str(b)] * (b != 0))), -a if flip else a, b

def rpoly(r, deg):
    """Coefficients of a random polynomial of degree deg >= 1: positive leading term, >= 2 terms, content 1."""
    cs = [0]
    while sum(1 for c in cs if c) < 2 or math.gcd(*cs) > 1:
        cs = [r.randint(1, 5)] + [r.randint(-9, 9) if r.random() < .45 else 0 for _ in range(deg)]
    return cs

NS = dict(vars(math), ln=math.log, sec=lambda t: 1 / math.cos(t), csc=lambda t: 1 / math.sin(t), cot=lambda t: 1 / math.tan(t))

def proper_on(f, lo, hi, n=200):
    """Numeric check that f is real, without pole or blow-up, on [lo, hi] (keeps definite integrals proper)."""
    def ev(s, x): return eval(s.replace("^", "**"), {"__builtins__": {}}, dict(NS, x=x))
    try:
        a, b = ev(lo, 0), ev(hi, 0)
        ys = [ev(f, a + (b - a) * i / n) for i in range(n + 1)]
    except (ValueError, ZeroDivisionError, OverflowError, TypeError):
        return False
    m = sorted(map(abs, ys))[n // 2] + 1     # no complex values, no spike, no sign change through a pole
    return a < b and all(isinstance(y, float) for y in ys) and max(map(abs, ys)) < 1e3 * m and \
        not any(y * z < 0 and min(abs(y), abs(z)) > 2 * m for y, z in zip(ys, ys[1:]))

# ---------------- integral families: each returns (integrand, natural bounds) ----------------
FRACS = [F(1, 3), F(2, 3), F(4, 3), F(5, 3), F(3, 2), F(5, 2), F(1, 4), F(3, 4), F(2, 5), F(-1, 3), F(-2, 3), F(-3, 2), F(-1, 4)]
def fam_powers(r):
    """1. sums of a*x^n, b/x^k, c*sqrt(x), d*x^(p/q); some 'simplify first' forms."""
    if r.random() < .2:
        t = r.randrange(3)
        if t < 2:
            return frac(poly(rpoly(r, r.randint(2 - t, 3 - t))), "sqrt(x)" if t else xp(r.randint(1, 2))), [("1", "4"), ("1", "2")]
        o = r.choice(["sqrt(x)", lin(r, 1)[0], poly(rpoly(r, 2))])
        return times(*(["sqrt(x)", lin(r, 1)[0]] if o == "sqrt(x)" else [lin(r, 1)[0], o])), [("0", "1"), ("0", "2"), ("1", "4")]
    n, es = r.choice([2, 2, 3, 3, 3, 4]), []
    while len(es) < n:
        k = r.choices("mrsf", [5, 1.5, 1.5, 2])[0]
        e = F({"m": r.randint(0, 6), "r": -r.randint(1, 4), "s": r.choice([1, -1]) * F(1, 2), "f": r.choice(FRACS)}[k])
        es += [e] * (e not in es)
    es.sort(key=lambda e: (e == 0, e.denominator > 1 or e < 0, -e))    # polynomial part first, constant last
    cs = [sgn(r, .35 if i else 0) * coef(r, .35) for i in range(n)]
    ts = [(term if e >= 0 else lambda c, s: frac(str(c), s))(c, "sqrt(x)" if abs(e) == F(1, 2) else xp(abs(e)) if e else "")
          for c, e in zip(cs, es)]
    return join(ts), ([("1", "2"), ("1", "4"), ("1", "8")] if min(es) < 0 else [("0", "1"), ("1", "4"), ("1", "8")]
                      if any(e.denominator > 1 for e in es) else [("0", "1"), ("0", "2"), ("-1", "1"), ("1", "2"), ("1", "3")])

def fam_table(r):
    """2. table integrals (exponential, trig, inverse trig) with a linear inside a*x+b."""
    k = r.choice("exp exp ax sin cos sin cos sec2 csc2 sectan csccot tan cot sec csc inv pow sqrt isqrt atan atan asin asin".split())
    u, a, b = lin(r, 1 if k in ("inv", "pow", "sqrt", "isqrt") else .45)
    c, A, n = str(coef(r, .6)), r.choice([1, 4, 9, 16, 25, 2, 3, 5]), r.choice([2, 3, 4, 5, 6, 7, 8, -2, -3])
    B = r.choice([v for v in (1, 1, 4, 9) if math.gcd(A, v) == 1])    # A+B*x^2 without a common factor
    s, sq, bx, q = F(isqrt(A), isqrt(B)), isqrt(A) ** 2 == A, term(B, "x^2"), F(1, abs(a))
    if k in ("sin", "cos", "tan", "cot", "sec", "csc", "sec2", "csc2", "sectan", "csccot"):
        f = {"sec2": pw(f"sec({u})", 2), "csc2": pw(f"csc({u})", 2), "sectan": times(f"sec({u})", f"tan({u})"),
             "csccot": times(f"csc({u})", f"cot({u})")}.get(k, f"{k}({u})")
        return times(c, f), [] if b else [("0", pif(q / 2)), ("0", pif(q / 4)), (pif(q / 4), pif(q / 2)), ("0", pif(q))]
    if k in ("atan", "asin"):
        return (frac(c, r.choice([f"{A}+{bx}", f"{bx}+{A}"])), [("0", str(s)), (str(-s), str(s))] if sq else [("0", "1")]) \
            if k == "atan" else (frac(c, f"sqrt({A}-{bx})"), [("0", str(s / 2))] if sq else [])
    f = {"exp": times(c, f"exp({u})"), "ax": f"{r.choice([2, 3, 5, 10])}^{par(u, '+-*/^')}", "inv": frac(c, u),
         "pow": times(c, pw(u, n)) if n > 0 else frac(c, pw(u, -n)), "sqrt": times(c, f"sqrt({u})"), "isqrt": frac(c, f"sqrt({u})")}[k]
    return f, {"exp": [("0", "1"), ("0", "ln(2)")], "ax": [("0", "1"), ("0", "2")]}.get(k, [("0", "1"), ("0", "2"), ("1", "2"), ("-1", "1")])

USUB_F = {"poly": "pow pow npow sqrt isqrt inv exp sin cos sec2 atan", "sin": "pow pow npow sqrt inv exp atan",
          "cos": "pow pow npow sqrt inv exp atan", "tan": "pow pow npow sqrt inv exp", "exp": "pow npow sqrt inv sin cos sec2 atan",
          "ln": "pow pow npow sqrt isqrt inv sin cos sec2 atan", "sqrt": "pow npow inv exp sin cos sec2 atan"}
POWLIKE = ("pow", "npow", "sqrt", "isqrt", "inv")
def fam_usub(r):
    """3. u-substitution: c*g'(x)*f(g(x)), with g' printed up to a constant factor."""
    gk = r.choice(["poly"] * 4 + ["sin", "cos", "tan", "exp", "ln", "sqrt"])
    fk, a, gd, g2 = r.choice(USUB_F[gk].split()), r.choice([1, 1, 1, 2, 3]), "", ""
    ax, bs = term(a, "x"), [("0", "1"), ("0", "2"), ("1", "2")]
    if gk == "poly" and fk == "atan":   # x^(k-1)/(A+x^(2k))
        k, A = r.choice([2, 3]), r.choice([1, 1, 4, 9, 16])
        return frac(xp(k - 1), r.choice([f"{A}+{xp(2 * k)}", f"{xp(2 * k)}+{A}"])), [("0", "1"), ("1", "2")]
    if gk == "poly":
        t, p, q = r.choice(["mono"] * 3 + ["quad", "cubic"]) if fk in POWLIKE + ("exp",) else "mono", r.randint(1, 6), r.randint(1, 9)
        if t == "quad":
            q += p * p == 4 * q     # not a perfect square
            g, gn = poly([1, p, q]), poly([1, p // 2] if p % 2 == 0 else [2, p])
        elif t == "cubic":
            g, gn = poly([1, 0, p, q]), poly([1, 0, p // 3] if p % 3 == 0 else [3, 0, p])
        else:
            k, ca = 1 if fk in POWLIKE and r.random() < .15 else r.choice([2, 3]), r.choice([1, 1, 2, 3])
            b = q if k == 1 or fk not in ("exp", "sin", "cos", "sec2") or r.random() < .3 else 0
            g = f"{b}-{term(ca, xp(k))}" if b and r.random() < .3 else \
                join([term(sgn(r, .4 if fk == "exp" and not b else 0) * ca, xp(k))] + [str(b)] * (b > 0))
            gn = xp(k - 1) if k > 1 else ""
    else:
        g0, gn, bs = {"sin": (f"sin({ax})", f"cos({ax})", [("0", pif(F(1, 2 * a))), (pif(F(1, 6 * a)), pif(F(1, 2 * a)))]),
                      "cos": (f"cos({ax})", f"sin({ax})", [("0", pif(F(1, 3 * a))), ("0", pif(F(1, 4 * a)))]),
                      "tan": (f"tan({ax})", pw(f"sec({ax})", 2), [("0", pif(F(1, 4 * a)))]),
                      "exp": (f"exp({ax})", f"exp({ax})", [("0", "1"), ("0", "ln(2)")]),
                      "ln": ("ln(x)", "", [("1", "exp(1)"), ("exp(1)", "exp(2)")]), "sqrt": ("sqrt(x)", "", [("1", "4"), ("1", "9")])}[gk]
        gd, g2 = {"ln": "x", "sqrt": "sqrt(x)"}.get(gk, ""), {"exp": f"exp({term(2 * a, 'x')})", "sqrt": "x"}.get(gk, pw(g0, 2))
        shift = fk in POWLIKE and (gk in ("exp", "sqrt") or fk == "inv" and gk in ("sin", "cos") or r.random() < .4)
        g = f"{r.choice([1, 1, 2, 3, 4])}+{g0}" if shift else g0    # e.g. cos(x)/(2+sin(x)), exp(x)*(1+exp(x))^3
    n = r.randint(2, 6) if fk == "pow" else r.randint(2, 3)
    fn, fd = {"pow": (pw(g, n), ""), "npow": ("", pw(g, n)), "sqrt": (f"sqrt({g})", ""), "isqrt": ("", f"sqrt({g})"),
              "inv": ("", g), "exp": (f"exp({g})", ""), "sin": (f"sin({g})", ""), "cos": (f"cos({g})", ""),
              "sec2": (pw(f"sec({g})", 2), ""), "atan": ("", "1+" + g2)}[fk]
    nums = [gn, fn] if gk == "poly" or r.random() < .5 else [fn, gn]
    return frac(times(str(coef(r, .65, 6)), *nums), times(gd, fd)), bs

def fam_parts(r):
    """4. integration by parts."""
    k = r.choice("xexp xsin xcos xln xln ln2 atan xatan asin acos esin ecos".split())
    n, a, b, m = r.choice([1, 1, 2, 2, 3]), r.choice([1, 1, 2, 3, 4]), r.choice([1, 2, 3, 4]), r.choice([0, 1, 2, 3, 4, F(1, 2), -2, -3])
    if k == "xexp":
        return times(xp(n), f"exp({term(sgn(r, .4) * a, 'x')})"), [("0", "1"), ("0", "2")]
    if k in ("xsin", "xcos"):
        return times(xp(n), f"{k[1:]}({term(a, 'x')})"), [("0", pif(F(1, a))), ("0", pif(F(1, 2 * a)))]
    if k == "xln":
        f = "ln(x)" if m == 0 else times("sqrt(x)" if m == F(1, 2) else xp(m), "ln(x)") if m > 0 else frac("ln(x)", xp(-m))
        return f, [("1", "exp(1)"), ("1", "2")]
    if k in ("atan", "asin", "acos"):
        a = 1 if k == "acos" else r.choice([1, 1, 2, 3])
        return f"{k}({term(a, 'x')})", [("0", str(F(1, a))), ("0", str(F(1, 2 * a)))]
    if k in ("ln2", "xatan"):
        return (r.choice(["ln(x)^2", "x*ln(x)^2"]), [("1", "exp(1)")]) if k == "ln2" else \
            (times(xp(r.choice([1, 1, 2])), "atan(x)"), [("0", "1")])
    return times(f"exp({term(sgn(r, .3) * a, 'x')})", f"{k[1:]}({term(b, 'x')})"), [("0", "pi"), ("0", "pi/2")]

def fam_trig(r):
    """5. sin^m*cos^n, tan^m*sec^n (or cot/csc), sin(a*x)*cos(b*x)."""
    a, k = r.choice([1, 1, 1, 2, 3]), r.choice(["sc", "sc", "ts", "ts", "prod"])
    u, q, even = term(a, "x"), F(1, a), r.random() < .3
    if k == "sc":    # one odd power, or both even and small
        m, n = r.choice([(2, 0), (0, 2), (2, 2), (4, 0), (0, 4)]) if even else (0, 0)
        while not even and not ((m % 2 or n % 2) and m + n >= 2):
            m, n = r.randint(0, 5), r.randint(0, 5)
        return times(pw(f"sin({u})", m) if m else "", pw(f"cos({u})", n) if n else ""), [("0", pif(q / 2)), ("0", pif(q)), ("0", pif(q / 4))]
    if k == "ts":
        (t, s), m, n = ("tan", "sec") if r.random() < .75 else ("cot", "csc"), 0, 0
        while m + n > 8 or (m, n) in ((0, 0), (0, 1), (1, 0), (0, 2), (1, 1)) or \
                not (n % 2 == 0 and n or m % 2 and n or not n and m >= 2 or (m, n) in ((0, 3), (2, 1))):
            m, n = r.randint(0, 5), r.randint(0, 6)
        return times(pw(f"{t}({u})", m) if m else "", pw(f"{s}({u})", n) if n else ""), \
            [("0", pif(q / 4))] if t == "tan" else [(pif(q / 4), pif(q / 2))]
    (a, b), (f1, f2) = r.sample(range(1, 7), 2), r.choice([("sin", "cos"), ("sin", "sin"), ("cos", "cos"), ("cos", "sin")])
    return times(f"{f1}({term(a, 'x')})", f"{f2}({term(b, 'x')})"), [("0", "pi"), ("0", "pi/2"), ("-pi", "pi")]

def fam_trigsub(r):
    """6. trig substitution: x^p * R^e, R = a^2-x^2, x^2+a^2 or x^2-a^2 (x^2 maybe b^2*x^2), e = 1/2, -1/2, -3/2."""
    t, a, b = r.choice("mpd"), r.randint(1, 5), r.choice([1, 1, 1, 1, 2, 3])
    A = a * a if r.random() < .8 else r.choice([2, 3, 5, 7])
    bx = term((b := b if A % b else 1) ** 2, "x^2")    # no common factor (not 4*x^2-4)
    R = {"m": f"{A}-{bx}", "p": r.choice([f"{bx}+{A}", f"{A}+{bx}"]), "d": f"{bx}-{A}"}[t]
    e, p = r.choice([(1, 0), (1, 2), (1, 3), (1, -1), (1, -2), (-1, 0), (-1, 2), (-1, 3), (-1, -1), (-1, -2), (-3, 0), (-3, 2)]
                    + [(1, -3), (1, -4)] * (t == "d"))
    rad, s = f"sqrt({R})" if e != -3 else pw(R, F(3, 2)), F(isqrt(A), b)
    f = frac(times(xp(p) if p > 0 else "", rad if e == 1 else ""), times(xp(-p) if p < 0 else "", rad if e < 0 else ""))
    return f, [] if isqrt(A) ** 2 != A else {"m": [("0", str(s)), ("0", str(s / 2)), (str(s / 2), str(s))],
                                            "p": [("0", str(s)), (str(s), str(2 * s))], "d": [(str(s), str(2 * s)), (str(2 * s), str(4 * s))]}[t]

def fam_pfrac(r):
    """7. partial fractions: distinct linear, repeated linear, linear*(x^2+c), improper (deg P >= deg Q)."""
    t, (a, b), c = r.choice(["2", "2", "rep", "quad", "impr"]), r.sample(range(-5, 6), 2), r.randint(1, 9)
    facs, Q, roots = {"quad": ([lf(a), f"x^2+{c}"], pmul([1, -a], [1, 0, c]), [a]),
                      "rep": ([pw(lf(a), 2), lf(b)], pmul(pmul([1, -a], [1, -a]), [1, -b]), [a, b])}.get(
                          t, ([lf(a), lf(b)], pmul([1, -a], [1, -b]), [a, b]))
    dp, P = len(Q) - 1 + r.choice([0, 0, 1]) if t == "impr" else r.randint(0, len(Q) - 2), [0]
    while not all(pval(P, z) for z in roots) or t == "quad" and dp == 2 and not P[1] and P[2] == P[0] * c:   # no common factor
        P = [r.randint(1, 5)] + [r.randint(-9, 9) if r.random() < .6 else 0 for _ in range(dp)]
    lo = max(roots) + r.randint(1, 2) if r.random() < .6 else min(roots) - r.randint(2, 4)
    hi = lo + r.randint(1, 3) if lo > max(roots) else min(roots) - 1
    den = times(*sorted(facs, key=lambda f: f not in ("x", "x^2"))) if r.random() < .65 else poly(Q)   # x*(x-3) or expanded
    return frac(poly(P), den), [(str(lo), str(hi))]

def fam_csq(r):
    """8. completing the square: 1/(x^2+b*x+c), (p*x+d)/(x^2+b*x+c), 1/sqrt(c+b*x-x^2) and relatives."""
    t, h, k = r.choice(["inv", "inv", "lin", "isq", "isq", "sqrt", "xisq", "isqp"]), r.choice([-4, -3, -2, -1, 1, 2, 3, 4]), r.randint(1, 9)
    if t in ("inv", "lin", "isqp"):
        m, p = r.choice([1, 1, 1, 1, 2, 3]) if t == "inv" else 1, r.randint(1, 4)
        Q, s = poly([m * m, 2 * m * h, h * h + k]), isqrt(k) if m == 1 and isqrt(k) ** 2 == k else 0
        num = {"inv": str(coef(r, .7, 5)), "isqp": "1", "lin": poly([p, r.choice([v for v in range(-9, 10) if v != p * h])])}[t]
        return frac(num, f"sqrt({Q})" if t == "isqp" else Q), [(str(-h), str(-h + s)), (str(-h - s), str(-h + s))] if s else [("0", "1")]
    k2 = r.choice([1, 4, 9, 16, 25, 2, 3, 5, 8])
    h = abs(h) if k2 <= h * h else h
    c0, s = k2 - h * h, F(isqrt(k2))    # c0 + 2*h*x - x^2 = k2 - (x-h)^2
    E = join([str(c0), term(2 * h, "x"), "-x^2"] if c0 > 0 else [term(2 * h, "x"), "-x^2"] + [str(c0)] * (c0 < 0))
    f = {"isq": frac("1", f"sqrt({E})"), "sqrt": f"sqrt({E})", "xisq": frac("x", f"sqrt({E})")}[t]
    return f, [] if s * s != k2 else [(str(h - s), str(h + s))] if t == "sqrt" else [(str(h), str(h + s / 2))]

def improper(r):   # 9b. convergent improper integrals -> (integrand, lo, hi)
    a, b, n, A = r.randint(1, 4), r.randint(1, 4), r.randint(1, 3), r.choice([1, 1, 4, 9, 3])
    c, ea = str(coef(r, .6, 5)), f"exp({term(-a, 'x')})"
    return r.choice([
        lambda: (frac(c, xp(r.choice([2, 3, 4, F(3, 2), F(5, 2), F(4, 3)]))), "1", "inf"), lambda: (times(c, ea), "0", "inf"),
        lambda: (times(xp(n), ea), "0", "inf"), lambda: (times(ea, f"{r.choice(['sin', 'cos'])}({term(b, 'x')})"), "0", "inf"),
        lambda: (times("x", f"exp({term(-a, 'x^2')})"), "0", "inf"), lambda: (frac(c, f"x^2+{A}"), r.choice(["0", "-inf"]), "inf"),
        lambda: (frac(c, pw(lf(-a), r.choice([2, 3]))), "0", "inf"), lambda: (frac("1", times(lf(-a), lf(-a - b))), "0", "inf"),
        lambda: (frac("ln(x)", xp(r.choice([2, 3]))), "1", "inf"), lambda: (frac("x", f"(x^2+{A})^2"), "0", "inf"),
        lambda: (frac("1", r.choice(["sqrt(x)", xp(F(1, 3)), xp(F(2, 3)), xp(F(3, 4))])), "0", "1"),
        lambda: (r.choice(["ln(x)", "ln(x)/sqrt(x)", "x*ln(x)"]), "0", "1"), lambda: ("atan(x)/(1+x^2)", "0", "inf"),
        lambda: (frac("1", f"x*ln(x)^{r.choice([2, 3])}"), "exp(1)", "inf"), lambda: (frac("1", f"sqrt({a * a}-x^2)"), "0", str(a)),
        lambda: (times(xp(n - 1) if n > 1 else "", f"exp({term(a, 'x')})"), "-inf", "0")])()

INT_FAMS = [("powers", fam_powers), ("table", fam_table), ("usub", fam_usub), ("parts", fam_parts), ("trig", fam_trig),
            ("trigsub", fam_trigsub), ("pfrac", fam_pfrac), ("csquare", fam_csq), ("nat", None)]

def gen_int(r, name, fam):
    nat = name == "nat"    # family 9: definite with natural bounds, or improper
    if nat and r.random() < P_IMPROPER:
        return "nat-improper", "integrate({},x,{},{})".format(*improper(r))
    name, fam = r.choice(INT_FAMS[:-1]) if nat else (name, fam)
    f, bs = fam(r)
    while not (bs := [bd for bd in bs if proper_on(f, *bd)]) and nat:
        f, bs = fam(r)
    if nat or bs and r.random() < P_DEF:
        lo, hi = r.choice(bs)
        return ("nat-" if nat else "def-") + name, f"integrate({f},x,{lo},{hi})"
    return name, f"integrate({f},x)"

# ---------------- derivative families ----------------
D_ATOMS = ["poly", "pow", "exp", "trig", "ln", "inv", "ax", "root"]
OUTER = {"poly": "pow pow sqrt exp ln sin cos tan sec atan", "trig": "pow pow sqrt exp ln", "exp": "pow sqrt ln sin cos atan",
         "ln": "pow sqrt ln sin cos atan", "sqrt": "pow exp sin cos tan atan ln", "recip": "exp sin cos atan",
         "pow": "exp ln sin cos", "inv": "pow sqrt exp"}

def datom(r, k):
    a = r.choice([1, 1, 2, 3])
    return {"poly": lambda: poly(rpoly(r, r.randint(1, 4))), "pow": lambda: r.choice([xp(r.randint(2, 6)), xp(r.choice(FRACS[:9])), "sqrt(x)"]),
            "exp": lambda: f"exp({term(sgn(r) * a, 'x')})", "ln": lambda: r.choice(["ln(x)", f"ln({lin(r, 1)[0]})"]),
            "trig": lambda: f"{r.choice(['sin', 'cos', 'sin', 'cos', 'tan', 'sec', 'csc', 'cot'])}({term(a, 'x')})",
            "inv": lambda: f"{r.choice(['atan', 'atan', 'asin', 'acos'])}({term(a if r.random() < .3 else 1, 'x')})",
            "ax": lambda: f"{r.choice([2, 3, 5, 10])}^x", "root": lambda: f"sqrt({poly(rpoly(r, r.randint(1, 2)))})"}[k]()

def inner(r):
    k, a = r.choice(["poly", "poly", "trig", "exp", "ln", "sqrt", "recip"]), r.choice([1, 1, 2, 3])
    return k, {"poly": lambda: poly(rpoly(r, r.randint(2, 3))), "trig": lambda: f"{r.choice(['sin', 'cos', 'tan'])}({term(a, 'x')})",
               "exp": lambda: f"exp({term(r.choice([1, 2, -1, 3]), 'x')})", "ln": lambda: "ln(x)", "sqrt": lambda: "sqrt(x)",
               "recip": lambda: "1/x"}[k]()

def outer(r, k, u):   # apply a random outer function to u (an expression of kind k) -> (kind, text)
    o = r.choice(OUTER[k].split())
    if o in ("pow", "sqrt", "ln") and k in ("exp", "trig", "ln", "sqrt", "inv") and (k == "exp" or k == "sqrt" and o != "ln" or r.random() < .4):
        u = f"{r.randint(1, 4)}+{u}"    # e.g. ln(1+exp(x)), (1+sqrt(x))^3, sqrt(2+sin(x))
    if o != "pow":
        return {"atan": "inv", "sin": "trig", "cos": "trig", "tan": "trig", "sec": "trig"}.get(o, o), f"{o}({u})"
    n = r.choice([2, 3, 4, 5, 6, 7, 8, -1, -2, F(1, 3), F(2, 3), F(3, 2)])
    return "pow", frac("1", pw(u, -n)) if n < 0 else pw(u, n)

def chain2(r): return outer(r, *inner(r))[1]
def chain3(r): return outer(r, *outer(r, *inner(r)))[1]

def product(r):
    ks = sorted(r.sample(D_ATOMS, 2), key=lambda k: k not in ("poly", "pow"))
    return times(str(coef(r, .7)), *[datom(r, k) for k in ks], *([chain2(r)] if r.random() < .2 else []))

def quotient(r):
    nk = r.choice(["poly", "poly", "pow", "exp", "trig", "ln", "inv"])
    ds = [poly(rpoly(r, r.randint(1, 2))), "1+" + datom(r, "trig"), "1+" + datom(r, "exp"), datom(r, "root")]
    return frac(datom(r, nk), r.choice(ds + [xp(r.randint(1, 3))] * (nk != "pow")))    # no x^(1/3)/x^2

def a_x(r):
    k, u = r.choice([2, 3, 5, 10]), inner(r)[1]
    return r.choice([f"{k}^x", f"{k}^{par(u, '+-*/^')}", times(xp(r.randint(1, 3)), f"{k}^x"), frac(f"{k}^x", xp(r.randint(1, 2))),
                     times(f"{k}^x", datom(r, "trig")), f"{k}^{par(term(r.randint(2, 5), 'x'), '+-*/^')}"])

def logs(r):
    (k, u), n = inner(r), r.randint(1, 4)
    return r.choice([f"ln({'1+' * (k == 'exp')}{u})", times(xp(n), "ln(x)"), frac("ln(x)", xp(n)), pw("ln(x)", r.choice([2, 3])),
                     f"ln({frac(lin(r, 1)[0], lin(r, 1)[0])})", f"ln({datom(r, 'root')})", f"ln({times(xp(n), datom(r, 'root'))})",
                     f"ln({r.choice(['sec(x)+tan(x)', 'csc(x)-cot(x)', '1+cos(x)'])})"])

def invtrig(r):
    f, a = r.choice(["atan", "atan", "asin", "acos"]), r.randint(2, 5)
    simple = r.choice(["x^2", "x^3", term(a, "x"), "sqrt(x)", f"x/{a}", "1/x"])
    return r.choice([f"atan({inner(r)[1]})", f"{f}({simple})", times(xp(r.randint(1, 2)), f"{f}(x)"), pw(f"{f}(x)", 2),
                     "sqrt(1-x^2)*asin(x)", frac(f"{f}(x)", "1+x^2"), f"{f}({frac(lin(r, 1)[0], str(a))})"])

def x_x(r):
    a, tr = r.randint(2, 4), f"{r.choice(['sin', 'cos', 'tan'])}(x)"
    return r.choice(["x^x", f"x^{tr}", "x^ln(x)", "x^(1/x)", "x^sqrt(x)", "x^exp(x)", f"x^({term(a, 'x')})", f"{tr}^x",
                     "ln(x)^x", "(1+1/x)^x", f"({poly(rpoly(r, 2))})^x", "x^(x^2)", f"{tr}^ln(x)", "x^atan(x)"])

def sums(r):
    pick = [lambda: times(str(coef(r)), datom(r, r.choice(D_ATOMS[1:]))), lambda: chain2(r), lambda: product(r)]
    ts = [r.choice(pick)() for _ in range(r.choice([2, 2, 3]))]
    return join([t if i == 0 or r.random() < .6 else "-" + par(t, "+-") for i, t in enumerate(ts)])

D_FAMS = [(f.__name__.replace("_", "^"), f) for f in (product, quotient, chain2, chain3, a_x, logs, invtrig, x_x, sums)]

def main():
    ap = argparse.ArgumentParser(description="Random textbook calculus problems in giac syntax.")
    ap.add_argument("--seed", type=int, required=True)
    ap.add_argument("--kind", choices=["integrate", "diff"], required=True)
    ap.add_argument("--n", type=int, default=100, help="number of problems (default 100)")
    ap.add_argument("--tags", action="store_true", help="prefix each line with its family and a tab")
    args = ap.parse_args()
    sys.stdout.reconfigure(newline="\n")
    r, seen, order, fams = random.Random(args.seed), set(), [], INT_FAMS if args.kind == "integrate" else D_FAMS
    for _ in range(100 * args.n):
        if len(seen) >= args.n:
            break
        order = order or r.sample(fams, len(fams))     # shuffled rounds: equal family shares
        name, fam = order[-1]
        tag, line = gen_int(r, name, fam) if args.kind == "integrate" else (name, f"diff({fam(r)},x)")
        if line not in seen:    # a duplicate retries the same family
            seen.add(line)
            order.pop()
            print(f"{tag}\t{line}" if args.tags else line)

if __name__ == "__main__":
    main()
