// focus_tutor.cc - DeltaCE's "Your turn" screen (the checks: tutor.cc). The problem at the top, the
// student's lines under it (a teal edge: right, with its rule; red: wrong, with why; gray: a line a
// hint showed), and at the bottom the line being typed, in 2D as on the console's edit line (the
// same keys: / makes a fraction, ^ an exponent, sin( a call, the arrows follow the drawing).
// enter: check the line   F1: a hint (the rule's name, its formula, then its line)
// F2: the next line   F3: all the steps   F4: another problem (Practice) or start again   F5: back
// WITH_TUTOR (off since 2026-10-30, makefile): else no screen and no steps page chip; tutor.cc stays
#if defined FLOWCE_STEPS && defined WITH_TUTOR
#include <string.h>
#include <stdlib.h>
#include <giac/giacPCH.h> // first: its "#define std ustl"
#include "k_csdk.h"
#include "k_defs.h"
#include "console.h"
#include "focus.h"
#include "ui_gfx.h"
#include "ui_font.h"
#include "ui_fontdata.h"
#include "ui_math.h"
#include "mathedit.h"
#include "tutor.h"

extern giac::context * contextptr;
extern int dconsole_mode; // console.cc: 0, giac's notes go nowhere
extern "C" bool khicas_implicit_mult(char * line, int maxlen); // tiinput_glue.cc: 2xcos(x) -> 2*x*cos(x)
extern "C" void GetKey(int * key);
#define NOINL __attribute__((noinline))

enum { SB = 16, MB = 22, SBOT = UI_H - MB, Y0 = SB, FH = 34, FY = SBOT - FH - 4, TOP = Y0 + 62, CAP = 200 };
enum { E_RIGHT, E_WRONG, E_SHOWN, E_HINT, E_FORMULA, E_NOTE, E_DONE };
struct t_entry { std::string math, msg; char kind; };
static std::vector<t_entry> * E;
static char fld[CAP]; // the line being typed
static int caret, flv; // its caret, its math level as drawn
static bool done;

static NOINL void add(const std::string & m, const std::string & msg, int kind) { t_entry e; e.math = m; e.msg = msg; e.kind = (char)kind; E->push_back(e); }
static NOINL int lines(const std::string & s) { int n = 1; for (unsigned i = 0; i < s.size(); ++i) n += s[i] == '\n'; return s.empty() ? 0 : n; }
static NOINL void text_lines(const std::string & s, int x, int base, int fg, int bg) { // the message, a line a \n
  const char * p = s.c_str();
  for (;;) {
    const char * q = strchr(p, '\n');
    ui_draw_text(&ui_tr10, p, q ? q - p : -1, x, base, ui_ramp(0, fg, bg), 0);
    if (!q) break;
    p = q + 1; base += 12;
  }
}
static NOINL int msg_w(const std::string & m) { // its widest line
  int w = 0;
  for (const char * p = m.c_str(); *p;) {
    const char * q = strchr(p, '\n');
    const int lw = ui_text_width(&ui_tr10, p, q ? q - p : -1);
    if (lw > w) w = lw;
    if (!q) break;
    p = q + 1;
  }
  return w;
}
// an entry's layout: its math (level, size), its message beside it when there is room, else under
// it; returns its height (with the gap under it)
static NOINL int entry_lay(const t_entry & e, mi_layout & L, int & lv, int & mx, int & my) {
  int h = 0, wm = 0;
  const bool box = e.kind == E_HINT || e.kind == E_DONE || e.kind == E_FORMULA;
  if (!e.math.empty()) {
    lv = ui_math_fit(e.math.c_str(), e.math.size(), -1, 4, box ? 284 : 200, 40, MI_F_IMPLDOT, L);
    h = L.asc + L.desc; wm = L.width;
  }
  const int t = lines(e.msg) * 12, beside = !box && (!h || 18 + wm + 14 + msg_w(e.msg) <= UI_W - 8);
  mx = box ? 18 : h ? (beside ? 18 + wm + 14 : 18) : 18;
  my = beside ? 12 : h + 14; // (the first line's baseline, from the top)
  if (box && h) my = h + 14;
  return (beside ? (h > t ? h : t) : h + t + (h ? 2 : 0)) + 8;
}
static NOINL int entry_h(const t_entry & e) { mi_layout L; int lv, mx, my; return entry_lay(e, L, lv, mx, my); }
static NOINL void entry_draw(const t_entry & e, int y) {
  mi_layout L;
  int lv = 0, mx, my;
  const int h = entry_lay(e, L, lv, mx, my) - 8;
  const bool box = e.kind == E_HINT || e.kind == E_DONE || e.kind == E_FORMULA;
  const int bg = box ? UC_ACCSOFT : UC_BG;
  if (box) ui_rrect(0, 10, y, UI_W - 20, h + 6, 6, UC_ACCSOFT, UC_BG);
  else ui_fill(10, y + 1, 3, h, ui_col(0, e.kind == E_RIGHT ? UC_ACC : e.kind == E_WRONG ? UC_RED : UC_LINE));
  if (!e.math.empty())
    ui_math_draw(L, e.math.c_str(), lv, box ? (UI_W - L.width) / 2 : 18, y + 3 + L.asc, 0, e.kind == E_SHOWN ? UC_SUB : UC_INK, bg, UC_ACC);
  const int fg = e.kind == E_RIGHT || e.kind == E_DONE ? UC_ACC : e.kind == E_WRONG ? UC_RED : UC_SUB;
  text_lines(e.msg, mx, y + my, fg, bg);
}
static NOINL void paint() {
  const int rows = ui_band_open(48);
  for (int b = Y0; b < SBOT; b += rows ? rows : SBOT - Y0) {
    const int e = rows && b + rows < SBOT ? b + rows : SBOT;
    if (rows) ui_band_begin(b, e); else ui_clip(0, b, UI_W, e);
    ui_fill(0, Y0, UI_W, SBOT - Y0, ui_col(0, UC_BG));
    ui_draw_text(&ui_tb10, tutor_ask(), -1, 12, Y0 + 14, ui_ramp(0, UC_ACC, UC_BG), 0);
    { // the problem
      const std::string & p = tutor_problem();
      mi_layout L;
      const int lv = ui_math_fit(p.c_str(), p.size(), -1, 3, UI_W - 24, 34, MI_F_IMPLDOT, L);
      ui_math_draw(L, p.c_str(), lv, (UI_W - L.width) / 2, Y0 + 22 + L.asc, 0, UC_INK, UC_BG, UC_ACC);
    }
    ui_fill(12, TOP - 4, UI_W - 24, 1, ui_col(0, UC_LINE));
    // the entries, the last ones that fit above the field (the newest at the bottom)
    int y = done ? SBOT - 4 : FY - 4, k = E->size();
    while (k > 0 && y - entry_h((*E)[k - 1]) >= TOP) y -= entry_h((*E)[--k]);
    for (; k < (int)E->size(); ++k) { entry_draw((*E)[k], y); y += entry_h((*E)[k]); }
    if (!done) { // the line being typed
      ui_rrect(0, 8, FY, UI_W - 16, FH, 7, UC_ACC, UC_BG);
      ui_rrect(0, 9, FY + 1, UI_W - 18, FH - 2, 6, UC_CARD, UC_ACC);
      const int n = strlen(fld);
      mi_layout L;
      flv = ui_math_fit(fld, n, caret, 3, UI_W - 32, FH - 6, MI_F_CALLBOX | MI_F_HINTS, L);
      const int x = 16 + (L.width > UI_W - 32 ? UI_W - 32 - L.width : 0), base = FY + (FH - L.asc - L.desc) / 2 + L.asc;
      if (!n) ui_draw_text(&ui_tr10, "your next line", -1, 20, FY + 21, ui_ramp(0, UC_SUB, UC_CARD), 0);
      ui_clip(10, FY + 2, UI_W - 10, FY + FH - 2);
      ui_math_draw(L, fld, flv, x, base, 0, UC_INK, UC_CARD, UC_ACC);
      ui_math_caret(L, x, base, 0, UC_ACC);
      if (rows) ui_noclip(); else ui_clip(0, b, UI_W, e);
    }
    if (rows) ui_band_end();
  }
  ui_band_close();
  ui_noclip();
}

// ---------------------------------------------------------------- the line being typed
static NOINL void type_key(int key) { // as on the console's edit line (console.cc, 2D)
  int n = strlen(fld), mk = 0;
  if (key == KEY_CHAR_PMINUS) key = '-';
  if (key == '\t') key = KEY_CHAR_RECIP; // (the TI-84 CE's x^-1 key)
  if (strchr("/()[]{},=<>+-", key) && key) mk = key;
  else if (key == KEY_CHAR_SQUARE) mk = ME_SQUARE;
  else if (key == KEY_CHAR_RECIP) mk = ME_RECIP;
  if (mk && me_key(fld, CAP, &caret, mk)) return;
  char one[2] = {(char)key, 0};
  const char * t = key >= ' ' && key < 127 ? one : keytostring(key, 0, false);
  if (!t) return;
  char ins[64];
  const int tl = strlen(t);
  if (tl > 60) return;
  strcpy(ins, t);
  int back = 0;
  if (!strcmp(t, "^") || (tl > 1 && t[tl - 1] == '(')) { strcat(ins, t[tl - 1] == '(' ? ")" : "()"); back = 1; } // (^(), sin(): the caret inside)
  const int il = strlen(ins);
  if (n + il >= CAP) return;
  memmove(fld + caret + il, fld + caret, n - caret + 1);
  memcpy(fld + caret, ins, il);
  caret += il - back;
}
static NOINL void edit_key(int key) {
  const mi_metrics & m = ui_math_metrics(flv, MI_F_CALLBOX | MI_F_HINTS);
  const int n = strlen(fld);
  if (key == KEY_CTRL_LEFT || key == KEY_CTRL_RIGHT || key == KEY_CTRL_UP || key == KEY_CTRL_DOWN) {
    const int c = mi_move(fld, n, caret, key == KEY_CTRL_LEFT ? 0 : key == KEY_CTRL_RIGHT ? 1 : key == KEY_CTRL_UP ? 2 : 3, m);
    if (c >= 0) caret = c;
  } else if (key == KEY_CTRL_DEL) {
    if (!me_backspace(fld, &caret)) {
      int from, to;
      const int c = mi_backspace(fld, n, caret, from, to, m);
      if (to > from) memmove(fld + from, fld + to, n - to + 1);
      caret = c;
    }
  } else type_key(key);
}

// ---------------------------------------------------------------- the screen
static NOINL void quiet(int on) { dconsole_mode = !on; giac::control_c_hook = on ? focus_busy_tick : 0; giac::ctrl_c = giac::interrupted = false; }
static NOINL void summary() {
  int r, w, h, b;
  tutor_counts(r, w, h, b);
  std::string m = "Solved in " + giac::print_INT_(r) + (r == 1 ? " line" : " lines");
  if (h) m += ", " + giac::print_INT_(h) + (h == 1 ? " hint" : " hints");
  m += "\nThe book took " + giac::print_INT_(b) + (b == 1 ? " step" : " steps") + (w ? "" : ". No slips!");
  add("", m, E_DONE);
  done = true;
}
static NOINL void hint(bool line) { // F1 a level, F2 down to the line
  for (int guard = 0; guard < 3; ++guard) {
    std::string t;
    std::vector<std::string> shown;
    quiet(1);
    const int lv = tutor_hint(t, shown, contextptr);
    quiet(0);
    if (!lv) { add("", "Nothing left: it is solved", E_NOTE); return; }
    if (lv == 3) {
      for (unsigned i = 0; i < shown.size(); ++i) add(shown[i], i ? "" : t, E_SHOWN);
      return;
    }
    if (!line) { add(lv == 2 ? t : "", lv == 2 ? "" : t, lv == 2 ? E_FORMULA : E_HINT); return; }
  }
}
static NOINL bool start(const char * cmd) {
  quiet(1);
  const bool ok = tutor_start(cmd, contextptr);
  quiet(0);
  E->clear();
  fld[0] = 0; caret = 0; done = false;
  return ok;
}
// cmd: the problem (solve(...), factor(...), diff(...)); next: Practice's next one (0: start again)
bool focus_tutor(const char * cmd, const char * (*next)(), const char * label) {
  if (!E) E = new std::vector<t_entry>;
  if (!start(cmd)) return false;
  const int view0 = focus_view;
  focus_view = 1;
  static const char * const tabs[5] = {"hint", "next line", "all steps", "new one", "back"};
  static const char * const tabs2[5] = {"hint", "next line", "all steps", "again", "back"};
  static const char * const tabs3[5] = {"", "", "all steps", "new one", "back"}, * const tabs4[5] = {"", "", "all steps", "again", "back"};
  for (;;) {
    focus_status_label(done ? "SOLVED" : label);
    focus_bar_tabs(done ? (next ? tabs3 : tabs4) : next ? tabs : tabs2, -1); // (solved: no hints left)
    paint();
    int key;
    GetKey(&key);
    if ((key == KEY_CTRL_EXIT || key == KEY_CTRL_AC) && fld[0]) { fld[0] = 0; caret = 0; continue; } // (clear: the line first)
    if (key == KEY_CTRL_EXIT || key == KEY_CTRL_F5 || key == KEY_CTRL_AC) break;
    if (key == KEY_CTRL_F4) { // another problem, or this one again
      const char * c = next ? next() : cmd;
      for (int k = 0; next && c && !start(c) && k < 5; ++k) c = next(); // (a made-up one it does not take: another)
      if (!next && c) start(c);
      continue;
    }
    if (key == KEY_CTRL_F3) { // all the steps (the book), then back here
      focus_steps(cmd, 0);
      focus_view = 1;
      continue;
    }
    if (done) continue;
    if (key == KEY_CTRL_F1 || key == KEY_CTRL_F2) { hint(key == KEY_CTRL_F2); continue; }
    if (key == KEY_CTRL_EXE) {
      if (!fld[0]) continue;
      quiet(1);
      const tutor_res r = tutor_check(fld, contextptr);
      quiet(0);
      char shown[2 * CAP]; // (as the console reads it: 2xcos(x) drawn 2x cos(x))
      strcpy(shown, fld);
      khicas_implicit_mult(shown, sizeof shown);
      add(shown, r.msg, r.ok == 1 ? E_RIGHT : r.ok == 0 ? E_WRONG : E_NOTE);
      if (r.ok == -1) E->back().math.clear();
      if (r.ok == 1) { fld[0] = 0; caret = 0; }
      if (r.solved) summary();
      continue;
    }
    edit_key(key);
  }
  focus_view = view0;
  focus_status_label(0);
  focus_invalidate();
  return true;
}
#endif
