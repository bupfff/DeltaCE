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
extern giac::context * contextptr;

const char * phrase_story;
int phrase_bare;
// fact: 0 a question, 1 a story's fact, 2 find, 3 a question answered by an equation, 4 the wrapper
struct phr { const char * name; unsigned char na, fact; const char * words, * tmpl; };
static const phr PH[] = {
  // questions
  {"ph_value", 3, 0, "the value of\0when\0=\0", "subst(%1,%2=%3)"},
  {"ph_when", 2, 0, "when is\0=\0", "solve(%1=%2)"},
  {"ph_solvefor", 2, 0, "solve\0for\0", "solve(%1,%2)"},
  {"ph_zeros", 1, 0, "the zeros of\0", "solve(%1=0)"},
  {"ph_rate", 3, 0, "the rate of change of\0at\0=\0", "subst(diff(%1,%2),%2=%3)"},
  {"ph_avgrate", 4, 0, "the average rate of change of\0from\0=\0to\0", "(subst(%1,%2=%4)-subst(%1,%2=%3))/(%4-%3)"},
  {"ph_tangent", 3, 3, "the tangent line to\0at\0=\0", "y=subst(diff(%1,%2),%2=%3)*(%2-%3)+subst(%1,%2=%3)"},
  {"ph_area", 4, 0, "the area under\0from\0=\0to\0", "integrate(%1,%2,%3,%4)"},
  {"ph_slope", 4, 0, "the slope through (\0,\0) and (\0,\0)", "(%4-%2)/(%3-%1)"},
  {"ph_line", 4, 3, "the line through (\0,\0) and (\0,\0)", "y=(%4-%2)/(%3-%1)*(x-%1)+%2"},
  {"ph_distance", 4, 0, "the distance from (\0,\0) to (\0,\0)", "sqrt((%3-%1)^2+(%4-%2)^2)"},
  {"ph_midpoint", 4, 0, "the midpoint of (\0,\0) and (\0,\0)", "[(%1+%3)/2,(%2+%4)/2]"},
  {"ph_hypotenuse", 2, 0, "the hypotenuse with legs\0and\0", "sqrt(%1^2+%2^2)"},
  {"ph_percentof", 2, 0, "\0% of\0", "%1/100*%2"},
  {"ph_whatpercent", 2, 0, "what percent of\0is\0?", "100*%2/%1"},
  {"ph_increased", 2, 0, "\0increased by\0%", "%1*(1+%2/100)"},
  {"ph_decreased", 2, 0, "\0decreased by\0%", "%1*(1-%2/100)"},
  {"ph_interest", 4, 0, "\0at\0% compounded\0times a year for\0years", "round(evalf(%1*(1+%2/100/%3)^(%3*%4)),2)"},
  {"ph_atleastone", 2, 0, "the chance of at least one success in\0tries, each\0", "1-(1-%2)^%1"},
  // (2026-10-31) motion (the time it lands: the root after 0; g = 9.8 exact, solve refused -4.9),
  // shapes, sequences, money, rates
  {"ph_lands", 2, 0, "a ball thrown up at\0m/s from a height of\0m lands after", "evalf(max(solve(-49/10*t^2+%1*t+%2=0,t)))"},
  {"ph_landsft", 2, 0, "a ball thrown up at\0ft/s from a height of\0ft lands after", "max(solve(-16*t^2+%1*t+%2=0,t))"},
  {"ph_maxheight", 2, 0, "the highest a ball thrown up at\0m/s from a height of\0m goes", "evalf(%2+5*%1^2/98)"},
  {"ph_circlearea", 1, 0, "the area of a circle of radius\0", "pi*%1^2"},
  {"ph_circumference", 1, 0, "the circumference of a circle of radius\0", "2*pi*%1"},
  {"ph_cylinder", 2, 0, "the volume of a cylinder of radius\0and height\0", "pi*%1^2*%2"},
  {"ph_sphere", 1, 0, "the volume of a sphere of radius\0", "4*pi*%1^3/3"},
  {"ph_cone", 2, 0, "the volume of a cone of radius\0and height\0", "pi*%1^2*%2/3"},
  {"ph_triangle", 2, 0, "the area of a triangle with base\0and height\0", "%1*%2/2"},
  {"ph_leg", 2, 0, "the other leg of a right triangle with hypotenuse\0and leg\0", "sqrt(%1^2-%2^2)"},
  {"ph_thirdangle", 2, 0, "the third angle of a triangle with angles\0and\0", "180-%1-%2"},
  {"ph_arithmetic", 3, 0, "term\0of the arithmetic sequence starting at\0adding\0each time", "%2+(%1-1)*%3"},
  {"ph_geometric", 3, 0, "term\0of the geometric sequence starting at\0times\0each time", "%2*%3^(%1-1)"},
  {"ph_simpleinterest", 3, 0, "the simple interest on\0at\0% for\0years", "%1*%2/100*%3"},
  {"ph_together", 2, 0, "working together, one taking\0hours and one\0hours, take", "1/(1/%1+1/%2)"},
  {"ph_proportion", 3, 0, "if\0goes with\0then\0goes with", "%3*%2/%1"},
  {"ph_unitrate", 2, 0, "the unit rate of\0for\0", "%1/%2"},
  // a story's facts
  {"ph_is", 2, 1, "\0is\0", "%1=%2"},
  {"ph_more", 3, 1, "\0is\0more than\0", "%1=%3+%2"},
  {"ph_less", 3, 1, "\0is\0less than\0", "%1=%3-%2"},
  {"ph_times", 3, 1, "\0is\0times\0", "%1=%2*%3"},
  {"ph_percentis", 3, 1, "\0is\0% of\0", "%1=%2/100*%3"},
  {"ph_sum", 3, 1, "the sum of\0and\0is\0", "%1+%2=%3"},
  {"ph_difference", 3, 1, "the difference of\0and\0is\0", "%1-%2=%3"},
  {"ph_product", 3, 1, "the product of\0and\0is\0", "%1*%2=%3"},
  {"ph_perimeter", 3, 1, "a rectangle\0by\0has perimeter\0", "2*%1+2*%2=%3"},
  {"ph_rectarea", 3, 1, "a rectangle\0by\0has area\0", "%1*%2=%3"},
  {"ph_drt", 3, 1, "distance\0= rate\0times time\0", "%1=%2*%3"},
  {"ph_consecutive", 3, 1, "the sum of\0consecutive integers from\0is\0", "%1*%2+%1*(%1-1)/2=%3"},
  {"ph_find", 1, 2, "find\0", 0},
  {"ph_fact", 2, 4, "\0=\0", 0}, // (an equation, neither stored nor solved: drawn as one)
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
static bool namec(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }
// the kind (fact 1, find 2) of the first phrase in line, -1: none
static int first_kind(const char * line) {
  for (const char * p = strstr(line, "ph_"); p; p = strstr(p + 1, "ph_")) {
    if (p > line && namec(p[-1])) continue;
    int n = 0;
    while (namec(p[n])) ++n;
    const int i = lookup(p, n, -1);
    if (i >= 0) return PH[i].fact;
  }
  return -1;
}
bool phrase_is_fact(const char * line) { return first_kind(line) == 1; }
bool phrase_is_find(const char * line) { return first_kind(line) == 2; }

// find(v): [v, the story's other letters] = their values
static void find_of(std::string & out, const std::string & v) {
  if (!phrase_story || !*phrase_story) { out = "\"Tell the story first, a fact a line\""; return; }
  std::string st("["); st += phrase_story; st += ']';
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
  out = vars; out += "=(solve("; out += st; out += ','; out += vars; out += "))[0]";
}
bool phrase_expand(char * buf, int cap) {
  bool any = false;
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
    std::string r;
    // the line through two points with the same x (typed alike): x = that x, not a division by 0
    const char * tm = i == 9 /* ph_line */ && e[0] == e[2] && !strncmp(a[0], a[2], e[0]) ? "x=%1" : PH[i].tmpl;
    if (!tm) find_of(r, std::string(a[0], e[0]));
    else
      for (const char * t = tm; *t; ++t) {
        if (*t == '%' && t[1] >= '1' && t[1] <= '0' + na) { r += '('; r.append(a[t[1] - '1'], e[t[1] - '1']); r += ')'; ++t; }
        else r += *t;
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
  return any;
}
#endif
