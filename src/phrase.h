// phrase.h - DeltaCE's phrases (phrase.cc): a word problem typed the way the textbook says it. A
// phrase is a call ph_name(a,b,...) drawn as its words with the arguments in boxes ("the value of
// [] when [] = []": mathinput.cc, through mi_phrase), put into giac by the input pre-pass
// (answer_prepass) before it is evaluated. A fact of a story (an equation: "[l] is [3] more than
// [2w]") shows its translation as its answer; "find [w]" solves the story told since the last find.
#ifndef PHRASE_H
#define PHRASE_H

// the phrase called name[0,nlen) with na arguments: its words, na + 1 pieces each ending in a 0
// (an empty piece: nothing there), else 0
const char * phrase_words(const char * name, int nlen, int na);
// the phrases in buf (cap bytes) put into giac, innermost first; false: none there
bool phrase_expand(char * buf, int cap);
// line (as typed) states a fact of a story (its phrase is an equation)
bool phrase_is_fact(const char * line);
// line asks for the story's unknowns (find)
bool phrase_is_find(const char * line);
// the story's facts, in giac ("l=2*w+3,2*l+2*w=36"), for the next find: the console sets it from
// its history before the line is put into giac (0: none)
extern const char * phrase_story;
// nonzero: an equation is put into giac bare, not in ph_fact(...) (a story's facts, for find)
extern int phrase_bare;
#endif
