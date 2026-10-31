// focus_practice.cc - DeltaCE's practice (more > Practice): problems made up by topic (linear and
// quadratic equations, systems, rational and radical equations, derivatives, factoring), solved
// line by line on the "Your turn" screen (focus_tutor.cc, tutor.cc): each line checked, hints from
// the steps, F4 another problem. The problems come from patterns with random whole numbers chosen
// so that the answers are clean; giac computes what the screen shows.
// WITH_PRACTICE (the steps build, DeltaCE; needs WITH_TUTOR): else the more menu has no Practice,
// focus_practice() is empty
#if defined FLOWCE_STEPS && defined WITH_PRACTICE
#include <string.h>
#include <stdlib.h>
#include <giac/giacPCH.h> // first: its "#define std ustl"
#include "k_csdk.h"
#include "k_defs.h"
#include "focus.h"
#include "ui_gfx.h"
#include "ui_font.h"
#include "ui_fontdata.h"
using namespace giac;
extern giac::context * contextptr;
extern "C" volatile unsigned getkey_polls;
#define NOINL __attribute__((noinline))

enum { SB = 16, MB = 22, SBOT = UI_H - MB, Y0 = SB, NT = 7 };
static const char * const TOPIC[NT] = {"Linear equations", "Quadratics", "Systems", "Rational equations",
                                       "Radical equations", "Derivatives", "Factoring"};
static const char * const TPREV[NT] = {"3x+5=11", "x^2-5x+6=0", "[2x+y=5,x-y=1]", "3/(x-1)=6/(x+2)",
                                       "sqrt(x+3)=x-3", "diff(x^3*sin(x),x)", "x^2+5x+6"};
static int topic;
static unsigned long seed;
static std::string * cmd_;

static NOINL int R(int a, int b) { // a whole number in [a, b]
  seed = seed * 1103515245UL + 12345UL;
  return a + (int)((seed >> 8) % (unsigned long)(b - a + 1));
}
static NOINL int Rnz(int a, int b) { int v; do v = R(a, b); while (!v); return v; }
// pattern p with %0..%5 the numbers v (negative ones in parentheses)
static NOINL std::string pat(const char * p, const int * v) {
  std::string s;
  for (; *p; ++p) {
    if (*p == '%' && p[1] >= '0' && p[1] <= '5') {
      const int n = v[*++p - '0'];
      if (n < 0) s += '(';
      s += print_INT_(n);
      if (n < 0) s += ')';
    } else s += *p;
  }
  return s;
}
static NOINL gen rd(const std::string & s) { return gen(s, contextptr); }
static NOINL gen ev(const gen & g) { giac::ctrl_c = giac::interrupted = false; return eval(g, 1, contextptr); }
static NOINL std::string pr(const gen & g) { return g.print(contextptr); }
// an equation "L=R" with each side evaluated as giac writes it (3*x+(-5) is 3*x-5, 1*x is x)
static NOINL std::string eqn(const std::string & s) {
  const gen e = rd(s);
  if (!e.is_symb_of_sommet(at_equal)) return pr(ev(e));
  return pr(ev(e._SYMBptr->feuille[0])) + "=" + pr(ev(e._SYMBptr->feuille[1])); // (giac would write =(x+27))
}
// the next problem of the topic, as a command (solve(...,x), solve([...],[x,y]), diff(...), factor(...))
static NOINL const char * make() {
  int v[6];
  std::string & c = *cmd_;
  switch (topic) {
  case 0: { // a x + b = c x + d, x whole
    v[4] = R(-9, 9); v[0] = R(2, 9); do v[2] = R(-4, 4); while (v[2] == v[0]);
    v[1] = R(-12, 12); v[3] = (v[0] - v[2]) * v[4] + v[1];
    c = "solve(" + eqn(pat("%0*x+%1=%2*x+%3", v)) + ",x)";
    break;
  }
  case 1: { // a (x - r) (x - s) = 0 expanded
    static const char A[5] = {1, 1, 1, 2, 3};
    v[0] = A[R(0, 4)]; v[1] = R(-9, 9); v[2] = R(-9, 9);
    c = "solve(" + pr(ev(rd(pat("expand(%0*(x-%1)*(x-%2))", v)))) + "=0,x)";
    break;
  }
  case 2: { // a x + b y = e, c x + d y = f
    const int x = R(-6, 6), y = R(-6, 6);
    do { v[0] = Rnz(-5, 5); v[1] = Rnz(-5, 5); v[2] = Rnz(-5, 5); v[3] = Rnz(-5, 5); } while (v[0] * v[3] == v[1] * v[2]);
    v[4] = v[0] * x + v[1] * y; v[5] = v[2] * x + v[3] * y;
    c = "solve([" + eqn(pat("%0*x+%1*y=%4", v)) + "," + eqn(pat("%2*x+%3*y=%5", v)) + "],[x,y])";
    break;
  }
  case 3: { // k(s+p)/(x+p) = k(s+q)/(x+q): x = s (s+p, s+q not 0)
    const int s = R(-6, 6);
    do { v[1] = Rnz(-5, 5); v[3] = Rnz(-5, 5); } while (v[1] == v[3] || s + v[1] == 0 || s + v[3] == 0);
    const int k = R(1, 3);
    v[0] = k * (s + v[1]); v[2] = k * (s + v[3]);
    c = "solve(" + eqn(pat("%0/(x+%1)=%2/(x+%3)", v)) + ",x)";
    break;
  }
  case 4: { // sqrt(a x + b) = c, or sqrt(x + a) = x + b (squaring lets in a second answer)
    const int s = R(-4, 8);
    if (R(0, 1)) { v[0] = R(1, 4); v[2] = R(1, 6); v[1] = v[2] * v[2] - v[0] * s; c = "solve(" + eqn(pat("sqrt(%0*x+%1)=%2", v)) + ",x)"; }
    else { v[1] = R(1, 5) - s; v[0] = (s + v[1]) * (s + v[1]) - s; c = "solve(" + eqn(pat("sqrt(x+%0)=x+%1", v)) + ",x)"; }
    break;
  }
  case 5: { // a pattern with small numbers
    static const char * const D[7] = {"%0*x^%1+%2*x^2-%3*x", "x^%1*sin(x)", "sin(%0*x^2)", "e^(%0*x)*x", "(%0*x+%2)^%1", "ln(x^2+%2)", "x/(x+%2)"};
    v[0] = R(2, 5); v[1] = R(2, 5); v[2] = R(1, 9); v[3] = R(1, 9);
    c = "diff(" + pr(ev(rd(pat(D[R(0, 6)], v)))) + ",x)";
    break;
  }
  default: { // (x + a)(x + b), a (x + b)(x + c), (p x + q)(x + s), x^2 - b^2: expanded
    static const char * const F[4] = {"(x+%1)*(x+%2)", "%0*(x+%1)*(x+%2)", "(%0*x+%1)*(x+%2)", "x^2-%3^2"};
    v[0] = R(2, 3); v[1] = Rnz(-9, 9); do v[2] = Rnz(-9, 9); while (v[2] == -v[1]); v[3] = R(2, 9);
    c = "factor(" + pr(ev(rd("expand(" + pat(F[R(0, 3)], v) + ")"))) + ")";
  }
  }
  return c.c_str();
}
void focus_practice() {
  giac::ctrl_c = giac::interrupted = false;
  if (!cmd_) cmd_ = new std::string;
  seed ^= getkey_polls * 2654435761UL + (unsigned long)RTC_GetTicks();
  const int view0 = focus_view;
  focus_view = 1;
  for (;;) { // a topic, then its problems on the "Your turn" screen until back
    focus_status_label("PRACTICE");
    static const char * const tabs[5] = {"", "", "", "", "back"};
    focus_bar_tabs(tabs, -1);
    ui_fill(0, Y0, UI_W, SBOT - Y0, ui_col(0, UC_BG));
    const int t = focus_card_list(3, "topic", TPREV, TOPIC, 0, NT, topic);
    if (t < 0) break;
    topic = t;
    focus_tutor(make(), make, "PRACTICE");
  }
  focus_view = view0;
  focus_status_label(0);
  focus_invalidate();
}
#elif defined FLOWCE_STEPS
#include "focus.h"
void focus_practice() {}
#endif
