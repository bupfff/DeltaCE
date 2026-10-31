// delta.h - DeltaCE's Δ key (prgm; the screen: focus_steps.cc focus_delta; on the PC: tools/hostgiac
// -D): what an input is (a number, a function, an equation, a curve, data, a matrix...) in a
// textbook's words, the facts a textbook asks about it, and the next moves. Its lines are the
// analysis' (analyze.h): the head, sentences, sections and rows; a function's domain, intercepts,
// symmetry and asymptotes come from analyze_of itself (analyze_brief).
#ifndef DELTA_H
#define DELTA_H
#include <giac/giacPCH.h>
#include "analyze.h"

// a next move: its label, the console line it runs, and how: 'r' run it, 's' run it and open its
// steps, 'g' graph it (cmd: what the graph screen draws), 'a' its analysis (cmd: the function)
struct dl_move { const char * label; std::string cmd; char how; };

// the facts of g (parsed as typed, not evaluated: 6/8 stays 6/8), its text as typed for the head;
// last: the answer shown (0: none), compared with g at the end (your answer and the book's); 0:
// done, else why not
const char * delta_of(const giac::gen & g, const char * typed, const char * last, std::vector<an_line> & out, std::vector<dl_move> & moves, const giac::context * contextptr);
#endif
