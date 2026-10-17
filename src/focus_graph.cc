// focus_graph.cc - DeltaCE's graph screen, for y = f(x) (one to four functions; other plots stay
// KhiCAS's graph view). Made to be easier than the calculator's own grapher:
// - a bar that says what the keys do: pan/trace, zoom, points, table, more;
// - pan: the arrows move the view while held, + and - zoom while held (on the middle of the
//   screen); trace: the arrows move a cursor along a curve (up/down: the next curve);
// - the grid and its numbers follow the zoom (steps of 1, 2, 5 x 10^k); zoomed in, dots on the
//   curves at the grid's x, and their coordinates when that setting is on;
// - the key points (zeros, extrema, inflection points from the analysis, analyze.cc; where the
//   curves cross), exact; the cursor jumps to them (F3, 2nd left/right);
// - a table of values, the tangent line, the area under the curve, f' too, the analysis.
// Fast: each function is compiled to a small program over floats (giac's evalf took 0.4-0.8 s a
// redraw on the calculator). A frame is about 0.5 s; while a key is held, 0.1 to 0.2 s (every 3rd
// column, lines without anti-aliasing; a view moved by whole pixels reuses the rows it had).
#ifdef FLOWCE_STEPS
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <giac/giacPCH.h> // first: its "#define std ustl"
#include "k_csdk.h"
#include "focus.h"
#include "ui_gfx.h"
#include "ui_font.h"
#include "ui_fontdata.h"
#include "ui_math.h"
#include "analyze.h"
using namespace giac;
extern giac::context * contextptr;
extern int dconsole_mode; // console.cc: 0, giac's notes go nowhere
#define NOINL __attribute__((noinline))

int focus_prompt(const char * title, const char * label, std::string & s, bool numeric); // focus_menu.cc

// ---------------------------------------------------------------- the functions, compiled
enum { O_X, O_C, O_ADD, O_MUL, O_NEG, O_INV, O_IPOW, O_POW, O_ROOT, O_F1 };
enum { F_SIN, F_COS, F_TAN, F_EXP, F_LN, F_SQRT, F_ABS, F_ASIN, F_ACOS, F_ATAN, F_SINH, F_COSH, F_TANH, F_LOG10, F_FLOOR, F_CEIL };
enum { NMAX = 4, PMAX = 80, SMAX = 20 };
struct prog { unsigned char op[PMAX], fn[PMAX]; float c[PMAX]; unsigned char n, depth, ok; };
static prog * P_;          // the functions' programs
static gen * F_;           // the functions (giac), F_[i]
static int NF;             // how many
static gen * X_;           // the variable
static const unsigned char COLS[NMAX] = {UC_ACC, UC_RED, UC_PURPLE, UC_ORANGE};

static NOINL bool emit(prog & p, int op, float c, int fn, int d) {
  if (p.n >= PMAX) return false;
  p.op[p.n] = (unsigned char)op; p.c[p.n] = c; p.fn[p.n] = (unsigned char)fn; ++p.n;
  if (d > p.depth) p.depth = (unsigned char)d;
  return d < SMAX;
}
// g as a float (NAN: not a real number)
static NOINL float fl(const gen & g) {
  const gen z = evalf(g, 1, contextptr);
  return z.type == _INT_ ? (float)z.val : z.type == _DOUBLE_ ? (float)z._DOUBLE_val : NAN;
}
static bool fin(float v);
static NOINL bool cnum(const gen & g, float & v) { // a number (pi, 2/3, sqrt(2) too)
  if (g.type == _IDNT && g == *X_) return false;
  v = fl(g);
  return fin(v);
}
// g compiled into p, its value at stack depth d (+1)
static bool comp(prog & p, const gen & g, int d) {
  float v;
  if (g.type == _IDNT && g == *X_) return emit(p, O_X, 0, 0, d + 1);
  if (g.type != _SYMB) return cnum(g, v) && emit(p, O_C, v, 0, d + 1);
  if (!depend(g, *X_->_IDNTptr)) return cnum(g, v) && emit(p, O_C, v, 0, d + 1);
  const unary_function_ptr & s = g._SYMBptr->sommet;
  const gen & a = g._SYMBptr->feuille;
  if (s == at_plus || s == at_prod) {
    if (a.type != _VECT || a._VECTptr->empty()) return false;
    const vecteur & v2 = *a._VECTptr;
    if (!comp(p, v2[0], d)) return false;
    for (unsigned i = 1; i < v2.size(); ++i)
      if (!comp(p, v2[i], d + 1) || !emit(p, s == at_plus ? O_ADD : O_MUL, 0, 0, d + 1)) return false;
    return true;
  }
  if (s == at_neg) return comp(p, a, d) && emit(p, O_NEG, 0, 0, d + 1);
  if (s == at_inv) return comp(p, a, d) && emit(p, O_INV, 0, 0, d + 1);
  if (s == at_pow && a.type == _VECT && a._VECTptr->size() == 2) {
    const gen & b = a[0], & e = a[1];
    if (e.type == _INT_ && e.val >= -64 && e.val <= 64) return comp(p, b, d) && emit(p, O_IPOW, (float)e.val, 0, d + 1);
    if (e.type == _FRAC && e._FRACptr->num.type == _INT_ && e._FRACptr->den.type == _INT_) {
      const int q = e._FRACptr->den.val, n = e._FRACptr->num.val;
      if (q == 2 && n == 1) return comp(p, b, d) && emit(p, O_F1, 0, F_SQRT, d + 1);
      if (q & 1) // an odd root, real for negative bases too (the textbooks' cube root of -8: -2)
        return comp(p, b, d) && emit(p, O_ROOT, (float)q, 0, d + 1) && (n == 1 || emit(p, O_IPOW, (float)n, 0, d + 1));
    }
    return comp(p, b, d) && comp(p, e, d + 1) && emit(p, O_POW, 0, 0, d + 2);
  }
  if (s == at_surd && a.type == _VECT && a._VECTptr->size() == 2 && a[1].type == _INT_)
    return comp(p, a[0], d) && emit(p, O_ROOT, (float)a[1].val, 0, d + 1);
  static const unary_function_ptr * const * const fs[] = {&at_sin, &at_cos, &at_tan, &at_exp, &at_ln, &at_sqrt, &at_abs,
    &at_asin, &at_acos, &at_atan, &at_sinh, &at_cosh, &at_tanh, &at_log10, &at_floor, &at_ceil};
  for (int k = 0; k < (int)(sizeof(fs) / sizeof(fs[0])); ++k)
    if (s == **fs[k]) return comp(p, a, d) && emit(p, O_F1, 0, k, d + 1);
  return false;
}
// sin and cos for the curves' pixels (quick_): a table, 3e-5; sinf takes 20 float operations, each a
// call to the OS's routines (1000 cycles), this 3. A period is 2^22, the phase the low bits of u 2^22/2pi.
static const unsigned short QS[129] = {
  0, 804, 1608, 2412, 3216, 4019, 4821, 5623, 6424, 7223, 8022, 8820, 9616, 10411, 11204, 11996,
  12785, 13573, 14359, 15142, 15924, 16703, 17479, 18253, 19024, 19792, 20557, 21319, 22078, 22834, 23586, 24334,
  25079, 25820, 26557, 27291, 28020, 28745, 29465, 30181, 30893, 31600, 32302, 32999, 33692, 34379, 35061, 35738,
  36409, 37075, 37736, 38390, 39039, 39682, 40319, 40950, 41575, 42194, 42806, 43411, 44011, 44603, 45189, 45768,
  46340, 46905, 47464, 48014, 48558, 49095, 49624, 50145, 50659, 51166, 51664, 52155, 52638, 53113, 53580, 54039,
  54490, 54933, 55367, 55794, 56211, 56620, 57021, 57413, 57797, 58171, 58537, 58895, 59243, 59582, 59913, 60234,
  60546, 60850, 61144, 61429, 61704, 61970, 62227, 62475, 62713, 62942, 63161, 63371, 63571, 63762, 63943, 64114,
  64276, 64428, 64570, 64703, 64826, 64939, 65042, 65136, 65219, 65293, 65357, 65412, 65456, 65491, 65515, 65530,
  65535};
static bool quick_;
static float qsin(float u, unsigned long shift) {
  const float p = u * 667544.214f;
  if (!(p > -2.1e9f && p < 2.1e9f)) return shift ? cosf(u) : sinf(u);
  const unsigned long t = (unsigned long)(long)p + shift;
  const int q = (int)(t >> 20) & 3;
  int r = (int)(t & 0xFFFFF);
  if (q & 1) r = 0x100000 - r;
  const int i = r >> 13, f = r & 0x1FFF; // (128 steps a quarter)
  const int v = i == 128 ? QS[128] : QS[i] + (((int)QS[i + 1] - (int)QS[i]) * f >> 13);
  const float s = v * (1.0f / 65535);
  return q & 2 ? -s : s;
}
static float fun1(int k, float u) {
  if (quick_ && k <= F_TAN) {
    if (k == F_SIN) return qsin(u, 0);
    if (k == F_COS) return qsin(u, 0x100000);
    return qsin(u, 0) / qsin(u, 0x100000);
  }
  switch (k) {
  case F_SIN: return sinf(u); case F_COS: return cosf(u); case F_TAN: return tanf(u);
  case F_EXP: return expf(u); case F_LN: return u > 0 ? logf(u) : NAN;
  case F_SQRT: return u >= 0 ? sqrtf(u) : NAN; case F_ABS: return fabsf(u);
  case F_ASIN: return u >= -1 && u <= 1 ? asinf(u) : NAN; case F_ACOS: return u >= -1 && u <= 1 ? acosf(u) : NAN;
  case F_ATAN: return atanf(u); case F_SINH: return sinhf(u); case F_COSH: return coshf(u); case F_TANH: return tanhf(u);
  case F_LOG10: return u > 0 ? log10f(u) : NAN; case F_FLOOR: return floorf(u); case F_CEIL: return ceilf(u);
  }
  return NAN;
}
// v a number (not NaN, not infinite), by its bits: the CE's float library compares NaN equal to itself
static NOINL bool fin(float v) {
  union { float f; unsigned long u; } q;
  q.f = v;
  return (q.u & 0x7f800000UL) != 0x7f800000UL;
}
// g at x: its program p, else giac (a function the programs cannot do: slow)
static NOINL float run_prog(const prog & p, const gen & g, float x) {
  if (!p.ok) return fl(subst(g, *X_, gen((double)x), false, contextptr));
  float s[SMAX];
  int k = -1;
  for (int j = 0; j < p.n; ++j) {
    const float c = p.c[j];
    switch (p.op[j]) {
    case O_X: s[++k] = x; break;
    case O_C: s[++k] = c; break;
    case O_ADD: s[k - 1] += s[k]; --k; break;
    case O_MUL: s[k - 1] *= s[k]; --k; break;
    case O_NEG: s[k] = -s[k]; break;
    case O_INV: s[k] = 1 / s[k]; break;
    case O_IPOW: { // (x^2: one multiplication)
      int e = (int)c;
      float b = e < 0 ? 1 / s[k] : s[k], r = 1;
      bool one = true;
      for (e = e < 0 ? -e : e;;) {
        if (e & 1) { r = one ? b : r * b; one = false; }
        if (!(e >>= 1)) break;
        b *= b;
      }
      s[k] = r;
    } break;
    case O_POW: s[k - 1] = powf(s[k - 1], s[k]); --k; break;
    case O_ROOT: { const float u = s[k]; s[k] = u < 0 ? -powf(-u, 1 / c) : powf(u, 1 / c); } break;
    case O_F1: s[k] = fun1(p.fn[j], s[k]); break;
    }
  }
  return k == 0 ? s[0] : NAN;
}
static NOINL float val(int i, float x) { return run_prog(P_[i], F_[i], x); } // f_i at x

// ---------------------------------------------------------------- the view
enum { GT = 16, GB = 218, GH = GB - GT, BAR_H = UI_H - GB };
static float vx0, vx1, vy0, vy1;          // the window
static float w0;                          // its width when the graph opened: zoomed in from it, dots
// (the frame: frame below)
static int mode;                          // 0 pan, 1 trace
static int tf;                            // trace: the curve
static float tx;                          // trace: x
static bool coords, grid = true;          // dots' coordinates shown; the grid
static bool klabels;                      // the key points' coordinates on the graph
static bool tang;                         // the tangent line at the trace point
static float area_a, area_b;              // the area under the curve shaded (a < b), else a = b
static char note[64];                     // the readout's own line (a key point, the area, the tangent)
static int note_x;                        // ... for the trace x it was made for (pixel column), -1: any

static NOINL float col_x(float c) { return vx0 + (vx1 - vx0) * c / (UI_W - 1); }
static NOINL float row_of(float y) { return GT + (vy1 - y) * (GH - 1) / (vy1 - vy0); }
static NOINL int clampi(float v) { return v < -1000 ? -1000 : v > 2000 ? 2000 : v >= 0 ? (int)(v + 0.5f) : -(int)(0.5f - v); } // (rounded; no floorf)
static NOINL int px_of(float x) { return clampi((x - vx0) * (UI_W - 1) / (vx1 - vx0)); }
static NOINL int py_of(float y) { return clampi(row_of(y)); }

// the step of the grid: 1, 2 or 5 x 10^k, at least minpx pixels apart (no log10f, powf: slow)
static NOINL float step_of(float range, int pixels, int minpx) {
  const float raw = range * minpx / pixels;
  float m = 1;
  for (int k = 0; m * 10 <= raw && k < 40; ++k) m *= 10;
  for (int k = 0; m > raw && k < 40; ++k) m /= 10;
  const float r = raw / m;
  return (r <= 1 ? 1 : r <= 2 ? 2 : r <= 5 ? 5 : 10) * m;
}
// v with dec decimals (at most 5); from a million: 1.23e7
static void fmtd(char * b, float v, int dec);
static NOINL void trim(char * b) { // 2.500: 2.5, 3.000: 3
  char * d = strchr(b, '.');
  if (!d) return;
  char * e = b + strlen(b) - 1;
  while (e > d && *e == '0') *e-- = 0;
  if (e == d) *e = 0;
}
static NOINL void expo(char * b, int e) { // e-5, e7
  b += strlen(b); *b++ = 'e';
  if (e < 0) { *b++ = '-'; e = -e; }
  char t[4]; int n = 0;
  do { t[n++] = '0' + e % 10; e /= 10; } while (e && n < 3);
  while (n) *b++ = t[--n];
  *b = 0;
}
static void fmtd(char * b, float v, int dec) {
  float a = fabsf(v), sc = 1;
  if (a >= 1e6f) {
    int e = 0;
    while (a >= 10 && e < 40) { a /= 10; ++e; }
    fmtd(b, v < 0 ? -a : a, 2); trim(b); expo(b, e);
    return;
  }
  for (int k = 0; k < dec; ++k) sc *= 10;
  while (dec > 0 && a * sc >= 2e9f) { --dec; sc /= 10; } // (a long)
  long n = (long)(a * sc + 0.5f), ip = n;
  if (v < 0 && n) *b++ = '-';
  for (int k = 0; k < dec; ++k) ip /= 10;
  char t[12]; int m = 0;
  do { t[m++] = '0' + ip % 10; ip /= 10; } while (ip && m < 11);
  while (m) *b++ = t[--m];
  if (dec) {
    long fr = n;
    char d[8];
    for (int k = dec - 1; k >= 0; --k) { d[k] = '0' + fr % 10; fr /= 10; }
    *b++ = '.';
    for (int k = 0; k < dec; ++k) *b++ = d[k];
  }
  *b = 0;
}
// v written with the decimals the step needs (step 0.05: 2), at most 5 (no log10f: slow)
static NOINL void fmt(char * b, float v, float step) {
  int dec = 0;
  for (float t = step; t < 0.999f && dec < 5; t *= 10) ++dec;
  fmtd(b, fabsf(v) < step * 1e-3f ? 0 : v, dec);
}
// v with 6 significant digits, no trailing zeros (the table, the tangent): 7940, 0.333333, 1.2e-5
static NOINL void fmtg(char * b, float v, int sig = 6) {
  float a = fabsf(v);
  if (a != 0 && a < 1e-4f) {
    int e = 0;
    while (a < 1 && e < 40) { a *= 10; ++e; }
    fmtd(b, v < 0 ? -a : a, 3); trim(b); expo(b, -e);
    return;
  }
  int d = sig;
  for (float t = 1; t <= a && d > 0; t *= 10) --d;
  fmtd(b, v, d > 5 ? 5 : d); trim(b);
}

// The frame: what is drawn, worked out once (floats, slow on the eZ80: the OS's routines), then
// drawn band by band in integers
enum { NONE = -0x7fff00, TOPC = (GT - 64 * GH) * 16, BOTC = (GB + 64 * GH) * 16, NG = 16, ND = 40, NKP = 40 };
struct gl { float v; int p; unsigned char w; char t[14]; }; // a grid line: its value, where, its number
struct dt { int x, y; unsigned char f, w; char t[30]; };  // a dot on a curve: where, its coordinates
struct frame {
  int y[NMAX * UI_W];            // the curves' rows at each column, 1/16 px (NONE: none)
  unsigned char j[NMAX * UI_W];  // joined to the column sampled before
  gl gx[NG], gy[NG];
  dt d[ND];
  int kx[NKP], ky[NKP];          // the key points
  short lx[NKP], ly[NKP];        // their coordinates' places (lx < 0: not shown, over another)
  char ln[64];                   // the readout's line
  int ngx, ngy, nd;
};
static frame * FR;
// y's row in 1/16 px, within 64 screens above and below (TOPC, BOTC); NONE: not a number
static int row16(float v) {
  if (!fin(v)) return NONE;
  const float h = vy1 - vy0;
  if (v > vy1 + 64 * h) return TOPC;
  if (v < vy0 - 64 * h) return BOTC;
  return (int)(GT * 16 + (vy1 - v) * (16 * (GH - 1)) / h);
}
// curve i continuous from column c0 (row a) to c1 (row z): a steep step is checked halfway (not
// joined across tan(x)'s asymptote, 1/x at 0: halfway it is not between them)
static NOINL bool joined(int i, int c0, int c1, int a, int z) {
  if ((a > z ? a - z : z - a) <= GH * 16 / 3) return true;
  const int m = row16(val(i, col_x((c0 + c1) * 0.5f)));
  if (m == NONE) return false;
  if ((a == TOPC && z == BOTC) || (a == BOTC && z == TOPC)) return m != TOPC && m != BOTC;
  return m >= (a < z ? a : z) - 32 && m <= (a < z ? z : a) + 32;
}
// the curves' rows (every 3 columns while a key is held); each float operation is a call to the
// OS (1000 cycles): the fewest, the row as A - v k
static float lx0, ly0, lw, lh; // the view the rows are for
static int lev;                // ... sampled every lev columns (0: none, the functions changed)
static int accy;               // rows moved up or down since all were evaluated (TOPC, BOTC: kept)
static int shx, shy;           // the last sample's shift (pixels; -1000: not a shift)
static int lnk = -1;           // the key points the frame's pixels are for (-2: new ones)
static NOINL void sample(int every) {
  const float w = vx1 - vx0, h = vy1 - vy0, pw = w / (UI_W - 1), dx = pw * every, k = 16 * (GH - 1) / h, A = GT * 16 + vy1 * k, top = vy1 + 64 * h, bot = vy0 - 64 * h;
  // The view moved by whole pixels (pan): the rows reused, shifted; only the columns that came
  // into view evaluated (left, right), or none (up, down: those beyond a screen only)
  int nx = 0, ny = 0;
  bool re = lev && (lev == 1 || lev == every) && fabsf(w - lw) <= w * 1e-5f && fabsf(h - lh) <= h * 1e-5f;
  if (re) {
    const float fx = (vx0 - lx0) / pw, fy = (vy0 - ly0) * (GH - 1) / h;
    nx = clampi(fx); ny = clampi(fy);
    re = fabsf(fx - nx) < 0.02f && fabsf(fy - ny) < 0.02f && nx > -UI_W / 2 && nx < UI_W / 2 && ny > -GH && ny < GH;
    accy = re ? accy + ny : 0;
    if (accy > 32 * GH || accy < -32 * GH) re = false, accy = 0; // (a point beyond 64 screens could be nearer now)
  }
  else accy = 0;
  const bool last = !re || (nx && lev != 1); // the last column: sampled always, not where its old one went
  quick_ = h > 0.05f; // (zoomed in further: 2e-5 shows)
  lx0 = vx0; ly0 = vy0; lw = w; lh = h; lev = every;
  const int n0 = re && nx < 0 ? -nx : 0, n1 = re && nx > 0 ? UI_W - nx : UI_W; // reused: n0 to n1
  shx = re ? nx : -1000; shy = ny;
  for (int i = 0; i < NF; ++i) {
    int * y = FR->y + i * UI_W;
    unsigned char * j = FR->j + i * UI_W;
    if (re && nx > 0) memmove(y, y + nx, (UI_W - nx) * sizeof(int));
    if (re && nx < 0) memmove(y - nx, y, (UI_W + nx) * sizeof(int));
    int pc = -1;
    float x = vx0;
    for (int c = 0, n = 0; c < UI_W; ++c, --n) { // (n: columns to the next sample; % is a helper call)
      j[c] = 0;
      if (n > 0 && c != UI_W - 1) { y[c] = NONE; continue; }
      n = every;
      int r = y[c];
      bool ev = !re || c < n0 || c >= n1 || (c == UI_W - 1 && last);
      if (!ev && ny && r != NONE && r != TOPC && r != BOTC) r += 16 * ny;
      if (ev) {
        const float v = val(i, c == UI_W - 1 ? vx1 : x);
        r = !fin(v) ? NONE : v > top ? TOPC : v < bot ? BOTC : (int)(A - v * k);
      }
      x += dx;
      y[c] = r;
      if (r == NONE) { pc = -1; continue; }
      if (pc >= 0) j[c] = joined(i, pc, c, y[pc], r);
      pc = c;
    }
  }
  quick_ = false;
}

// ---------------------------------------------------------------- the key points
struct kpt { float x, y; unsigned char kinds, f, lw; char * label; char lt[24]; }; // kinds: 1 << kind; label: math (giac syntax); lt: in decimals, lw wide
static kpt * K_;
static int NK = -1; // -1: not computed yet
static const char * const KIND[] = {"Zero", "y-intercept", "Local max", "Local min", "Inflection", "Crossing"};
// a point's name: its main kind (x^2's (0, 0): local min, sin's zeros: zero, not inflection)
static NOINL const char * kname(int m) {
  static const unsigned char order[6] = {2, 3, 0, 4, 5, 1};
  for (int k = 0; k < 6; ++k) if (m >> order[k] & 1) return KIND[order[k]];
  return "";
}
static NOINL void kpt_add(int kind, int f, const gen & x, const gen & y) {
  if (NK >= 40) return;
  const float xf = fl(x);
  if (!fin(xf)) return;
  for (int j = 0; j < NK; ++j) // one point: its kinds (x^2's zero, minimum, y-intercept; a period's both ends)
    if (K_[j].f == f && fabsf(K_[j].x - xf) <= 1e-3f * (1 + fabsf(xf))) { K_[j].kinds |= 1 << kind; return; }
  kpt & k = K_[NK];
  k.x = xf;
  k.y = fl(y);
  if (!fin(k.y)) k.y = val(f, k.x);
  k.kinds = (unsigned char)(1 << kind); k.f = (unsigned char)f;
  { // its coordinates in decimals, for the graph: (1.73, 0)
    char u[16];
    strcpy(k.lt, "("); fmtg(u, k.x, 3); strcat(k.lt, u); strcat(k.lt, ", ");
    if (fin(k.y)) fmtg(u, k.y, 3); else strcpy(u, "?");
    strcat(k.lt, u); strcat(k.lt, ")");
    k.lw = (unsigned char)ui_text_width(&ui_tr9, k.lt, -1);
  }
  std::string s = "(";
  s += x.print(contextptr); s += ","; s += y.print(contextptr); s += ")";
  k.label = (char *)malloc(s.size() + 1);
  if (k.label) { strcpy(k.label, s.c_str()); ++NK; }
}
static void kpt_clear() {
  for (int i = 0; i < (NK > 0 ? NK : 0); ++i) free(K_[i].label);
  NK = -1;
}
// The key points of every curve (the analysis', analyze.cc: its period repeated over the view) and
// where two curves cross; a few seconds on the calculator: once, when first needed
static NOINL void kpt_find() {
  if (NK >= 0) return;
  lnk = -2;
  if (!K_) K_ = (kpt *)malloc(40 * sizeof(kpt));
  if (!K_) return;
  NK = 0;
  focus_status_msg("Finding the key points");
  focus_busy(UI_H / 2);
  giac::control_c_hook = focus_busy_tick;
  dconsole_mode = 0;
  for (int i = 0; i < NF && !giac::ctrl_c && !giac::interrupted; ++i) {
    std::vector<an_pt> pts;
    gen per;
    analyze_points(F_[i], pts, per, contextptr);
    const float pf = is_zero(per) ? 0 : fl(per);
    for (unsigned j = 0; j < pts.size(); ++j) {
      if (pf <= 0 || pts[j].kind == 1) { kpt_add(pts[j].kind, i, pts[j].x, pts[j].y); continue; }
      const gen & x = pts[j].x; // periodic: each one over the view (and around it), as x + k period
      const float x0 = fl(x);
      const int k0 = (int)floorf((vx0 - x0) / pf) - 1, k1 = (int)ceilf((vx1 - x0) / pf) + 1;
      for (int k = k0; k <= k1 && k - k0 < 12; ++k)
        kpt_add(pts[j].kind, i, k ? ratnormal(x + gen(k) * per, contextptr) : x, pts[j].y);
    }
  }
  for (int i = 0; i < NF; ++i) // two curves crossing
    for (int j = i + 1; j < NF && !giac::ctrl_c; ++j) {
      gen s = _solve(makesequence(symbolic(at_equal, makesequence(F_[i], F_[j])), *X_), contextptr);
      if (s.type != _VECT) continue;
      for (unsigned k = 0; k < s._VECTptr->size(); ++k) {
        const gen x = normal((*s._VECTptr)[k], contextptr);
        kpt_add(5, i, x, normal(subst(F_[i], *X_, x, false, contextptr), contextptr));
      }
    }
  for (int i = 1; i < NK; ++i) // left to right
    for (int j = i; j > 0 && K_[j].x < K_[j - 1].x; --j) { const kpt t = K_[j]; K_[j] = K_[j - 1]; K_[j - 1] = t; }
  giac::control_c_hook = 0;
  dconsole_mode = 1;
  focus_busy(0);
  giac::ctrl_c = giac::interrupted = false;
  focus_status_msg(0);
}

// ---------------------------------------------------------------- drawing
static NOINL void hline(int y, int c) { if (y >= GT && y < GB) ui_fill(0, y, UI_W, 1, ui_col(0, c)); }
static NOINL void vline(int x, int c) { if (x >= 0 && x < UI_W) ui_fill(x, GT, 1, GH, ui_col(0, c)); }
// the dots (radius 2, 4) and the key points' rings (3, the background inside) as ui_rrect and
// ui_rframe draw them, their pixels worked out once (tools: the same formula): 0 untouched, 1-3 the
// color's shade, 4 the background; the corners' arithmetic took 15% of a frame with 40 key points
static const unsigned char DOT2[25] = {2,3,3,3,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,2,3,3,3,2};
static const unsigned char RING3[49] = {0,2,3,3,3,2,0,2,1,4,4,4,1,2,3,4,4,4,4,4,3,3,4,4,4,4,4,3,3,4,4,4,4,4,3,2,1,4,4,4,1,2,0,2,3,3,3,2,0};
static const unsigned char DOT4[81] = {0,1,3,3,3,3,3,1,0,1,3,3,3,3,3,3,3,1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,3,3,3,3,3,3,3,1,0,1,3,3,3,3,3,1,0};
static NOINL void disc(int X, int Y, int r, int c, bool fill) {
  if (X < -r || X >= UI_W + r || Y < ui_cy0 - r || Y >= ui_cy1 + r) return;
  const unsigned char * m = r == 2 ? DOT2 : r == 3 ? RING3 : DOT4, * ramp = ui_ramp(0, c, UC_BG), bg = ui_col(0, UC_BG);
  (void)fill;
  const int n = 2 * r + 1;
  unsigned char * row = ui_fb + (Y - r) * UI_W;
  for (int j = 0, y = Y - r; j < n; ++j, ++y, row += UI_W) {
    if (y < ui_cy0 || y >= ui_cy1) { m += n; continue; }
    for (int i = 0, x = X - r; i < n; ++i, ++x, ++m)
      if (*m && x >= ui_cx0 && x < ui_cx1) row[x] = *m == 4 ? bg : ramp[*m];
  }
}
// a segment (1/16 px) cut to the rows lo..hi (1/16 px): a curve going off the screen, its visible part
static NOINL void seg16(int x0, int y0, int x1, int y1, int lo, int hi, int th, const unsigned char * r) {
  if (y0 > y1) { int t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  if (y1 < lo || y0 > hi) return;
  if (y0 < lo) { x0 += (int)((long)(x1 - x0) * (lo - y0) / (y1 - y0)); y0 = lo; }
  if (y1 > hi) { x1 = x0 + (int)((long)(x1 - x0) * (hi - y0) / (y1 - y0)); y1 = hi; }
  ui_seg16(x0, y0, x1, y1, th, r);
}
// a curve's segment while a key is held: a run of pixels per column, 2 at least (no anti-aliasing:
// 15 thousand cycles a segment less); x in pixels, y in 1/16 px, the band's rows lo..hi
static NOINL void runs(int x0, int y0, int x1, int y1, int lo, int hi, unsigned char col) {
  const int n = x1 - x0, d = n > 1 ? (y1 - y0) / n : y1 - y0;
  for (int k = 0, y = y0; k < n; ++k, y += d) {
    int a = y >> 4, z = (k == n - 1 ? y1 : y + d) >> 4;
    if (a > z) { const int t = a; a = z; z = t; }
    if (z - a < 1) z = a + 1;
    if (a < lo) a = lo;
    if (z >= hi) z = hi - 1;
    if (z >= a) ui_fill(x0 + k, a, 1, z - a + 1, col);
  }
}
// a number on the grid: on the background (not crossed by its line), if in the band b..e
static NOINL int text_at(const char * s, int w, int x, int base, int c, int b, int e) {
  if (base + 3 < b || base - 8 >= e) return w;
  ui_fill(x - 1, base - 8, w + 2, 11, ui_col(0, UC_BG));
  ui_draw_text(&ui_tr9, s, -1, x, base, ui_ramp(0, c, UC_BG), 0);
  return w;
}
static bool tang_on() { return tang && mode; }
static bool shown_note() { return tang_on() || (note[0] && (note_x < 0 || note_x == px_of(tx))); }
static char * tang_note();
static float slope(int i, float x);
static NOINL void tangent_text(char * t, float m, float b);
// the readout card: x and y of the trace point (the middle: pan), the note (a key point's exact
// values, the tangent, the area) under them
static void readout(int b, int e) {
  const bool n = shown_note();
  const int w = 200, x0 = 4;
  // the note's line: its words, then its math (a fraction: the card taller)
  const char * nt = n ? (tang_on() ? tang_note() : note) : 0, * m = n ? strchr(nt, '\x01') : 0;
  const int tw = n ? ui_text_width(&ui_tb10, nt, m ? (int)(m - nt) : -1) : 0;
  mi_layout L;
  int lv = 0, asc = 10, desc = 2;
  if (m) {
    lv = ui_math_fit(m + 1, strlen(m + 1), -1, 6, w - tw - 22, 40, 0, L);
    if (L.asc > asc) asc = L.asc;
    if (L.desc > desc) desc = L.desc;
  }
  const int h = n ? 20 + asc + desc + 3 : 20, yy = GB - h - 4, base = yy + 18 + asc;
  if (yy + h <= b || yy >= e) return;
  ui_rrect(0, x0, yy, w, h, 6, UC_CARD, UC_BG);
  ui_rframe(0, x0, yy, w, h, 6, UC_LINE, UC_CARD);
  if (mode) ui_rrect(0, x0 + 6, yy + 7, 6, 6, 3, COLS[tf], UC_CARD);
  ui_draw_text(&ui_tb10, FR->ln, -1, x0 + (mode ? 16 : 8), yy + 14, ui_ramp(0, UC_INK, UC_CARD), 0);
  if (n) {
    ui_draw_text(&ui_tb10, nt, m ? (int)(m - nt) : -1, x0 + 8, base, ui_ramp(0, UC_ACC, UC_CARD), 0);
    if (m) ui_math_draw(L, m + 1, lv, x0 + 12 + tw, base, 0, UC_ACC, UC_CARD, UC_ACC);
  }
}
static char tnote[48];
static char * tang_note() { return tnote; }
// the grid's lines from lo to hi at step st (k st, k an integer), their numbers
static NOINL int lines_of(gl * g, float lo, float hi, float st, bool x) {
  int n = 0;
  for (long k = (long)ceilf(lo / st), k1 = (long)floorf(hi / st); k <= k1 && n < NG; ++k) {
    const float v = k * st;
    g[n].v = v;
    g[n].p = x ? px_of(v) : py_of(v);
    g[n].t[0] = 0;
    if (k) fmt(g[n].t, v, st);
    g[n].w = (unsigned char)ui_text_width(&ui_tr9, g[n].t, -1);
    ++n;
  }
  return n;
}
extern "C" { volatile unsigned graph_frames; } // frames drawn (for tools/emu: frames a second)
static NOINL void paint(bool fast) {
  frame & F = *FR;
  ++graph_frames;
  sample(fast ? 3 : 1);
  const float sx = step_of(vx1 - vx0, UI_W, 44), sy = step_of(vy1 - vy0, GH, 32);
  const int ax = px_of(0), ay = py_of(0);
  F.ngx = lines_of(F.gx, vx0, vx1, sx, true);
  F.ngy = lines_of(F.gy, vy0, vy1, sy, false);
  F.nd = 0; // zoomed in: the curves' points at the grid's x, their coordinates
  if (coords || vx1 - vx0 <= w0 / 2)
    for (int i = 0; i < NF; ++i) {
      int le = -99; // (where the last coordinates end)
      for (int k = 0; k < F.ngx && F.nd < ND; ++k) {
        const float xg = F.gx[k].v, y = val(i, xg);
        if (!fin(y)) continue;
        dt & d = F.d[F.nd];
        d.x = F.gx[k].p; d.y = py_of(y); d.f = (unsigned char)i; d.t[0] = 0; d.w = 0;
        if (d.y < GT - 3 || d.y > GB + 3) continue;
        if (coords && d.x + 4 > le + 4) {
          char u[16];
          strcpy(d.t, "("); fmt(u, xg, sx); trim(u); strcat(d.t, u); strcat(d.t, ", ");
          fmtg(u, y, 4); strcat(d.t, u); strcat(d.t, ")");
          d.w = (unsigned char)ui_text_width(&ui_tr9, d.t, -1);
          le = d.x + 4 + d.w;
        }
        ++F.nd;
      }
    }
  for (int k = 0; k < NK && k < NKP; ++k)
    if (shx != -1000 && lnk == NK) { F.kx[k] -= shx; if (F.ky[k] != NONE) F.ky[k] += shy; }
    else { F.kx[k] = px_of(K_[k].x); F.ky[k] = fin(K_[k].y) ? py_of(K_[k].y) : NONE; }
  lnk = NK;
  for (int k = 0; k < NK && k < NKP; ++k) { // the coordinates: off the curve (a maximum's above it, a
    // minimum's below, the others up and right), not over one already placed
    F.lx[k] = -1;
    const int X = F.kx[k], Y = F.ky[k], m = K_[k].kinds, w = K_[k].lw, y = m & 4 ? Y - 7 : m & 8 ? Y + 14 : Y - 5;
    int x = m & 12 ? X - w / 2 : X + 5;
    if (x + w > UI_W - 2) x = X - 5 - w; // (at the right edge: left of it)
    if (!klabels || Y == NONE || x < 2 || x + w > UI_W - 2 || y < GT + 10 || y > GB - 4) continue;
    bool free_ = true;
    for (int j = 0; j < k && free_; ++j)
      if (F.lx[j] >= 0 && x < F.lx[j] + K_[j].lw + 3 && F.lx[j] < x + w + 3 && y > F.ly[j] - 11 && y < F.ly[j] + 11) free_ = false;
    if (free_) { F.lx[k] = (short)x; F.ly[k] = (short)y; }
  }
  int tl[4]; // the tangent, cut to the plot (1/16 px)
  bool tg = tang && mode && fin(val(tf, tx));
  if (tg) { // (its equation in the readout, for the trace point)
    const float y = val(tf, tx), m = slope(tf, tx);
    tangent_text(tnote, m, y - m * tx);
    float xa = 0, xb = UI_W - 1, ra = row_of(y + m * (vx0 - tx)), rb = row_of(y + m * (vx1 - tx));
    if (ra > rb) { float t = ra; ra = rb; rb = t; t = xa; xa = xb; xb = t; }
    tg = fin(m) && rb >= GT - 2 && ra <= GB + 2;
    if (tg) {
      if (ra < GT - 2) { xa += (xb - xa) * (GT - 2 - ra) / (rb - ra); ra = GT - 2; }
      if (rb > GB + 2) { xb = xa + (xb - xa) * (GB + 2 - ra) / (rb - ra); rb = GB + 2; }
      tl[0] = (int)(xa * 16); tl[1] = (int)(ra * 16); tl[2] = (int)(xb * 16); tl[3] = (int)(rb * 16);
    }
  }
  const float ty = mode ? val(tf, tx) : 0;
  const int tpx = mode ? px_of(tx) : 0, tpy = mode && fin(ty) ? py_of(ty) : NONE;
  { // the readout's line
    char b1[24], b2[24];
    const float x = mode ? tx : (vx0 + vx1) / 2, y = mode ? ty : (vy0 + vy1) / 2;
    fmt(b1, x, sx / 100); fmt(b2, y, sy / 100);
    strcpy(F.ln, "x = "); strcat(F.ln, b1); strcat(F.ln, "   y = "); strcat(F.ln, fin(y) ? b2 : "none");
  }
  const int pa = area_a < area_b ? (px_of(area_a) < 0 ? 0 : px_of(area_a)) : 1, pb = area_a < area_b ? (px_of(area_b) >= UI_W ? UI_W - 1 : px_of(area_b)) : 0;
  const int lb = ay + 11 < GB - 2 ? (ay < GT ? GT + 10 : ay + 11) : GB - 3; // the x numbers' baseline
  const int rows = ui_band_open(64), r0 = rows ? (GH % rows ? GH % rows : rows) : GH; // (the bands end at the bottom: the card in one)
  for (int b = GT, e = GT + r0; b < GB; b = e, e += rows) {
    const int lo = (b - 2) * 16, hi = (e + 2) * 16;
    if (rows) ui_band_begin(b, e);
    ui_clip(0, b, UI_W, e);
    ui_fill(0, b, UI_W, e - b, ui_col(0, UC_BG));
    if (grid) { // the grid, the axes over it
      for (int k = 0; k < F.ngx; ++k) vline(F.gx[k].p, UC_LINE);
      for (int k = 0; k < F.ngy; ++k) hline(F.gy[k].p, UC_LINE);
    }
    for (int c = pa; c <= pb; ++c) { // the area under the curve: shaded from the x-axis
      int r = F.y[tf * UI_W + c];
      for (int k = 1; r == NONE && k < 3 && c >= k; ++k) r = F.y[tf * UI_W + c - k]; // (fast: every 3 columns)
      if (r == NONE) continue;
      int r0 = r / 16, r1 = ay;
      if (r0 > r1) { const int t = r0; r0 = r1; r1 = t; }
      if (r0 < b) r0 = b;
      if (r1 > e) r1 = e;
      if (r1 > r0) ui_fill(c, r0, 1, r1 - r0, ui_col(0, UC_ACCSOFT));
    }
    vline(ax, UC_SUB); hline(ay, UC_SUB);
    for (int k = 0; k < F.ngx; ++k) { // the numbers: under the x-axis (or at the bottom), left of the y-axis
      const gl & g = F.gx[k];
      const int x = g.p - g.w / 2;
      if (g.t[0] && x > 1 && x + g.w < UI_W - 1) text_at(g.t, g.w, x, lb, UC_SUB, b, e);
    }
    for (int k = 0; k < F.ngy; ++k) {
      const gl & g = F.gy[k];
      // left of the y-axis (right of it near the left edge); the axis off the screen: at that edge
      const int x = ax < 0 ? 3 : ax >= UI_W ? UI_W - 3 - g.w : ax - 4 - g.w >= 2 ? ax - 4 - g.w : ax + 4;
      if (g.t[0] && g.p + 4 > GT + 9 && g.p + 4 < GB - 2) text_at(g.t, g.w, x, g.p + 4, UC_SUB, b, e);
    }
    for (int i = 0; i < NF; ++i) { // the curves
      const int * yr = F.y + i * UI_W;
      const unsigned char * jn = F.j + i * UI_W, * r = ui_ramp(0, COLS[i], UC_BG);
      int pc = -1;
      for (int c = 0; c < UI_W; c += !fast || c == UI_W - 1 ? 1 : c + 3 < UI_W ? 3 : UI_W - 1 - c) { // (the columns sampled)
        const int z = yr[c];
        if (z == NONE) { pc = -1; continue; }
        if (pc >= 0 && jn[c]) {
          const int a = yr[pc];
          if (!((a < lo && z < lo) || (a > hi && z > hi))) {
            if (fast) runs(pc, a, c, z, b, e, r[3]);
            else seg16(pc * 16 + 8, a, c * 16 + 8, z, lo, hi, 26, r);
          }
        }
        pc = c;
      }
    }
    for (int k = 0; k < F.nd; ++k) { // the dots, their coordinates
      const dt & d = F.d[k];
      if (d.y + 3 >= b && d.y - 3 < e) disc(d.x, d.y, 2, COLS[d.f], true);
      if (d.t[0] && d.y - 5 > GT + 9 && d.y - 5 < GB - 4) text_at(d.t, d.w, d.x + 4, d.y - 5, COLS[d.f], b, e);
    }
    for (int k = 0; k < NK && k < NKP; ++k) { // the key points, their coordinates
      if (F.ky[k] == NONE) continue;
      if (F.ky[k] + 4 >= b && F.ky[k] - 4 < e) disc(F.kx[k], F.ky[k], 3, COLS[K_[k].f], false);
      if (F.lx[k] >= 0) text_at(K_[k].lt, K_[k].lw, F.lx[k], F.ly[k], COLS[K_[k].f], b, e);
    }
    if (tg) seg16(tl[0], tl[1], tl[2], tl[3], lo, hi, 18, ui_ramp(0, UC_SUB, UC_BG)); // the tangent
    if (mode) { // the trace point: a filled dot, faint lines through it
      vline(tpx, UC_ACCSOFT);
      if (tpy != NONE) { hline(tpy, UC_ACCSOFT); disc(tpx, tpy, 4, COLS[tf], true); }
    } else { // pan: a small cross in the middle
      const int cx = UI_W / 2, cy = GT + GH / 2;
      ui_fill(cx - 5, cy, 11, 1, ui_col(0, UC_INK)); ui_fill(cx, cy - 5, 1, 11, ui_col(0, UC_INK));
    }
    readout(b, e);
    ui_noclip();
    if (rows) ui_band_end();
  }
  ui_band_close();
}
static NOINL void bar() {
  static const char * const lab[2][5] = {{"pan", "zoom", "points", "table", "more"}, {"trace", "zoom", "points", "table", "more"}};
  focus_bar_tabs(lab[mode], 0);
  focus_status_label(mode ? "GRAPH  TRACE" : "GRAPH  PAN");
}

// ---------------------------------------------------------------- moving
// the keypad's rows as the hardware sees them now (held keys): 6: + -, 7: the arrows
// (the OS leaves the keypad in its "any key" mode, where they are not valid: scanned once first,
// as keypadc's kb_Scan, then the OS's mode again)
static NOINL void kb_scan() {
  volatile unsigned char * const m = (volatile unsigned char *)0xF50000;
  __asm__ volatile("di");
  const unsigned char m0 = *m & 3;
  *m = 2;
  while (*m & 3) ;
  *m = m0;
  __asm__ volatile("ei");
}
static inline unsigned kb_row(int g) { return ((volatile unsigned short *)0xF50010)[g]; }
enum { KB_DOWN = 1, KB_LEFT = 2, KB_RIGHT = 4, KB_UP = 8, KB_ADD = 2, KB_SUB = 4 };
static NOINL void zoom_at(float cx, float cy, float f) {
  const float w = (vx1 - vx0) * f, m = fabsf(cx) > fabsf(cy) ? fabsf(cx) : fabsf(cy);
  if (f < 1 && w < 2e-4f * (1 + m)) return; // (floats: 7 digits; a pixel no finer)
  if (f > 1 && w > 1e7f) return;
  vx0 = cx + (vx0 - cx) * f; vx1 = cx + (vx1 - cx) * f;
  vy0 = cy + (vy0 - cy) * f; vy1 = cy + (vy1 - cy) * f;
}
static NOINL void follow() { // the trace point on the screen
  const float w = vx1 - vx0;
  if (tx < vx0 + w / 20) { vx0 = tx - w / 2; vx1 = vx0 + w; }
  if (tx > vx1 - w / 20) { vx0 = tx - w / 2; vx1 = vx0 + w; }
  const float y = val(tf, tx), h = vy1 - vy0;
  if (fin(y) && (y < vy0 + h / 20 || y > vy1 - h / 20)) { vy0 = y - h / 2; vy1 = vy0 + h; }
}
// a key that moves (arrows; + -): moved while it is held, a frame at a time, faster the longer
static void hold() {
  int fr = 0;
  for (;; ++fr) {
    kb_scan();
    const unsigned a = kb_row(7), z = kb_row(6);
    if (!(a & 15) && !(z & (KB_ADD | KB_SUB))) break;
    const float sp = fr < 4 ? 1 : fr < 12 ? 2 : 3, w = vx1 - vx0, h = vy1 - vy0;
    if (z & KB_ADD) zoom_at(mode ? tx : (vx0 + vx1) / 2, mode ? val(tf, tx) : (vy0 + vy1) / 2, 0.88f);
    if (z & KB_SUB) zoom_at(mode ? tx : (vx0 + vx1) / 2, mode ? val(tf, tx) : (vy0 + vy1) / 2, 1 / 0.88f);
    if (vy0 != vy0 || vy1 - vy0 <= 0) { vy0 = -10; vy1 = 10; }
    if (!mode) {
      const float dx = 9 * sp * w / (UI_W - 1), dy = 8 * sp * h / (GH - 1); // (whole pixels: the rows reused; 9: every 3)
      if (a & KB_LEFT) { vx0 -= dx; vx1 -= dx; }
      if (a & KB_RIGHT) { vx0 += dx; vx1 += dx; }
      if (a & KB_UP) { vy0 += dy; vy1 += dy; }
      if (a & KB_DOWN) { vy0 -= dy; vy1 -= dy; }
    } else if (a & (KB_LEFT | KB_RIGHT)) {
      tx += (a & KB_RIGHT ? 1 : -1) * w / (UI_W - 1) * (fr < 6 ? 1 : fr < 16 ? 3 : 6);
      follow();
    }
    paint(true);
    if (fr == 0) os_wait_1ms(120); // (a tap moves once)
  }
  key_discard();
  while (key_waiting()) key_discard(); // (the presses the OS saw meanwhile)
  paint(false);
}

// ---------------------------------------------------------------- the menus
static NOINL void fit_y() { // the y range of the curves over the view
  float lo = 1e30f, hi = -1e30f;
  for (int i = 0; i < NF; ++i)
    for (int c = 0; c < UI_W; c += 2) {
      const float y = val(i, col_x(c));
      if (fin(y)) { if (y < lo) lo = y; if (y > hi) hi = y; }
    }
  if (lo > hi) { lo = -10; hi = 10; }
  if (hi - lo < 1e-6f) { lo -= 1; hi += 1; }
  const float m = (hi - lo) / 10;
  vy0 = lo - m; vy1 = hi + m;
}
static NOINL bool ask(const char * t, const char * sub, float & v, gen * exact = 0) {
  std::string s;
  if (focus_prompt(t, sub, s, false) != KEY_CTRL_EXE || s.empty()) return false; // (pi/3, sqrt(2), 1/2)
  const gen e(s, contextptr);
  if (exact) *exact = eval(e, 1, contextptr);
  v = fl(e);
  return fin(v);
}
static void bar();
static void redraw() { bar(); paint(false); } // (a menu closed: gone before a prompt)
static void zoom_menu() {
  static const char * const lab[] = {"Zoom in", "Zoom out", "Fit the curve", "Standard", "Trig", "Square", "Window..."};
  static const char * const hint[] = {"+", "-", 0, "-10 to 10", "x: -2pi to 2pi", "equal units", 0};
  const int c = focus_card_list(1, "zoom", 0, lab, hint, 7, 0);
  const float cx = mode ? tx : (vx0 + vx1) / 2, cy = mode ? val(tf, tx) : (vy0 + vy1) / 2;
  switch (c) {
  case 0: zoom_at(cx, cy, 0.5f); break;
  case 1: zoom_at(cx, cy, 2); break;
  case 2: fit_y(); break;
  case 3: vx0 = vy0 = -10; vx1 = vy1 = 10; break;
  case 4: vx0 = -6.2832f; vx1 = 6.2832f; vy0 = -4; vy1 = 4; break;
  case 5: { const float m = (vy0 + vy1) / 2, h = (vx1 - vx0) * GH / UI_W; vy0 = m - h / 2; vy1 = m + h / 2; } break;
  case 6: {
    redraw();
    float a = vx0, b = vx1, p = vy0, q = vy1;
    if (ask("Window", "x min", a) && ask("Window", "x max", b) && ask("Window", "y min", p) && ask("Window", "y max", q) && a < b && p < q) { vx0 = a; vx1 = b; vy0 = p; vy1 = q; }
  } break;
  }
}
// giac at work: "Calculating" after a moment, ON stops it, its notes nowhere
static NOINL void working(bool on) {
  if (on) { focus_busy(UI_H / 2); giac::control_c_hook = focus_busy_tick; dconsole_mode = 0; return; }
  dconsole_mode = 1;
  giac::control_c_hook = 0;
  focus_busy(0);
  giac::ctrl_c = giac::interrupted = false;
}
// (x,y), or x=x without y, in giac syntax into b (n bytes); else in decimals
static NOINL void gtext(char * b, int n, const gen & x, const gen * y) {
  std::string m = y ? "(" : "x=";
  m += x.print(contextptr);
  if (y) { m += ","; m += y->print(contextptr); m += ")"; }
  if ((int)m.size() < n) { strcpy(b, m.c_str()); return; }
  char u[16];
  strcpy(b, y ? "(" : "x="); fmtg(u, fl(x), 4); strcat(b, u);
  if (y) { strcat(b, ","); fmtg(u, fl(*y), 4); strcat(b, u); strcat(b, ")"); }
}
// f_i at x, exact (sin(pi/3): sqrt(3)/2), its value v; false if not a real number there
static NOINL bool exact_at(int i, const gen & x, gen & y, float & v) {
  y = normal(eval(subst(F_[i], *X_, x, false, contextptr), 1, contextptr), contextptr);
  v = fl(y);
  return fin(v);
}
// the trace point on curve i at x (exact x, y); the readout's note: words, then (x, y)
static NOINL void point_at(int i, const gen & x, const gen & y, const char * words) {
  mode = 1; tf = i; tx = fl(x);
  follow();
  strcpy(note, words); strcat(note, "\x01");
  gtext(note + strlen(note), sizeof(note) - strlen(note), x, &y);
  note_x = px_of(tx);
}
// Value at x: an x typed (exact: pi/3), the point there (also enter in trace mode)
static NOINL void value_at() {
  float x, y;
  gen gx, gy;
  if (!ask("Value at x =", "x", x, &gx)) return;
  if (!exact_at(tf, gx, gy, y)) { focus_status_msg("Not defined there"); return; }
  point_at(tf, gx, gy, "Point");
}
// Solve f(x) = c: every x (sin, cos, tan: in the view and a screen each side), exact when giac
// finds it; several: a list to pick from
static NOINL void solve_for() {
  float c;
  gen gc;
  if (!ask("Solve  f(x) =", "y", c, &gc)) return;
  const std::string fs = F_[tf].print(contextptr);
  const bool t = fs.find("sin") != std::string::npos || fs.find("cos") != std::string::npos || fs.find("tan") != std::string::npos;
  const float w = vx1 - vx0;
  vecteur r;
  working(true);
  analyze_zeros(F_[tf] - gc, *X_, t ? vx0 - w : 0, t ? vx1 + w : 0, false, r, contextptr);
  working(false);
  const int n = r.size() < 24 ? r.size() : 24;
  paint(false); // (the prompt gone)
  if (!n) { focus_status_msg(t ? "No x near the view" : "No x gives that y"); return; }
  int k = 0;
  char * pool = n > 1 ? (char *)malloc(n * 48) : 0;
  if (pool) { // which one: x exact, in decimals
    const char * mth[24], * lab[24];
    const float mid = mode ? tx : (vx0 + vx1) / 2;
    int sel = 0;
    for (int j = 0; j < n; ++j) {
      const float v = fl(r[j]);
      gtext(pool + 48 * j, 32, r[j], 0);
      fmtg(pool + 48 * j + 32, v, 4);
      mth[j] = pool + 48 * j; lab[j] = pool + 48 * j + 32;
      if (fabsf(v - mid) < fabsf(fl(r[sel]) - mid)) sel = j; // (opens on the one nearest the middle)
    }
    k = focus_card_list(2, "points", mth, lab, 0, n, sel);
    free(pool);
    if (k < 0) return;
  }
  point_at(tf, r[k], gc, "Solution");
}
// Max and min on [a, b]: f at a, at b and where f' is 0 or undefined between: the largest, the
// smallest (exact); the two to go to
static NOINL void extrema_on() {
  float a, b;
  gen ga, gb;
  if (!ask("Max and min from x =", "a", a, &ga) || !ask("Max and min to x =", "b", b, &gb) || a == b) return;
  if (a > b) { const float t = a; a = b; b = t; }
  vecteur r;
  working(true);
  analyze_zeros(eval(derive(F_[tf], *X_, contextptr), 1, contextptr), *X_, a, b, true, r, contextptr);
  r.push_back(ga); r.push_back(gb);
  gen bx[2], by[2];
  float bv[2] = {0, 0};
  bool any = false;
  const float m = 1e-4f * (1 + fabsf(a) + fabsf(b));
  for (unsigned j = 0; j < r.size() && !giac::ctrl_c; ++j) {
    gen y;
    float v;
    const float x = fl(r[j]);
    if (!(x >= a - m && x <= b + m) || !exact_at(tf, r[j], y, v)) continue;
    for (int q = 0; q < 2; ++q)
      if (!any || (q ? v < bv[1] : v > bv[0])) { bx[q] = r[j]; by[q] = y; bv[q] = v; }
    any = true;
  }
  working(false);
  paint(false);
  if (!any) { focus_status_msg("Not defined there"); return; }
  char ms[2][48];
  const char * mth[2] = {ms[0], ms[1]}, * lab[2] = {"Absolute max", "Absolute min"};
  for (int q = 0; q < 2; ++q) gtext(ms[q], 48, bx[q], &by[q]);
  const int k = focus_card_list(2, "points", mth, lab, 0, 2, 0);
  if (k >= 0) point_at(tf, bx[k], by[k], lab[k]);
}
static NOINL void jump_to(int k) { // the trace cursor on key point k
  mode = 1; tf = K_[k].f; tx = K_[k].x;
  follow();
  strcpy(note, kname(K_[k].kinds)); strcat(note, "\x01");
  strncat(note, K_[k].label, sizeof(note) - strlen(note) - 1);
  note_x = px_of(tx);
}
// F3: the tools on top, then the key points (zeros, extrema, inflection points, crossings)
static void points_menu() {
  if (NK < 0) { kpt_find(); paint(false); } // (the busy dots gone)
  enum { NT = 4 };
  const char * lab[NT + 40], * mth[NT + 40], * hint[NT + 40];
  static const char * const tools[NT] = {"Value at x", "Solve  f(x) = ...", "Max and min on [a, b]", "Labels on the graph"};
  const int n = NT + (NK > 0 ? NK : 0);
  for (int k = 0; k < n; ++k) {
    lab[k] = k < NT ? tools[k] : kname(K_[k - NT].kinds);
    mth[k] = k < NT ? 0 : K_[k - NT].label;
    hint[k] = 0;
  }
  hint[3] = klabels ? "on" : "off";
  const int c = focus_card_list(2, "points", mth, lab, hint, n, 0);
  if (c >= NT) { jump_to(c - NT); return; }
  if (c < 0 || c == 3) { if (c == 3) klabels = !klabels; return; }
  redraw();
  if (c == 0) value_at();
  if (c == 1) solve_for();
  if (c == 2) extrema_on();
}
static void next_point(int dir) { // 2nd left/right: the key point before/after the trace point
  if (NK < 0) { kpt_find(); paint(false); }
  int best = -1;
  for (int k = 0; k < NK; ++k) {
    if (K_[k].f != tf && mode) continue;
    const float d = (K_[k].x - tx) * dir;
    if (d > (vx1 - vx0) / 1000 && (best < 0 || d < (K_[best].x - tx) * dir)) best = k;
  }
  if (best >= 0) jump_to(best);
}
static void table() { // x around the trace point (the middle) by steps of 1, 2 or 5 x 10^k; up/down: more
  const float st = step_of(vx1 - vx0, UI_W, 14);
  float x0 = floorf((mode ? tx : (vx0 + vx1) / 2) / st + 0.5f) * st - 4 * st;
  static char cells[9 * 5][16];
  const char * c[9 * 5];
  static const char * const head[5] = {"x", "y1", "y2", "y3", "y4"};
  const int nc = NF + 1 > 5 ? 5 : NF + 1;
  for (;;) {
    for (int r = 0; r < 9; ++r)
      for (int k = 0; k < nc; ++k) {
        const float x = x0 + r * st, y = k ? val(k - 1, x) : x;
        char * t = cells[r * nc + k];
        if (!k) { fmt(t, x, st); trim(t); }
        else if (fin(y)) fmtg(t, y);
        else strcpy(t, "undefined");
        c[r * nc + k] = t;
      }
    focus_table(head, nc, c, 9, "up/down: more   other keys: back");
    int key;
    GetKey(&key);
    if (key == KEY_CTRL_UP) x0 -= 4 * st;
    else if (key == KEY_CTRL_DOWN) x0 += 4 * st;
    else break;
  }
}
// The slope of curve i at x: its derivative compiled (exact: x^3-3x at 1 is 0, a difference
// quotient gave 0.002), else a difference quotient
static prog * DP_;
static gen * DF_;
static NOINL float slope(int i, float x) {
  if (!DP_) { DP_ = (prog *)calloc(NMAX, sizeof(prog)); DF_ = new gen[NMAX]; }
  if (DP_ && !DP_[i].depth && !DP_[i].ok) {
    DF_[i] = ratnormal(eval(derive(F_[i], *X_, contextptr), 1, contextptr), contextptr);
    DP_[i].depth = 1; // (tried)
    DP_[i].ok = comp(DP_[i], DF_[i], 0);
  }
  if (DP_ && DP_[i].ok) return run_prog(DP_[i], DF_[i], x);
  const float h = (vx1 - vx0) / 4000;
  return (val(i, x + h) - val(i, x - h)) / (2 * h);
}
// the tangent's equation: y = m x + b, its terms when not 0
static NOINL void tangent_text(char * t, float m, float b) {
  char u[20];
  strcpy(t, "Tangent  y = ");
  const bool mz = fabsf(m) < 1e-5f, bz = fabsf(b) < 1e-5f * (1 + fabsf(m));
  if (!mz) {
    if (fabsf(m - 1) < 1e-6f) strcat(t, "x");
    else if (fabsf(m + 1) < 1e-6f) strcat(t, "-x");
    else { fmtg(u, m); strcat(t, u); strcat(t, "x"); }
  }
  if (!bz || mz) {
    fmtg(u, mz ? b : fabsf(b));
    if (!mz) strcat(t, b < 0 ? " - " : " + ");
    strcat(t, u);
  }
}
static NOINL void more_menu(int & nfun) {
  static const char * const lab[] = {"Tangent line here", "Area under the curve", "Graph f' too", "Analyze f(x)", "Point coordinates", "Grid"};
  const char * hint[6] = {tang ? "on" : "off", 0, 0, "the report", coords ? "on" : "off", grid ? "on" : "off"};
  const int c = focus_card_list(4, "more", 0, lab, hint, 6, 0);
  if (c == 0) {
    tang = !tang; // (drawn at the trace point, its equation in the readout: paint)
    if (tang && !mode) { mode = 1; tx = (vx0 + vx1) / 2; }
  }
  if (c == 1) { // from a to b: shaded, its value (giac: exact when it can)
    redraw();
    float a, b;
    gen ga, gb;
    if (ask("Area from x =", "a", a, &ga) && ask("Area to x =", "b", b, &gb) && a != b) {
      if (a > b) { float t = a; a = b; b = t; gen u = ga; ga = gb; gb = u; }
      area_a = a; area_b = b;
      // the integral from a to b (signed: below the x-axis counts less), exact when giac finds it
      working(true);
      gen I = _integrate(makesequence(F_[tf], *X_, ga, gb), contextptr);
      working(false);
      const float v = fl(I);
      std::string ex = I.print(contextptr);
      strcpy(note, "Integral");
      if (!is_undef(I) && I.type != _DOUBLE_ && ex.size() < 30 && ex.find('.') == std::string::npos && ex.find("int") == std::string::npos) {
        strcat(note, " = \x01"); strcat(note, ex.c_str()); // exact: 8/3
      }
      else if (fin(v)) {
        char t[20];
        fmtg(t, v);
        strcat(note, " = "); strcat(note, t);
      }
      else strcat(note, ": not found");
      note_x = -1;
    }
  }
  if (c == 2 && nfun < NMAX) { // f' as another curve
    gen d = eval(derive(F_[tf], *X_, contextptr), 1, contextptr);
    F_[nfun] = ratnormal(d, contextptr);
    memset(&P_[nfun], 0, sizeof(prog));
    P_[nfun].ok = comp(P_[nfun], F_[nfun], 0);
    NF = ++nfun;
    lev = 0;
    kpt_clear();
  }
  if (c == 3) { // the analysis' report (its screen), then the graph again
    std::string s = F_[tf].print(contextptr);
    focus_analyze(s.c_str());
  }
  if (c == 4) coords = !coords;
  if (c == 5) grid = !grid;
}

// ---------------------------------------------------------------- the screen
static NOINL void run(float a, float b) {
  vx0 = a; vx1 = b; vy0 = -10; vy1 = 10;
  w0 = b - a;
  int in = 0, all = 0; // y from -10 to 10, unless the curves are mostly outside: fitted to them
  for (int i = 0; i < NF; ++i)
    for (int c = 0; c < UI_W; c += 4) {
      const float y = val(i, col_x(c));
      if (fin(y)) { ++all; if (y >= -10 && y <= 10) ++in; }
    }
  if (4 * in < all) fit_y();
  if (vy1 - vy0 > 1e4f) { vy0 = -10; vy1 = 10; } // (1/x: not its 1e8 near 0)
  mode = 0; tf = 0; tx = 0; tang = false; area_a = area_b = 0; note[0] = 0;
  focus_view = 1;
  int nfun = NF;
  bar();
  paint(false);
  focus_status_msg("arrows move, + - zoom"); // (until a key)
  for (;;) {
    int key;
    GetKey(&key);
    focus_status_msg(0);
    if (key == KEY_CTRL_EXIT || key == KEY_CTRL_AC || key == KEY_CTRL_QUIT) break;
    if (key == KEY_CTRL_LEFT || key == KEY_CTRL_RIGHT || key == KEY_CTRL_UP || key == KEY_CTRL_DOWN || key == KEY_CHAR_PLUS || key == KEY_CHAR_MINUS) {
      if (mode && (key == KEY_CTRL_UP || key == KEY_CTRL_DOWN)) { // trace: the next curve
        tf = (tf + (key == KEY_CTRL_DOWN ? 1 : NF - 1)) % NF;
        follow(); paint(false);
        continue;
      }
      hold();
      continue;
    }
    if (key == KEY_SHIFT_LEFT || key == KEY_SHIFT_RIGHT || key == KEY_CTRL_PAGEUP || key == KEY_CTRL_PAGEDOWN) {
      next_point(key == KEY_SHIFT_RIGHT || key == KEY_CTRL_PAGEDOWN ? 1 : -1);
      bar(); paint(false);
      continue;
    }
    if (key == KEY_CTRL_F1) { // pan <-> trace
      mode = !mode;
      if (mode) { tx = (vx0 + vx1) / 2; follow(); }
      bar(); paint(false);
      if (mode) focus_status_msg("2nd left right: key points");
      continue;
    }
    if (key == KEY_CTRL_EXE && mode) { // trace: an x to go to, its point exact
      redraw();
      value_at();
      bar(); paint(false);
      continue;
    }
    if (key == KEY_CTRL_F2) zoom_menu();
    else if (key == KEY_CTRL_F3) points_menu();
    else if (key == KEY_CTRL_F4) table();
    else if (key == KEY_CTRL_F5) more_menu(nfun);
    else continue;
    bar(); paint(false);
  }
  focus_view = 0;
  focus_status_label(0);
  focus_invalidate();
}

// The functions of plot(f), plot([f,g]), plot(f,x=a..b) typed (or the Graph actions' input):
// shown here when each is an expression in one variable (the same one), else 0 (KhiCAS's view)
int focus_graph_input(const char * in) {
  if (strncmp(in, "plot(", 5)) return 0;
  gen g(in, contextptr);
  if (!g.is_symb_of_sommet(at_plot)) return 0;
  gen a = g._SYMBptr->feuille, fs = a, rg = 0;
  if (a.type == _VECT && a.subtype == _SEQ__VECT) { // plot(f, x=a..b) or plot(f, x)
    if (a._VECTptr->size() != 2) return 0;
    fs = a[0]; rg = a[1];
  }
  vecteur v = fs.type == _VECT ? *fs._VECTptr : vecteur(1, fs);
  if (v.empty() || v.size() > NMAX) return 0;
  gen x = 0;
  float lo = -10, hi = 10;
  if (rg.is_symb_of_sommet(at_equal) && rg._SYMBptr->feuille[0].type == _IDNT) { // x=a..b
    x = rg._SYMBptr->feuille[0];
    gen r = evalf(rg._SYMBptr->feuille[1]._SYMBptr ? rg._SYMBptr->feuille[1]._SYMBptr->feuille : gen(0), 1, contextptr);
    if (r.type == _VECT && r._VECTptr->size() == 2 && r[0].type == _DOUBLE_ && r[1].type == _DOUBLE_) { lo = (float)r[0]._DOUBLE_val; hi = (float)r[1]._DOUBLE_val; }
  }
  else if (rg.type == _IDNT) x = rg;
  for (unsigned i = 0; i < v.size(); ++i) { // each in one variable, the same
    v[i] = eval(v[i], 1, contextptr);
    if (v[i].type == _VECT || v[i].is_symb_of_sommet(at_equal) || v[i].type == _STRNG) return 0;
    vecteur ids = lidnt(v[i]);
    for (unsigned j = 0; j < ids.size(); ++j) {
      if (evalf(ids[j], 1, contextptr).type == _DOUBLE_) continue;
      if (is_zero(x)) x = ids[j];
      else if (ids[j] != x) return 0;
    }
  }
  if (is_zero(x)) x = gen(identificateur("x"));
  static gen * xs;
  if (!xs) xs = new gen;
  *xs = x; X_ = xs;
  NF = v.size();
  F_ = new gen[NMAX];
  P_ = (prog *)calloc(NMAX, sizeof(prog));
  FR = (frame *)malloc(sizeof(frame));
  if (!P_ || !FR) { delete[] F_; free(P_); free(FR); FR = 0; return 0; }
  for (int i = 0; i < NF; ++i) { F_[i] = v[i]; P_[i].ok = comp(P_[i], F_[i], 0); }
  NK = -1; lev = 0;
  run(lo < hi ? lo : -10, lo < hi ? hi : 10);
  kpt_clear();
  free(K_); K_ = 0;
  delete[] F_; F_ = 0;
  free(P_); P_ = 0;
  free(FR); FR = 0;
  free(DP_); DP_ = 0;
  delete[] DF_; DF_ = 0;
  return 1;
}
#endif
