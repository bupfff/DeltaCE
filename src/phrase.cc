// phrase.cc - DeltaCE's phrases (phrase.h): the table, and the pre-pass that puts them into giac.
// A phrase: its name, its number of arguments, its words (one piece before, between and after the
// arguments) and its template (giac, %1..%4 its arguments, each put in parentheses); a fact (an
// equation) is a story's. find: the story's letters, the one asked first, solved together.
// An equation put into giac is ph_fact(left side, right side), drawn as left = right: typed alone,
// l=2w+3 would be stored into l (giac's equal-to-store) and x+5=12 solved (answer.cc,
// equation_unknown); ph_fact(l=2w+3) would be ph_fact with l replaced (giac's f(x=a)).
#if defined FLOWCE_STEPS || defined HOSTGIAC
#include <giac/giacPCH.h> // first: its "#define std ustl"
#include <string.h>
#include "phrase.h"
using namespace giac;
void answer_prepass(char * buf, int cap, bool focus); // answer.cc
extern giac::context * contextptr;

const char * phrase_story;
int phrase_bare;
// fact: 0 a question, 1 a story's fact, 2 find, 3 a question answered by an equation, 4 the wrapper
// group: its tab in Word problems (1-4, focus_catalog.cc; 0 none), ex: its example's arguments
struct phr { const char * name; unsigned char na, fact, group; const char * words, * tmpl, * ex; };
static const phr PH[] = {
  // % & rate
  {"ph_percentof", 2, 0, 1, "\0% of\0", "%1/100*%2", "15,80"},
  {"ph_whatpercent", 2, 0, 1, "what percent of\0is\0?", "100*%2/%1", "80,12"},
  {"ph_increased", 2, 0, 1, "\0increased by\0%", "%1*(1+%2/100)", "80,15"},
  {"ph_decreased", 2, 0, 1, "\0decreased by\0%", "%1*(1-%2/100)", "80,15"},
  {"ph_sale", 2, 0, 1, "the sale price of\0at\0% off", "%1*(1-%2/100)", "80,25"},
  {"ph_tax", 2, 0, 1, "\0plus\0% tax", "%1*(1+%2/100)", "40,8"},
  {"ph_change", 2, 0, 1, "the percent change from\0to\0", "100*(%2-%1)/%1", "50,65"},
  {"ph_interest", 4, 0, 1, "\0at\0% compounded\0times a year for\0years", "round(evalf(%1*(1+%2/100/%3)^(%3*%4)),2)", "1000,5,12,10"},
  {"ph_simpleinterest", 3, 0, 1, "the simple interest on\0at\0% for\0years", "%1*%2/100*%3", "1000,5,3"},
  {"ph_atleastone", 2, 0, 1, "the chance of at least one success in\0tries, each\0", "1-(1-%2)^%1", "4,1/6"},
  {"ph_halflife", 3, 0, 1, "what is left of\0with a half-life of\0after\0", "%1*(1/2)^(%3/%2)", "100,5,15"},
  {"ph_together", 2, 0, 1, "working together, one taking\0hours and one\0hours, take", "1/(1/%1+1/%2)*_h", "3,6"},
  {"ph_proportion", 3, 0, 1, "if\0goes with\0then\0goes with", "%3*%2/%1", "3,12,5"},
  {"ph_unitrate", 2, 0, 1, "the unit rate of\0for\0", "%1/%2", "12,4"},
  // (distance, rate and time: their units cancel in the answer, answer.cc: 150 mi at 50 mi/h, 3 h)
  {"ph_howlong", 2, 0, 1, "how long to go\0at\0", "%1/%2", "150_mi,50_mi/_h"},
  {"ph_howfar", 2, 0, 1, "how far at\0for\0", "%1*%2", "60_km/_h,2_h"},
  {"ph_speed", 2, 0, 1, "the speed to go\0in\0", "%1/%2", "150_mi,3_h"},
  {"ph_needscore", 2, 0, 1, "the score needed to average\0after scores\0", "%1*(size(%2)+1)-sum(%2)", "85,[80,90]"},
  // geometry (and right-triangle trig)
  {"ph_slope", 4, 0, 2, "the slope through (\0,\0) and (\0,\0)", "(%4-%2)/(%3-%1)", "1,2,3,8"},
  {"ph_line", 4, 3, 2, "the line through (\0,\0) and (\0,\0)", "y=(%4-%2)/(%3-%1)*(x-%1)+%2", "1,2,3,8"},
  {"ph_distance", 4, 0, 2, "the distance from (\0,\0) to (\0,\0)", "sqrt((%3-%1)^2+(%4-%2)^2)", "0,0,3,4"},
  {"ph_midpoint", 4, 0, 2, "the midpoint of (\0,\0) and (\0,\0)", "[(%1+%3)/2,(%2+%4)/2]", "1,2,3,8"},
  {"ph_hypotenuse", 2, 0, 2, "the hypotenuse with legs\0and\0", "sqrt(%1^2+%2^2)", "3,4"},
  {"ph_leg", 2, 0, 2, "the other leg of a right triangle with hypotenuse\0and leg\0", "sqrt(%1^2-%2^2)", "5,3"},
  {"ph_thirdangle", 2, 0, 2, "the third angle of a triangle with angles\0and\0", "180-%1-%2", "50,60"},
  {"ph_rightangle", 2, 0, 2, "the angle opposite side\0next to side\0(degrees)", "evalf(atan(%1/%2)*180/pi)", "3,4"},
  {"ph_lawcos", 3, 0, 2, "the third side of sides\0and\0with\0degrees between", "evalf(sqrt(%1^2+%2^2-2*%1*%2*cos(%3*pi/180)))", "5,7,60"},
  {"ph_elevation", 2, 0, 2, "the height at an angle of elevation of\0degrees from\0away", "%2*tan(%1*pi/180)", "30,50"},
  {"ph_circlearea", 1, 0, 2, "the area of a circle of radius\0", "pi*%1^2", "3"},
  {"ph_circumference", 1, 0, 2, "the circumference of a circle of radius\0", "2*pi*%1", "3"},
  {"ph_sector", 2, 0, 2, "the area of a sector of radius\0and angle\0degrees", "pi*%1^2*%2/360", "6,60"},
  {"ph_arclength", 2, 0, 2, "the arc length of radius\0and angle\0degrees", "pi*%1*%2/180", "6,60"},
  {"ph_rectperim", 2, 0, 2, "the perimeter of a rectangle\0by\0", "2*%1+2*%2", "5,3"},
  {"ph_trapezoid", 3, 0, 2, "the area of a trapezoid with bases\0and\0and height\0", "(%1+%2)*%3/2", "3,5,4"},
  {"ph_triangle", 2, 0, 2, "the area of a triangle with base\0and height\0", "%1*%2/2", "6,4"},
  {"ph_interior", 1, 0, 2, "each angle of a regular polygon with\0sides", "180*(%1-2)/%1", "6"},
  {"ph_box", 3, 0, 2, "the volume of a box\0by\0by\0", "%1*%2*%3", "2,3,4"},
  {"ph_cylinder", 2, 0, 2, "the volume of a cylinder of radius\0and height\0", "pi*%1^2*%2", "2,5"},
  {"ph_sphere", 1, 0, 2, "the volume of a sphere of radius\0", "4*pi*%1^3/3", "3"},
  {"ph_cone", 2, 0, 2, "the volume of a cone of radius\0and height\0", "pi*%1^2*%2/3", "3,4"},
  // multivar: a story's facts, then find
  {"ph_find", 1, 2, 3, "find\0", 0, "w"},
  {"ph_solvefor", 2, 0, 3, "solve\0for\0", "solve(%1,%2)", "A=l*w,w"},
  {"ph_is", 2, 1, 3, "\0is\0", "%1=%2", "t,36"},
  {"ph_more", 3, 1, 3, "\0is\0more than\0", "%1=%3+%2", "l,3,2w"},
  {"ph_less", 3, 1, 3, "\0is\0less than\0", "%1=%3-%2", "a,5,b"},
  {"ph_older", 3, 1, 3, "\0is\0years older than\0", "%1=%3+%2", "m,4,s"},
  {"ph_times", 3, 1, 3, "\0is\0times\0", "%1=%2*%3", "a,3,b"},
  {"ph_percentis", 3, 1, 3, "\0is\0% of\0", "%1=%2/100*%3", "s,20,p"},
  {"ph_sum", 3, 1, 3, "the sum of\0and\0is\0", "%1+%2=%3", "x,y,25"},
  {"ph_difference", 3, 1, 3, "the difference of\0and\0is\0", "%1-%2=%3", "x,y,7"},
  {"ph_product", 3, 1, 3, "the product of\0and\0is\0", "%1*%2=%3", "x,y,12"},
  {"ph_cost", 3, 1, 3, "\0items at\0each cost\0", "%1*%2=%3", "n,3,36"},
  {"ph_worth", 4, 0, 3, "\0at\0each and\0at\0each", "%1*%2+%3*%4", "q,0.25,d,0.1"},
  {"ph_mix", 4, 0, 3, "the percent of\0at\0% mixed with\0at\0%", "(%1*%2+%3*%4)/(%1+%3)", "2,10,3,40"},
  {"ph_perimeter", 3, 1, 3, "a rectangle\0by\0has perimeter\0", "2*%1+2*%2=%3", "l,w,36"},
  {"ph_rectarea", 3, 1, 3, "a rectangle\0by\0has area\0", "%1*%2=%3", "l,w,40"},
  {"ph_drt", 3, 1, 3, "distance\0= rate\0times time\0", "%1=%2*%3", "150_mi,r,3_h"},
  {"ph_consecutive", 3, 1, 3, "the sum of\0consecutive integers from\0is\0", "%1*%2+%1*(%1-1)/2=%3", "3,n,72"},
  // functions
  {"ph_value", 3, 0, 4, "the value of\0when\0=\0", "subst(%1,%2=%3)", "x^2-5x+6,x,3"},
  {"ph_when", 2, 0, 4, "when is\0=\0", "solve(%1=%2)", "2x+3,11"},
  {"ph_zeros", 1, 0, 4, "the zeros of\0", "solve(%1=0)", "x^2-4"},
  {"ph_meets", 2, 0, 4, "where\0meets\0", "solve(%1=%2)", "x^2,2x+3"},
  {"ph_rate", 3, 0, 4, "the rate of change of\0at\0=\0", "subst(diff(%1,%2),%2=%3)", "x^3,x,2"},
  {"ph_avgrate", 4, 0, 4, "the average rate of change of\0from\0=\0to\0", "(subst(%1,%2=%4)-subst(%1,%2=%3))/(%4-%3)", "x^2,x,1,3"},
  {"ph_tangent", 3, 3, 4, "the tangent line to\0at\0=\0", "y=subst(diff(%1,%2),%2=%3)*(%2-%3)+subst(%1,%2=%3)", "x^2,x,3"},
  {"ph_area", 4, 0, 4, "the area under\0from\0=\0to\0", "integrate(%1,%2,%3,%4)", "x^2,x,0,3"},
  {"ph_avgvalue", 4, 0, 4, "the average value of\0from\0=\0to\0", "integrate(%1,%2,%3,%4)/(%4-%3)", "x^2,x,0,3"},
  {"ph_limit", 3, 0, 4, "the limit of\0as\0goes to\0", "limit(%1,%2,%3)", "sin(x)/x,x,0"},
  {"ph_doubling", 3, 0, 4, "a population of\0doubling every\0after\0", "%1*2^(%3/%2)", "500,3,12"},
  {"ph_lands", 2, 0, 4, "a ball thrown up at\0m/s from a height of\0m lands after", "evalf(max(solve(-49/10*t^2+%1*t+%2=0,t)))*_s", "10,2"},
  {"ph_landsft", 2, 0, 4, "a ball thrown up at\0ft/s from a height of\0ft lands after", "evalf(max(solve(-16*t^2+%1*t+%2=0,t)))*_s", "64,0"},
  {"ph_maxheight", 2, 0, 4, "the highest a ball thrown up at\0m/s from a height of\0m goes", "evalf(%2+5*%1^2/98)*_m", "10,2"},
  {"ph_arithmetic", 3, 0, 4, "term\0of the arithmetic sequence starting at\0adding\0each time", "%2+(%1-1)*%3", "10,3,4"},
  {"ph_geometric", 3, 0, 4, "term\0of the geometric sequence starting at\0times\0each time", "%2*%3^(%1-1)", "5,2,3"},
  {"ph_fact", 2, 4, 0, "\0=\0", 0, 0}, // (an equation, neither stored nor solved: drawn as one)
};
enum { NPH = sizeof(PH) / sizeof(PH[0]) };

static int lookup(const char * name, int nlen, int na) {
  for (int i = 0; i < NPH; ++i)
    if ((na < 0 || PH[i].na == na) && !strncmp(PH[i].name, name, nlen) && !PH[i].name[nlen]) return i;
  return -1;
}
const char * phrase_words(const char * name, int nlen, int na) {
  const int i = nlen > 3 && name[0] == 'p' && name[1] == 'h' && name[2] == '_' ? lookup(name, nlen, na) : -1;
  return i < 0 ? 0 : PH[i].words;
}
static bool letter(char c) { return (c | 32) >= 'a' && (c | 32) <= 'z'; }
static bool namec(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }
// what the letters in a story's facts stand for (find answers in words: "width w = 5"): the roles
// of the phrases' blanks, recorded as their facts are put into giac
static const char * const ROLES[][2] = {
  {"ph_perimeter", "length\0width\0perimeter"}, {"ph_rectarea", "length\0width\0area"},
  {"ph_drt", "distance\0rate\0time"}, {"ph_cost", "number\0price\0cost"},
  {"ph_older", "older age\0years\0younger age"}, {"ph_consecutive", "count\0first\0sum"},
  {"ph_worth", "number\0price\0number\0price"}, {"ph_mix", "amount\0percent\0amount\0percent"},
  {"ph_sum", "first number\0second number\0sum"}, {"ph_difference", "first number\0second number\0difference"},
  {"ph_product", "first number\0second number\0product"}};
static char rv[8][4], ru[8][14]; // (ru: its unit, from a box of its fact: 36_cm; an area's cm^2: cm)
static const char * rr[8];
static int rn;
void phrase_roles_clear() { rn = 0; }
static void unit_in(const char * a, int e, char * u) { // a box's unit: 36_cm, _cm (cm^2: cm); "" none
  *u = 0;
  for (int c = 0; c < e; ++c)
    if (a[c] == '_' && c + 1 < e && (a[c + 1] | 32) >= 'a' && (a[c + 1] | 32) <= 'z') {
      int L = e - c;
      if (L > 2 && a[c + L - 2] == '^') L -= 2; // (cm^2: cm)
      if (L > 13) L = 13;
      memcpy(u, a + c, L); u[L] = 0;
      return;
    }
}
static void record_roles(const char * name, const char * const * a, const int * e, int na) {
  for (unsigned j = 0; j < sizeof(ROLES) / sizeof(ROLES[0]); ++j) {
    if (strcmp(ROLES[j][0], name)) continue;
    const char * r = ROLES[j][1];
    // a distance, a rate, a time: units of their own, the letter's from the other two (150_mi and
    // 3_h: r in mi/h; 60_km/_h: d in km, t in h). Else the first box's with a unit
    const bool drt = !strcmp(name, "ph_drt");
    char u[14] = "", du[3][14];
    for (int k = 0; k < na && (drt || !*u); ++k) unit_in(a[k], e[k], drt ? du[k] : u);
    for (int k = 0; k < na; ++k, r += strlen(r) + 1) {
      if (drt) {
        const char * sl = strchr(du[1], '/');
        *u = 0;
        if (k == 1 && *du[0] && *du[2] && strlen(du[0]) + strlen(du[2]) < 13) { strcpy(u, du[0]); strcat(u, "/"); strcat(u, du[2]); }
        else if (k == 0 && sl) { memcpy(u, du[1], sl - du[1]); u[sl - du[1]] = 0; }
        else if (k == 2 && sl) strcpy(u, sl + 1);
      }
      bool id = e[k] >= 1 && e[k] <= 3 && ((a[k][0] | 32) >= 'a' && (a[k][0] | 32) <= 'z');
      for (int c = 1; id && c < e[k]; ++c) id = namec(a[k][c]);
      if (!id) continue;
      int i = 0;
      while (i < rn && (strncmp(rv[i], a[k], e[k]) || rv[i][e[k]])) ++i;
      if (i < rn || rn == 8) continue; // (the first role a letter gets stays: 'number' over 'first number')
      memcpy(rv[rn], a[k], e[k]); rv[rn][e[k]] = 0; strcpy(ru[rn], u); rr[rn++] = r;
    }
  }
}
const char * phrase_at(int i, int * na, int * group, const char ** ex) {
  if (i < 0 || i >= NPH) return 0;
  *na = PH[i].na; *group = PH[i].group; *ex = PH[i].ex;
  return PH[i].name;
}
// the kind (fact 1, find 2) of the first phrase in line, -1: none; with find set, 2 if a find is
// anywhere in it
static int first_kind(const char * line, bool find = false) {
  for (const char * p = strstr(line, "ph_"); p; p = strstr(p + 1, "ph_")) {
    if (p > line && namec(p[-1])) continue;
    int n = 0;
    while (namec(p[n])) ++n;
    const int i = lookup(p, n, -1);
    if (i >= 0 && (!find || PH[i].fact == 2)) return PH[i].fact;
  }
  return -1;
}
bool phrase_is_fact(const char * line) { return first_kind(line) == 1; }
bool phrase_is_find(const char * line) { return first_kind(line, true) == 2; }

// a line that tells a story and asks at once ("[l] is [3] more than [2w], ..., find [w]"): its
// facts (the parts between commas whose phrase is a fact) join the story told before, put into
// giac as the history's facts are (bare, with their implicit products); the rest stays
static void combine(char * buf, int cap) {
  static std::string * story; // (no static constructor in the app)
  std::string rest;
  if (!story && !(story = new std::string)) return;
  *story = phrase_story ? phrase_story : "";
  bool any = false;
  for (int i = 0, d = 0, b = 0;; ++i) {
    const char c = buf[i];
    if (c == '(' || c == '[' || c == '{') ++d;
    else if ((c == ')' || c == ']' || c == '}') && d) --d;
    else if (!c || (c == ',' && !d)) {
      const std::string part(buf + b, i - b);
      const int pc = 2 * part.size() + 64;
      char * t = first_kind(part.c_str()) == 1 ? (char *)malloc(pc) : 0;
      if (t) {
        strcpy(t, part.c_str());
        phrase_bare = 1;
        answer_prepass(t, pc, true);
        phrase_bare = 0;
        if (!story->empty()) *story += ',';
        *story += t;
        free(t);
        any = true;
      }
      else { if (!rest.empty()) rest += ','; rest += part.c_str(); }
      if (!c) break;
      b = i + 1;
    }
  }
  if (!any || (int)rest.size() >= cap) return;
  phrase_story = story->c_str();
  strcpy(buf, rest.c_str());
}

#ifdef WITH_UNITS
bool unit_si(const gen & q, gen & si, gen & kind); // answer.cc
// the story's units: each kind's first is the one it is solved in, the others are put in it (30_min
// after 60_mi/_h: 1/2 h, 30 mi), and a role's answer is given in its own (phrase_answer)
static char uref[6][12];
static int nref;
static gen in_ref(const char * nm, int n) { // the unit nm (n chars) in the story's: 1, 1/60...
  std::string q("1_");
  q.append(nm, n);
  gen si, k, sr, kr;
  if (!unit_si(gen(q, contextptr), si, k)) return 1;
  for (int r = 0; r < nref; ++r) {
    std::string t("1_");
    t += uref[r];
    if (unit_si(gen(t, contextptr), sr, kr) && kr == k) return si / sr;
  }
  if (nref < 6 && n < 12) { memcpy(uref[nref], nm, n); uref[nref++][n] = 0; }
  return 1;
}
#endif
// text with its units as numbers: in the story's units (in_ref), else 1 (36_cm: 36*1)
static void unitless(std::string & st, const char * p) {
  for (; *p; ++p)
    if (*p == '_' && ((p[1] | 32) >= 'a' && (p[1] | 32) <= 'z')) {
      const char c = st.empty() ? 0 : st[st.size() - 1];
      if (namec(c) || c == ')') st += '*';
      int n = 0;
      while (namec(p[1 + n])) ++n;
#ifdef WITH_UNITS
      st += '('; st += in_ref(p + 1, n).print(contextptr).c_str(); st += ')';
#else
      st += '1';
#endif
      p += n;
    }
    else st += *p;
}
// find(v): [v, the story's other letters] = their values
static void find_of(std::string & out, const std::string & v) {
  if (!phrase_story || !*phrase_story) { out = "\"Tell the story first, a fact a line\""; return; }
  std::string st("[");
#ifdef WITH_UNITS
  nref = 0;
  for (const char * p = strstr(phrase_story, "/_"); p; p = strstr(p + 1, "/_")) { // a rate's units first
    const char * a = p; // (50_min then 40_mi/_h: solved in mi and h, the rates in mi/h)
    while (a > phrase_story && letter(a[-1])) --a;
    if (a > phrase_story && a[-1] == '_') in_ref(a, p - a);
    int n = 0;
    while (letter(p[2 + n])) ++n;
    in_ref(p + 2, n);
  }
#endif
  unitless(st, phrase_story);
  st += ']';
  gen g(st, contextptr);
  vecteur ids = lidnt(g);
  std::string vars("[");
  vars += v;
  for (unsigned i = 0; i < ids.size(); ++i) {
    if (evalf(ids[i], 1, contextptr).type == _DOUBLE_) continue; // (pi, e)
    const std::string s = ids[i].print(contextptr);
    if (s != v) { vars += ','; vars += s; }
  }
  vars += ']';
  // (one picked: phrase_answer). A story with decimals is solved exact, its answers decimals (solve
  // with 2.1, 6.5 and 5.4 found no single solution)
  const bool dec = strchr(phrase_story, '.');
  out = vars; out += dec ? "=evalf(solve(exact(" : "=solve("; out += st; out += dec ? ")," : ","; out += vars; out += dec ? "))" : ")";
}
bool phrase_expand(char * buf, int cap) {
  bool any = false;
  const char * told = phrase_story; // (a line's own facts join it for this line only)
  if (!phrase_bare && phrase_is_find(buf)) combine(buf, cap);
  for (int guard = 0; guard < 24; ++guard) {
    // the last phrase in buf: the innermost (a phrase in another's box comes after its start)
    char * p = 0;
    for (char * q = strstr(buf, "ph_"); q; q = strstr(q + 1, "ph_"))
      if ((q == buf || !namec(q[-1])) && strncmp(q, "ph_fact(", 8)) p = q;
    if (!p) break;
    int n = 0;
    while (namec(p[n])) ++n;
    if (p[n] != '(') break;
    // its arguments: [a[k], e[k]) at depth 0
    const char * a[4]; int e[4], na = 0, d = 0, j = n + 1;
    a[0] = p + j;
    for (; p[j]; ++j) {
      const char c = p[j];
      if (c == '(' || c == '[' || c == '{') ++d;
      else if ((c == ')' || c == ']' || c == '}') && d) --d;
      else if (c == ')' || (c == ',' && !d)) {
        if (na < 4) e[na] = p + j - a[na];
        ++na;
        if (c == ')') break;
        if (na < 4) a[na] = p + j + 1;
      }
    }
    if (p[j] != ')' || na > 4) break;
    const int i = lookup(p, n, na);
    if (i < 0) break;
    record_roles(PH[i].name, a, e, na);
    std::string r;
    // the line through two points with the same x (typed alike): x = that x, not a division by 0
    const char * tm = !strcmp(PH[i].name, "ph_line") && e[0] == e[2] && !strncmp(a[0], a[2], e[0]) ? "x=%1" : PH[i].tmpl;
    // a list in a question's box (15 % of [80,90]): the phrase for each of its numbers, a list of
    // answers (the score needed after scores [..] takes the list itself, one score a list of one)
    int lk = -1;
    const bool scores = !strcmp(PH[i].name, "ph_needscore");
    for (int k = 0; k < na && lk < 0 && !PH[i].fact && !scores; ++k)
      if (e[k] > 2 && a[k][0] == '[' && a[k][e[k] - 1] == ']') lk = k;
    if (!tm) find_of(r, std::string(a[0], e[0]));
    else {
      const char * la = lk >= 0 ? a[lk] + 1 : 0;
      const int le = lk >= 0 ? e[lk] - 2 : 0;
      if (lk >= 0) r += '[';
      for (int p0 = 0;;) {
        int p1 = p0;
        for (int dd = 0; lk >= 0 && p1 < le && (la[p1] != ',' || dd); ++p1) // (its next number)
          if (la[p1] == '(' || la[p1] == '[') ++dd; else if (la[p1] == ')' || la[p1] == ']') --dd;
        for (const char * t = tm; *t; ++t) {
          if (*t == '%' && t[1] >= '1' && t[1] <= '0' + na) {
            const int k = t[1] - '1';
            r += '(';
            if (k == lk) r.append(la + p0, p1 - p0);
            else if (scores && k == 1 && *a[k] != '[') { r += '['; r.append(a[k], e[k]); r += ']'; }
            else r.append(a[k], e[k]);
            r += ')'; ++t;
          }
          else r += *t;
        }
        if (lk < 0 || p1 >= le) break;
        r += ','; p0 = p1 + 1;
      }
      if (lk >= 0) r += ']';
    }
    if (PH[i].fact && !phrase_bare) // the equation: ph_fact(left, right)
      for (int k = 0, dd = 0; k < (int)r.size(); ++k) {
        const char c = r[k];
        if (c == '(' || c == '[') ++dd; else if (c == ')' || c == ']') --dd;
        else if (c == '=' && !dd) { std::string w("ph_fact("); w.append(r.c_str(), k); w += ','; w += r.c_str() + k + 1; w += ')'; r = w; break; }
      }
    const int at = p - buf, old = j + 1, rest = strlen(p + old);
    if (at + (int)r.size() + rest + 1 > cap) break;
    memmove(buf + at + r.size(), p + old, rest + 1);
    memcpy(buf + at, r.c_str(), r.size());
    any = true;
  }
  phrase_story = told;
  return any;
}
static bool negative(const gen & s) { // a value of the solution s below 0
  for (unsigned i = 0; s.type == _VECT && i < s._VECTptr->size(); ++i) {
    const gen e = evalf(s[i], 1, contextptr);
    if (e.type == _DOUBLE_ && e._DOUBLE_val < 0) return true;
  }
  return false;
}
// a find's answer, [w, l] = [5, 13] (ph_fact of two lists), in words when its letters have
// roles: "width w = 5, length l = 13". Its solutions come all: the first with no value below 0 is
// kept (a width, a time: l*w = 40 and l = w+3 is w = 5, not -8), else the first
bool phrase_answer(gen & g, std::string & msg) {
  if (!g.is_symb_of_sommet(at_of)) return false;
  const gen & f = g._SYMBptr->feuille;
  if (f.type != _VECT || f._VECTptr->size() != 2 || f[0].print(contextptr) != "ph_fact") return false;
  if (f[1].type == _VECT && f[1]._VECTptr->size() == 2 && f[1][1].type == _VECT && !f[1][1]._VECTptr->empty() && f[1][1][0].type == _VECT) {
    const vecteur & S = *f[1][1]._VECTptr;
    unsigned k = 0;
    while (k < S.size() && negative(S[k])) ++k;
    vecteur aa = *f[1]._VECTptr, ff = *f._VECTptr;
    aa[1] = S[k < S.size() ? k : 0];
    ff[1] = gen(aa, f[1].subtype);
    g = symbolic(at_of, gen(ff, f.subtype));
  }
  const gen & a = g._SYMBptr->feuille[1];
  if (a.type != _VECT || a._VECTptr->size() != 2 || a[0].type != _VECT || a[1].type != _VECT || a[0]._VECTptr->size() != a[1]._VECTptr->size()) return false;
  std::string m;
  for (unsigned i = 0; i < a[0]._VECTptr->size(); ++i) {
    const std::string v = a[0][i].print(contextptr);
    const char * un = "";
    if (i) m += ", ";
    for (int k = 0; k < rn; ++k)
      if (v == rv[k]) { m += rr[k]; m += ' '; un = ru[k]; break; }
    gen x = a[1][i]; // (10.0 from 0.25 and 0.1: 10)
#ifdef WITH_UNITS
    if (*un) { // (solved in the story's units: in the role's, 1/2 h is 30 min)
      std::string f;
      unitless(f, un);
      x = eval(x / gen(f, contextptr), 1, contextptr);
    }
#endif
    if (x.type == _FRAC && x._FRACptr->den.type == _INT_) { // (12/5 h: 2.4 h, as a textbook; 7/3 stays)
      int d = x._FRACptr->den.val;
      while (d % 2 == 0) d /= 2;
      while (d % 5 == 0) d /= 5;
      if (d == 1) x = evalf(x, 1, contextptr);
    }
    if (x.type == _DOUBLE_ && x._DOUBLE_val == (int)x._DOUBLE_val) x = (int)x._DOUBLE_val;
    m += v.c_str(); m += " = "; m += x.print(contextptr).c_str();
    if (*un) { m += ' '; for (; *un; ++un) if (*un != '_') m += *un; } // (km/h, not _km/_h)
    const gen y = evalf(x, 1, contextptr); // (5*sqrt(2) cm: then about 7.07107)
    if (y.type == _DOUBLE_ && x.type != _DOUBLE_ && x.type != _INT_ && x.type != _ZINT && x.type != _FRAC) { m += " \xe2\x89\x88 "; m += y.print(contextptr).c_str(); }
  }
  msg = m; // (letters without a role too: n = 18, not [n] = [18])
  rn = 0; // (used: the next story gives its letters their roles)
  return true;
}
#endif
