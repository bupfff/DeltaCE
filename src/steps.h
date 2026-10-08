// steps.h - step-by-step solutions (the steps build, FlowCE.b84; tools/hostgiac -S): textbook
// rules for derivatives, integrals, limits, sums and Taylor series. giac computes every result;
// the walk decides which rule explains it, and a chain is shown only once checked.
#ifndef STEPS_H
#define STEPS_H
#include <giac/giacPCH.h>

// A step: its rule (steps_title, steps_math), its depth (a part of the step above it) and the
// parts its math line shows, in the variable v.
struct step_t { unsigned char rule, depth; giac::gen a, b, c, v; };

// The steps of input (parsed, not evaluated): diff, integrate, limit, sum or series, or an
// expression containing them. false: none (the chain could not be explained, or did not check).
bool steps_of(const giac::gen & input, std::vector<step_t> & out, const giac::context * contextptr);

// The rule's title ("" for a step that continues the one above) and the formula it applies
// (0 for none); the step's math line (its rule's template filled with its parts).
const char * steps_title(const step_t & s);
const char * steps_formula(const step_t & s);
giac::gen steps_math(const step_t & s, const giac::context * contextptr);
#endif
