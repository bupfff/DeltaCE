// tutor.h - DeltaCE's "Your turn" (the screen: focus_tutor.cc; on the PC: tools/hostgiac -Y): the
// student solves a problem line by line, and each line is checked against the solution the steps
// engine found (steps.cc). Any correct line counts, not only the book's; skipping to the answer
// counts too. A wrong line gets a reason: a number that works in one and not the other, and, when it
// is plain, the slip (one side, a sign, the arithmetic). Hints come from the book's next step: its
// rule's name, its formula, then its line.
// Problems: solve(equation,x) (linear, quadratic, rational, radical, polynomial), solve([equations],
// [x,y]) (linear systems), factor(polynomial), diff(f,x).
#ifndef TUTOR_H
#define TUTOR_H
#include <giac/giacPCH.h>
#include <string>

// the problem, as typed (solve(3(x-2)+5=14,x)); false: not one the tutor takes (no steps, no
// solution, infinitely many, another kind)
bool tutor_start(const char * cmd, const giac::context * contextptr);
// the problem as the page shows it (3*(x-2)+5=14), and what is asked ("Solve for x")
const std::string & tutor_problem();
const char * tutor_ask();

// a line typed. ok: 1 right, 0 wrong, -1 not read (not math, another letter: nothing counted);
// solved: the answer is reached; msg: the rule's name, or why it is wrong (lines split by \n)
struct tutor_res { signed char ok, solved; std::string msg; };
tutor_res tutor_check(const char * line, const giac::context * contextptr);

// the next hint, one level a press: 1 the next rule's name, 2 its formula (when it has one), 3 its
// line(s), added to *shown as given lines (the student goes on from there). Returns the level
// given, 0 when nothing is left (solved)
int tutor_hint(std::string & text, std::vector<std::string> & shown, const giac::context * contextptr);

// the summary: lines typed right, wrong, hints taken, the book's steps
void tutor_counts(int & right, int & wrong, int & hints, int & book);
#endif
