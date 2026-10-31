// tutor.cc - DeltaCE's "Your turn" (tutor.h). The problem is solved once by giac and the steps
// engine (the book); each line the student types is then judged as mathematics, not as text:
//  - an equation is right when the problem's solutions all satisfy it and it lets in no other ones
//    (its two sides' difference a constant multiple of the last line's at a few test numbers, or,
//    when it is not (both sides squared, multiplied by x-1), giac's solve of the line); a step that
//    lets in more (squaring) is right with a note, as the book's checks at the end show;
//  - a system's line is right when the solution satisfies it (a consequence of the system);
//  - a factored form or a derivative is right when it equals the answer at test numbers.
// A right line is matched with the book's lines (both sides normalized, or the factors, or the
// derivatives left) to name its rule, or to say "a different way". A wrong one gets a number that
// works in one and not the other, and the slip when it is plain: one side right, a sign, the
// arithmetic. tools/hostgiac -Y runs it on lines read from a file.
#if defined FLOWCE_STEPS || defined HOSTGIAC
#include "tutor.h"
#include "steps.h"
#include "answer.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
using namespace giac;
#define NOINL __attribute__((noinline))

enum { T_EQ, T_SYS, T_FACTOR, T_DIFF };
struct bline { gen math, val; std::string title, formula, key, fine; signed char main, depth; short step; }; // (key: normalized, fine: as giac evaluates it)
struct tstate {
  int kind, pos, hintlv, hintfor, right, wrong, hints, nbook;
  bool extra, offbook, on;
  gen prob, var, last;   // the equation(s), polynomial or function; x or [x,y]; the last line kept
  vecteur sol;           // the solutions (values; a system's point), the factored form, f'
  std::vector<bline> book;
  std::string problem, lastkey, lastfine, lastraw;
};
static tstate * T;
static const context * C;

static NOINL gen ev(const gen & g) { ctrl_c = interrupted = false; return eval(g, 1, C); }
static NOINL std::string pr(const gen & g) { return g.print(C); }
static NOINL bool eq(const gen & g) { return g.is_symb_of_sommet(at_equal) && g._SYMBptr->feuille.type == _VECT && g._SYMBptr->feuille._VECTptr->size() == 2; }
static NOINL gen lhs(const gen & g) { gen h = g; while (eq(h)) h = h._SYMBptr->feuille[0]; return h; } // (a=b=c: a)
static NOINL gen rhs(const gen & g) { gen h = g; while (eq(h)) h = h._SYMBptr->feuille[1]; return h; } // (a=b=c: c)
static NOINL gen diffof(const gen & g) { return eq(g) ? lhs(g) - rhs(g) : g; }
// a line as the console reads it (implicit multiplication, the TI keys' words)
static NOINL gen rd(const char * s) {
  const int n = strlen(s), cap = 2 * n + 64;
  char * b = (char *)malloc(cap);
  if (!b) return undef;
  strcpy(b, s);
  answer_prepass(b, cap, true);
  ctrl_c = interrupted = false;
  gen g(b, C);
  free(b);
  return g;
}
static NOINL bool num(const gen & g, double & d) {
  const gen f = evalf(g, 1, C);
  if (f.type == _DOUBLE_) d = f._DOUBLE_val;
  else if (f.type == _INT_) d = f.val;
  else return false;
  return d == d && fabs(d) < 1e30;
}
static NOINL bool close(double a, double b) { return fabs(a - b) <= 1e-4 * (1 + fabs(a) + fabs(b)); }
static NOINL gen at(const gen & g, const gen & v) { // g at the point v (values of the variables)
  return T->var.type == _VECT ? subst(g, *T->var._VECTptr, *v._VECTptr, false, C) : subst(g, T->var, v, false, C);
}
static const double TP[5] = {0.37, 1.73, -1.21, 2.9, -2.6}; // test numbers: not where the problems' answers are
static NOINL gen tp(int i) { // test number i for each variable (a system: different ones)
  if (T->var.type != _VECT) return gen(TP[i]);
  vecteur v;
  for (unsigned k = 0; k < T->var._VECTptr->size(); ++k) v.push_back(gen(TP[(i + 2 * k) % 5] + 0.13 * k));
  return v;
}
// a factored form's irreducible factors, normalized, sorted (constants out): its key
static NOINL void factors(const gen & g, std::vector<std::string> & out) {
  if (g.is_symb_of_sommet(at_prod)) { const gen & f = g._SYMBptr->feuille; if (f.type == _VECT) for (unsigned i = 0; i < f._VECTptr->size(); ++i) factors((*f._VECTptr)[i], out); return; }
  if (g.is_symb_of_sommet(at_neg)) { factors(g._SYMBptr->feuille, out); return; }
  if (g.is_symb_of_sommet(at_pow) && g._SYMBptr->feuille[1].type == _INT_ && g._SYMBptr->feuille[1].val > 0) {
    for (int k = 0; k < g._SYMBptr->feuille[1].val; ++k) factors(g._SYMBptr->feuille[0], out);
    return;
  }
  if (lidnt(g).empty()) return; // (a number)
  gen n = normal(ev(g), C);
  double d; // (x+2 and -x-2: the same factor)
  if (num(subst(n, T->var, gen(TP[1]), false, C), d) && d < 0) n = normal(-n, C);
  out.push_back(pr(n));
}
static NOINL int nfac(const gen & g) { std::vector<std::string> f; factors(g, f); return f.size(); }
static NOINL std::string fkey(const gen & g) {
  std::vector<std::string> f;
  factors(g, f);
  for (unsigned i = 0; i < f.size(); ++i) for (unsigned j = i + 1; j < f.size(); ++j) if (f[j] < f[i]) { std::string t = f[i]; f[i] = f[j]; f[j] = t; }
  std::string k;
  for (unsigned i = 0; i < f.size(); ++i) { k += f[i]; k += '|'; }
  return k;
}
static NOINL int nderiv(const gen & g) { // derivatives not yet taken in g
  if (g.type == _SYMB) {
    int n = g.is_symb_of_sommet(at_derive);
    const gen & f = g._SYMBptr->feuille;
    if (f.type == _VECT) { for (unsigned i = 0; i < f._VECTptr->size(); ++i) n += nderiv((*f._VECTptr)[i]); }
    else n += nderiv(f);
    return n;
  }
  if (g.type == _VECT) { int n = 0; for (unsigned i = 0; i < g._VECTptr->size(); ++i) n += nderiv((*g._VECTptr)[i]); return n; }
  return 0;
}
// a line's fine key: each side as giac evaluates it (3*x/3 is x, 14+1 is 15, 3*(x+2) stays)
static NOINL std::string fine(const gen & g) {
  if (T->kind == T_FACTOR || T->kind == T_DIFF || !eq(g)) return pr(ev(T->kind == T_DIFF ? g : rhs(g)));
  std::string a = pr(ev(lhs(g))), b = pr(ev(rhs(g)));
  return a < b ? a + "=" + b : b + "=" + a;
}
// a line's key: the normalized sides (an equation), the factors, the derivatives left
static NOINL std::string key(const gen & g) {
  if (T->kind == T_FACTOR) return fkey(g);
  if (T->kind == T_DIFF) return print_INT_(nderiv(g));
  if (!eq(g)) return pr(normal(ev(g), C));
  std::string a = pr(normal(ev(lhs(g)), C)), b = pr(normal(ev(rhs(g)), C)); // (the steps keep 3*x/3 as written)
  return a < b ? a + "=" + b : b + "=" + a; // (15=3x and 3x=15 alike)
}
// var = constant: the value in *v
static NOINL bool assign(const gen & g, gen * v, gen * which) {
  if (!eq(g)) return false;
  const int sw = g._SYMBptr->feuille[0].type != _IDNT; // (5=x too)
  const gen & x = g._SYMBptr->feuille[sw], & c = g._SYMBptr->feuille[1 - sw];
  if (x.type != _IDNT) return false;
  bool isvar = T->var.type == _VECT ? equalposcomp(*T->var._VECTptr, x) != 0 : x == T->var;
  if (!isvar || !lidnt(c).empty()) return false;
  if (v) *v = c;
  if (which) *which = x;
  return true;
}
// the items of a line: a list, a sequence, "or"
static NOINL void items(const gen & g, vecteur & out) {
  if (g.type == _VECT) { for (unsigned i = 0; i < g._VECTptr->size(); ++i) items((*g._VECTptr)[i], out); return; }
  if (g.is_symb_of_sommet(at_ou) || g.is_symb_of_sommet(at_and)) { items(g._SYMBptr->feuille, out); return; }
  out.push_back(g);
}

// ---------------------------------------------------------------- the problem
const std::string & tutor_problem() { return T->problem; }
const char * tutor_ask() {
  static const char * const A[4] = {"Solve for x", "Solve the system", "Factor", "The derivative"};
  return T ? A[T->kind] : "";
}
static NOINL std::string shown(const gen & g) { // a line as the page writes it: a=b, not a=(b)
  if (eq(g)) return pr(g._SYMBptr->feuille[0]) + "=" + shown(g._SYMBptr->feuille[1]);
  if (g.type == _VECT) { std::string s; for (unsigned i = 0; i < g._VECTptr->size(); ++i) { if (i) s += ", "; s += shown((*g._VECTptr)[i]); } return s; }
  return pr(g);
}
static NOINL bool has_var(const gen & g) { // (in the printed line: the steps' held forms hide it from lidnt)
  const std::string t = pr(g);
  const vecteur v = T->var.type == _VECT ? *T->var._VECTptr : vecteur(1, T->var);
  for (unsigned k = 0; k < v.size(); ++k) {
    const std::string n = pr(v[k]);
    for (size_t p = t.find(n); p != std::string::npos; p = t.find(n, p + 1))
      if ((!p || !isalnum((unsigned char)t[p - 1])) && (p + n.size() >= t.size() || !isalnum((unsigned char)t[p + n.size()]))) return true;
  }
  return false;
}
// the book: the steps' lines, the main ones (an equation in x, a form of the polynomial or of the
// derivative) apart from the working (a, b and c, the discriminant, the checks)
static NOINL void make_book(const gen & cmd) {
  std::vector<step_t> v;
  T->book.clear();
  if (!steps_of(cmd, v, C)) return;
  int step = -1;
  for (unsigned i = 0; i < v.size(); ++i) {
    bline b;
    b.title = steps_title(v[i]);
    if (const char * f = steps_formula(v[i])) b.formula = f;
    b.math = steps_math(v[i], C);
    if (b.math.type == _VECT && b.math._VECTptr->size() == 1) b.math = b.math[0]; // ([a=b]: a=b)
    if (b.math.type == _IDNT && strchr(b.math._IDNTptr->id_name, '=')) { // (a line kept as written, a name: read it)
      ctrl_c = interrupted = false;
      const gen p(b.math._IDNTptr->id_name, C);
      if (!is_undef(p)) b.math = p;
    }
    b.depth = v[i].depth;
    if (!b.title.empty() && !b.depth) step = i;
    b.step = (short)step;
    b.main = 0;
    if (!b.depth) {
      const gen & m = b.math;
      if (T->kind == T_EQ || T->kind == T_SYS) {
        if (eq(m) && has_var(m)) { b.main = 1; b.val = m; }
        else if (m.type == _VECT && !m._VECTptr->empty()) { // x=2, x=3 (not a check: x=1, sqrt(1+3)=2)
          vecteur it;
          items(m, it);
          bool all = true;
          for (unsigned k = 0; k < it.size() && all; ++k) all = assign(it[k], 0, 0);
          if (all) { b.main = 1; b.val = m; }
        }
      } else {
        vecteur it; // (a factoring's line can be a list: its items)
        items(m, it);
        for (unsigned k = 0; k < it.size() && !b.main; ++k) {
          if (!eq(it[k])) continue;
          const gen l = lhs(it[k]);
          if (T->kind == T_FACTOR ? is_zero(normal(ev(l) - T->prob, C)) : pr(l).find("diff(") == 0) { b.main = 1; b.val = rhs(it[k]); }
        }
      }
    }
    if (b.main) { b.key = key(b.val); b.fine = fine(b.val); }
    T->book.push_back(b);
  }
}
static NOINL int count_steps(int from, int to) { // titled main steps in (from, to]
  int n = 0, s = -2;
  for (int i = from + 1; i <= to && i < (int)T->book.size(); ++i)
    if (T->book[i].main && T->book[i].step != s) { s = T->book[i].step; ++n; }
  return n;
}
static NOINL bool solutions(const gen & L, vecteur & out) { // an equation's real solutions; false: unknown
  const gen s = ev(symbolic(at_solve, makesequence(L, T->var)));
  if (s.type != _VECT) return false;
  for (unsigned i = 0; i < s._VECTptr->size(); ++i) { double d; if (!num(s[i], d)) return false; out.push_back(s[i]); }
  return true;
}
static NOINL bool inset(const gen & a, const vecteur & v) { // a value among v (numerically)
  double x, y;
  if (!num(a, x)) return false;
  for (unsigned i = 0; i < v.size(); ++i) if (num(v[i], y) && close(x, y)) return true;
  return false;
}
bool tutor_start(const char * cmd, const context * contextptr) {
  C = contextptr;
  if (!T) T = new tstate;
  T->book.clear(); T->sol.clear();
  T->pos = -1; T->hintlv = 0; T->hintfor = -2; T->right = T->wrong = T->hints = T->nbook = 0;
  T->extra = T->offbook = T->on = false;
  gen g = rd(cmd);
  if (is_undef(g)) return false;
  if (eq(g)) g = symbolic(at_solve, makesequence(g, gen(identificateur("x")))); // (an equation alone)
  if (g.type != _SYMB) return false;
  const gen f = g._SYMBptr->feuille;
  if (g.is_symb_of_sommet(at_factor)) { // factor(p): a polynomial in one letter that factors
    const vecteur l = lidnt(f);
    if (f.type == _VECT || l.size() != 1) return false;
    T->kind = T_FACTOR; T->prob = ev(f); T->var = l[0]; // (as giac writes it: 2*x^2-8, not (2*x^2)-8)
    const gen F = ev(g);
    if (fkey(F).size() <= fkey(f).size() && fkey(F) == fkey(f)) return false; // (nothing to factor)
    T->sol = vecteur(1, F);
    T->problem = pr(T->prob);
  } else if (g.is_symb_of_sommet(at_derive)) { // diff(f,x)
    if (f.type != _VECT || f._VECTptr->size() != 2 || f[1].type != _IDNT) return false;
    T->kind = T_DIFF; T->prob = f[0]; T->var = f[1];
    T->sol = vecteur(1, ev(g));
    T->problem = "diff(" + pr(f[0]) + "," + pr(f[1]) + ")";
  } else if (g.is_symb_of_sommet(at_solve)) {
    if (f.type != _VECT || f._VECTptr->size() != 2) return false;
    const gen a = f[0], b = f[1];
    T->kind = a.type == _VECT ? T_SYS : T_EQ;
    const std::string t = pr(a); // (not trig equations: their solutions repeat without end)
    if (t.find("sin(") != std::string::npos || t.find("cos(") != std::string::npos || t.find("tan(") != std::string::npos) return false;
    if (T->kind == T_EQ ? (!eq(a) || b.type != _IDNT) : (b.type != _VECT || b._VECTptr->size() < 2 || b._VECTptr->size() > 3)) return false;
    T->prob = a; T->var = b;
    gen s = ev(g);
    if (s.type != _VECT || s._VECTptr->empty()) return false; // (no solution, or not a list)
    if (T->kind == T_SYS) {
      if (s._VECTptr->size() != 1 || s[0].type != _VECT || s[0]._VECTptr->size() != b._VECTptr->size()) return false; // (one point)
      s = s[0];
    }
    for (unsigned i = 0; i < s._VECTptr->size(); ++i) { double d; if (!num(s[i], d)) return false; } // (real, no parameter)
    T->sol = *s._VECTptr;
    { // as giac writes each side ((x^2)-(5*x)+6 as typed through the prepass: x^2-5*x+6)
      vecteur it;
      items(a, it);
      T->problem.clear();
      for (unsigned i = 0; i < it.size(); ++i) {
        if (i) T->problem += ", ";
        T->problem += eq(it[i]) ? pr(ev(lhs(it[i]))) + "=" + pr(ev(rhs(it[i]))) : pr(it[i]);
      }
    }
  } else return false;
  T->last = T->prob;
  T->lastkey = key(T->prob);
  T->lastfine = fine(T->prob);
  make_book(g);
  for (int i = 0, st = -2; i < (int)T->book.size(); ++i) // (the steps with a main line)
    if (T->book[i].main && T->book[i].step != st) { st = T->book[i].step; ++T->nbook; }
  T->lastraw = pr(T->prob);
  return T->on = !T->book.empty();
}

// ---------------------------------------------------------------- a line
static NOINL std::string pt(const vecteur & p) { // x = 5, or (x, y) = (2, 1)
  if (T->var.type != _VECT) return pr(T->var) + " = " + pr(p[0]);
  std::string a = "(", b = "(";
  for (unsigned i = 0; i < p.size(); ++i) { if (i) { a += ", "; b += ", "; } a += pr((*T->var._VECTptr)[i]); b += pr(p[i]); }
  return a + ") = " + b + ")";
}
static NOINL std::string fmt(double d) { // 3 significant digits, as a person writes them (1.08, -0.54, 12.5, 140)
  std::string s;
  if (d < 0) { s = "-"; d = -d; }
  int dec = d >= 100 ? 0 : d >= 10 ? 1 : 2;
  long m = (long)(d * (dec == 2 ? 100 : dec ? 10 : 1) + 0.5);
  std::string ds = print_INT_((int)m);
  if (dec) {
    while ((int)ds.size() <= dec) ds = "0" + ds;
    ds.insert(ds.size() - dec, ".");
    while (ds[ds.size() - 1] == '0') ds.resize(ds.size() - 1);
    if (ds[ds.size() - 1] == '.') ds.resize(ds.size() - 1);
  }
  return s + ds;
}
// a = c*b at the test numbers: 1 yes, 0 no, -1 unknown (fewer than 2 numbers where both are defined)
static NOINL int ratio_same(const gen & a, const gen & b) {
  double r = 0, x, y;
  int n = 0;
  for (int i = 0; i < 5; ++i) {
    if (!num(at(a, tp(i)), x) || !num(at(b, tp(i)), y)) continue;
    if (fabs(y) < 1e-9) { if (fabs(x) > 1e-6) return 0; continue; }
    const double q = x / y;
    if (n && !close(q, r)) return 0;
    r = q; ++n;
  }
  return n < 2 ? -1 : fabs(r) > 1e-9;
}
static NOINL bool always_true(const gen & L) { // true at every test number (the letter canceled)
  int n = 0;
  for (int i = 0; i < 5; ++i) {
    double a, b;
    if (!num(at(lhs(L), tp(i)), a) || !num(at(rhs(L), tp(i)), b)) continue;
    if (!close(a, b)) return false;
    ++n;
  }
  return n >= 2;
}
static NOINL bool sameside(const gen & a, const gen & b) { return is_zero(normal(ev(a) - ev(b), C)); }
// a wrong equation's slip, when plain: only one side changed from the last line, or one side the
// book's next line has and the other off by a number or a sign
static NOINL std::string slip(const gen & L) {
  const gen a = lhs(L), b = rhs(L);
  for (int i = T->pos + 1, n = 0; i < (int)T->book.size() && n < 3; ++i) {
    const bline & B = T->book[i];
    if (!B.main || !eq(B.val)) continue;
    ++n;
    for (int sw = 0; sw < 2; ++sw) { // (its sides either way round)
      const gen ba = sw ? rhs(B.val) : lhs(B.val), bb = sw ? lhs(B.val) : rhs(B.val);
      for (int k = 0; k < 2; ++k) { // k 0: the left side is the book's, the right one is checked
        const gen & mine = k ? a : b, & other = k ? b : a, & good = k ? bb : ba, & bad = k ? ba : bb;
        if (!sameside(other, good)) continue;
        const char * side = k ? "left" : "right";
        if (sameside(mine, -bad)) return std::string("\nCheck the sign on the ") + side;
        if (lidnt(normal(ev(mine) - ev(bad), C)).empty()) return std::string("\nCheck the arithmetic on the ") + side;
        return std::string("\nThe ") + (k ? "right" : "left") + " side is right: check the " + side;
      }
    }
  }
  if (eq(T->last)) { // a side left exactly as it was, the other changed
    const std::string la = pr(ev(lhs(T->last))), lb = pr(ev(rhs(T->last))), ma = pr(ev(a)), mb = pr(ev(b));
    if ((la == ma) != (lb == mb)) return "\nYou changed one side only: do the same to both";
  }
  return "";
}
// a right line: its rule (the book's line it matches), or "a different way"; keeps it as the last
static NOINL void keep(tutor_res & r, const gen & line) {
  const std::string k = key(line), f = fine(line), w = pr(line);
  r.ok = 1;
  ++T->right;
  if (w == T->lastraw) { r.msg = "Right, but nothing changed"; return; }
  int m = -1;
  for (int pass = 0; pass < 2 && m < 0; ++pass) // (the book's line as written first: 3(x+2)=6(x-1) is the LCD's
    for (int i = (int)T->book.size() - 1; i >= 0 && m < 0; --i) // line, not the one distributed after it)
      if (T->book[i].main && (pass ? T->book[i].key == k : T->book[i].fine == f)) m = i;
  if (m < 0) { r.msg = "Right, a different way"; T->offbook = true; }
  else if (m == T->pos) r.msg = "Right, simplified";
  else if (m < T->pos) r.msg = "Right, but that is an earlier line";
  else {
    const int n = count_steps(T->pos, m), st = T->book[m].step;
    r.msg = st >= 0 ? T->book[st].title : "Right";
    if (n > 1) r.msg += "\n" + print_INT_(n) + " steps at once";
    T->pos = m;
    T->offbook = false;
  }
  T->last = line;
  T->lastkey = k;
  T->lastfine = f;
  T->lastraw = w;
}
static NOINL void solved(tutor_res & r) {
  int last = -1;
  for (unsigned i = 0; i < T->book.size(); ++i) if (T->book[i].main) last = i;
  const int n = count_steps(T->pos, last);
  r.ok = 1; r.solved = 1;
  ++T->right;
  r.msg = n > 1 ? "Solved, " + print_INT_(n) + " steps at once" : "Solved";
  T->pos = last;
}
static NOINL tutor_res wrong(const std::string & m) { tutor_res r; r.ok = 0; r.solved = 0; r.msg = m; ++T->wrong; return r; }

static NOINL tutor_res check_eq(const gen & g) {
  tutor_res r; r.ok = -1; r.solved = 0;
  vecteur it, vals, which;
  items(g, it);
  bool fin = !it.empty();
  for (unsigned i = 0; i < it.size() && fin; ++i) { gen v, w; fin = assign(it[i], &v, &w); vals.push_back(v); which.push_back(w); }
  if (fin && T->kind == T_SYS) { // a value for each letter: the point (one letter alone: a line)
    vecteur p = T->sol;
    bool all = true;
    for (unsigned k = 0; k < T->var._VECTptr->size() && all; ++k) all = equalposcomp(which, (*T->var._VECTptr)[k]) != 0;
    if (all) {
      for (unsigned i = 0; i < vals.size(); ++i) {
        const int k = equalposcomp(*T->var._VECTptr, which[i]) - 1;
        double a, b;
        if (!num(vals[i], a) || !num(T->sol[k], b) || !close(a, b)) return wrong("These values do not solve the system\nPut them into each equation");
      }
      solved(r);
      return r;
    }
  } else if (fin) { // x = 2, x = 3: the answer
    for (unsigned i = 0; i < vals.size(); ++i)
      if (!inset(vals[i], T->sol)) return wrong(pr(T->var) + " = " + pr(vals[i]) + " does not solve the problem" + (T->extra ? "\nSquaring let it in: check each answer" : "\nCheck it in the problem"));
    int miss = 0;
    for (unsigned i = 0; i < T->sol.size(); ++i) miss += !inset(T->sol[i], vals);
    if (miss) { // (right, not done: kept apart, the line before stays the last)
      r.ok = 1; ++T->right;
      r.msg = miss > 1 ? "Right so far: there are " + print_INT_(miss) + " more solutions" : std::string("Right so far: there is one more solution");
      return r;
    }
    solved(r);
    return r;
  }
  for (unsigned i = 0; i < it.size(); ++i) if (!eq(it[i])) { r.msg = "Write an equation, with ="; return r; }
  if (T->kind == T_SYS) { // each one true at the solution
    for (unsigned i = 0; i < it.size(); ++i) {
      double a, b;
      if (!num(at(lhs(it[i]), T->sol), a) || !num(at(rhs(it[i]), T->sol), b) || !close(a, b))
        return wrong(std::string("Not true at the system's solution") + slip(it[i]));
      if (always_true(it[i])) return wrong("That is true for any x and y:\nthe letters canceled");
    }
    keep(r, it.size() == 1 ? it[0] : g);
    return r;
  }
  if (it.size() > 1) { // x-2=0 or x-3=0: the solutions of the pieces together
    vecteur all;
    for (unsigned i = 0; i < it.size(); ++i) if (!solutions(it[i], all)) { keep(r, g); return r; }
    for (unsigned i = 0; i < T->sol.size(); ++i) if (!inset(T->sol[i], all)) return wrong("These lose a solution of the problem");
    for (unsigned i = 0; i < all.size(); ++i) if (!inset(all[i], T->sol)) return wrong(pt(vecteur(1, all[i])) + " solves one of these\nbut not the problem");
    keep(r, g);
    return r;
  }
  const gen L = it[0];
  for (unsigned i = 0; i < T->sol.size(); ++i) { // the solutions still solve it
    double a, b;
    if (!num(at(lhs(L), T->sol[i]), a) || !num(at(rhs(L), T->sol[i]), b))
      return wrong("This line is not defined\nat the problem's answer");
    if (!close(a, b)) { const std::string sl = slip(L); return wrong("Not the same equation as before" + (sl.empty() ? std::string("\nCheck this step") : sl)); }
  }
  if (always_true(L)) return wrong("That is true for every " + pr(T->var) + ":\nthe " + pr(T->var) + " canceled. Check this step");
  if (ratio_same(diffof(L), diffof(T->last)) != 1) { // not the last line times a number: its solutions
    vecteur s;
    if (solutions(L, s)) {
      bool more = false;
      for (unsigned i = 0; i < s.size(); ++i) more = more || !inset(s[i], T->sol);
      if (more) {
        keep(r, L);
        if (!T->extra) r.msg += "\nThis step can add solutions:\ncheck each answer at the end";
        T->extra = true;
        return r;
      }
    }
  }
  keep(r, L);
  return r;
}
static NOINL tutor_res check_expr(const gen & g) { // a factored form, or a derivative
  tutor_res r; r.ok = -1; r.solved = 0;
  gen E = eq(g) ? rhs(g) : g;
  if (E.type == _VECT) { r.msg = "One expression, please"; return r; }
  const gen want = T->kind == T_FACTOR ? T->prob : T->sol[0];
  const gen val = T->kind == T_DIFF ? ev(E) : E; // (derivatives left in it: taken)
  for (int i = 0; i < 5; ++i) {
    double a, b;
    if (!num(at(val, tp(i)), a) || !num(at(want, tp(i)), b) || close(a, b)) continue;
    static const signed char N[5] = {2, 1, 3, -1, 0}; // (a whole number to show it)
    for (int k = 0; k < 5; ++k) {
      double c, d;
      if (!num(at(val, gen(N[k])), c) || !num(at(want, gen(N[k])), d) || close(c, d)) continue;
      std::string m = "At " + pr(T->var) + " = " + print_INT_(N[k]) + " this is " + fmt(c) + ",\nit should be " + fmt(d);
      if (T->kind == T_DIFF) for (unsigned j = 0; j < T->book.size(); ++j) if (!T->book[j].title.empty()) { m += "\nCheck: " + T->book[j].title; break; }
      return wrong(m);
    }
    return wrong("This does not equal it everywhere");
  }
  if (T->kind == T_DIFF ? !nderiv(E) : nfac(E) >= nfac(T->sol[0])) { // (no derivative left; all the factors)
    if (pr(E) == T->lastraw && T->right) { keep(r, E); return r; }
    solved(r);
    return r;
  }
  keep(r, E);
  if (T->kind == T_FACTOR && r.msg.find("nothing") == std::string::npos) r.msg += "\nCan it be factored more?";
  return r;
}
tutor_res tutor_check(const char * line, const context * contextptr) {
  C = contextptr;
  tutor_res r; r.ok = -1; r.solved = 0;
  if (!T || !T->on || !line || !*line) { r.msg = "Type a line first"; return r; }
  const gen g = rd(line);
  if (is_undef(g) || g.type == _STRNG) { r.msg = "Not read: check the line"; return r; }
  const vecteur l = lidnt(g);
  for (unsigned i = 0; i < l.size(); ++i) {
    const std::string n = pr(l[i]);
    if (n == "pi" || n == "e" || n == "i") continue;
    if (T->var.type == _VECT ? !equalposcomp(*T->var._VECTptr, l[i]) : l[i] != T->var) {
      r.msg = "Only " + (T->var.type == _VECT ? pr(T->var[0]) + " and " + pr(T->var[1]) : pr(T->var)) + " here, not " + n;
      return r;
    }
  }
  return T->kind == T_EQ || T->kind == T_SYS ? check_eq(g) : check_expr(g);
}

// ---------------------------------------------------------------- hints
int tutor_hint(std::string & text, std::vector<std::string> & lines, const context * contextptr) {
  C = contextptr;
  if (!T) return 0;
  if (T->offbook && (T->kind == T_EQ || T->kind == T_FACTOR)) { // a way of their own: the book from their last line
    const gen cmd = T->kind == T_EQ ? symbolic(at_solve, makesequence(T->last, T->var)) : symbolic(at_factor, T->last);
    std::vector<bline> keepbook = T->book;
    make_book(cmd);
    if (T->book.empty()) T->book = keepbook;
    else T->pos = -1;
    T->offbook = false;
    T->hintfor = -2;
  }
  int j = T->pos + 1;
  while (j < (int)T->book.size() && (T->book[j].title.empty() || T->book[j].depth)) ++j;
  if (j >= (int)T->book.size()) return 0;
  if (T->hintfor != j) { T->hintfor = j; T->hintlv = 0; }
  const bline & B = T->book[j];
  if (++T->hintlv == 1) { ++T->hints; text = "Next: " + B.title; return 1; }
  if (T->hintlv == 2 && !B.formula.empty()) { text = B.formula; return 2; }
  int e = j; // its lines: the titled one, those going on with it, its parts
  while (e + 1 < (int)T->book.size() && (T->book[e + 1].title.empty() || T->book[e + 1].depth)) ++e;
  for (int i = j; i <= e; ++i) {
    lines.push_back(shown(T->book[i].math));
    if (T->book[i].main) { T->last = T->book[i].val; T->lastkey = T->book[i].key; T->lastfine = T->book[i].fine; T->lastraw = pr(T->book[i].val); }
  }
  T->pos = e; // (past them, working lines too: a, b and c)
  text = B.title;
  T->hintlv = 0; T->hintfor = -2;
  return 3;
}
#ifdef HOSTGIAC
void tutor_dump() { // (the PC: the book, as tutor.cc reads it)
  for (unsigned i = 0; i < T->book.size(); ++i) {
    const bline & b = T->book[i];
    printf("      book %2u d%d step %2d %s %-28s %-30s key %s\n", i, b.depth, b.step, b.main ? "MAIN" : "    ", b.title.c_str(), shown(b.math).c_str(), b.key.c_str());
  }
}
#endif
void tutor_counts(int & right, int & wrong, int & hints, int & book) {
  right = T ? T->right : 0; wrong = T ? T->wrong : 0; hints = T ? T->hints : 0; book = T ? T->nbook : 0;
}
#endif
