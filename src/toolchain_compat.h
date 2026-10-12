// toolchain_compat.h: included first in every C++ file (makefile: -include). For the CE C
// toolchain v15 (clang 19) only: empty with v14.2.
// giac renames std to ustl (#define std ustl, config.h). v14.2's <cstdlib>, <cmath> and <cstdio>
// declared the C library in namespace std each time they were included, so after the rename in
// ustl. v15's wrappers declare it once, with the C header, and <math.h> brings <type_traits>: under
// the rename its std::swap went into ustl and clashed with ustl::swap, and a file that had included
// <stdlib.h> before the rename had no ustl::rand or ustl::abs. Here the real std gets them first,
// and ustl gets the C library names it had with v14.2.
#if defined(__cplusplus) && defined(__clang_major__) && __clang_major__ >= 19
#include <type_traits>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
namespace ustl {
using ::std::FILE; using ::std::_Exit; using ::std::abort; using ::std::abs; using ::std::acos;
using ::std::acosf; using ::std::acosh; using ::std::acoshf; using ::std::acoshl;
using ::std::acosl; using ::std::asin; using ::std::asinf; using ::std::asinh; using ::std::asinhf;
using ::std::asinhl; using ::std::asinl; using ::std::asprintf; using ::std::at_quick_exit;
using ::std::atan; using ::std::atan2; using ::std::atan2f; using ::std::atan2l; using ::std::atanf;
using ::std::atanh; using ::std::atanhf; using ::std::atanhl; using ::std::atanl;
using ::std::atexit; using ::std::atof; using ::std::atoi; using ::std::atol; using ::std::atoll;
using ::std::bsearch; using ::std::calloc; using ::std::cbrt; using ::std::cbrtf;
using ::std::cbrtl; using ::std::ceil; using ::std::ceilf; using ::std::ceill;
using ::std::clearerr; using ::std::copysign; using ::std::copysignf; using ::std::copysignl;
using ::std::cos; using ::std::cosf; using ::std::cosh; using ::std::coshf; using ::std::coshl;
using ::std::cosl; using ::std::cospi; using ::std::cospif; using ::std::cospil; using ::std::div;
using ::std::double_t; using ::std::erf; using ::std::erfc; using ::std::erfcf; using ::std::erfcl;
using ::std::erff; using ::std::erfl; using ::std::exit; using ::std::exp; using ::std::exp2;
using ::std::exp2f; using ::std::exp2l; using ::std::expf; using ::std::expl; using ::std::expm1;
using ::std::expm1f; using ::std::expm1l; using ::std::fabs; using ::std::fabsf; using ::std::fabsl;
using ::std::fclose; using ::std::fdim; using ::std::fdimf; using ::std::fdiml; using ::std::feof;
using ::std::ferror; using ::std::fflush; using ::std::fgetc; using ::std::fgets;
using ::std::float_t; using ::std::floor; using ::std::floorf; using ::std::floorl;
using ::std::fma; using ::std::fmaf; using ::std::fmal; using ::std::fmax; using ::std::fmaxf;
using ::std::fmaxl; using ::std::fmin; using ::std::fminf; using ::std::fminl; using ::std::fmod;
using ::std::fmodf; using ::std::fmodl; using ::std::fopen; using ::std::fpclassify;
using ::std::fprintf; using ::std::fputc; using ::std::fputs; using ::std::fread; using ::std::free;
using ::std::freopen; using ::std::frexp; using ::std::frexpf; using ::std::frexpl;
using ::std::fseek; using ::std::ftell; using ::std::fwrite; using ::std::getc;
using ::std::getchar; using ::std::hypot; using ::std::hypotf; using ::std::hypotl;
using ::std::i48abs; using ::std::i48div; using ::std::ilogb; using ::std::ilogbf;
using ::std::ilogbl; using ::std::isfinite; using ::std::isgreater; using ::std::isgreaterequal;
using ::std::isinf; using ::std::isless; using ::std::islessequal; using ::std::islessgreater;
using ::std::isnan; using ::std::isnormal; using ::std::issignaling; using ::std::issubnormal;
using ::std::isunordered; using ::std::iszero; using ::std::labs; using ::std::ldexp;
using ::std::ldexpf; using ::std::ldexpl; using ::std::ldiv; using ::std::lgamma;
using ::std::lgammaf; using ::std::lgammal; using ::std::llabs; using ::std::lldiv;
using ::std::llrint; using ::std::llrintf; using ::std::llrintl; using ::std::llround;
using ::std::llroundf; using ::std::llroundl; using ::std::log; using ::std::log10;
using ::std::log10f; using ::std::log10l; using ::std::log1p; using ::std::log1pf;
using ::std::log1pl; using ::std::log2; using ::std::log2f; using ::std::log2l; using ::std::logb;
using ::std::logbf; using ::std::logbl; using ::std::logf; using ::std::logl; using ::std::lrint;
using ::std::lrintf; using ::std::lrintl; using ::std::lround; using ::std::lroundf;
using ::std::lroundl; using ::std::malloc; using ::std::modf; using ::std::modff;
using ::std::modfl; using ::std::nan; using ::std::nanf; using ::std::nanl; using ::std::nearbyint;
using ::std::nearbyintf; using ::std::nearbyintl; using ::std::nextafter; using ::std::nextafterf;
using ::std::nextafterl; using ::std::nextdown; using ::std::nextdownf; using ::std::nextdownl;
using ::std::nexttoward; using ::std::nexttowardf; using ::std::nexttowardl; using ::std::nextup;
using ::std::nextupf; using ::std::nextupl; using ::std::on_exit; using ::std::perror;
using ::std::pow; using ::std::powf; using ::std::powl; using ::std::printf; using ::std::putc;
using ::std::putchar; using ::std::puts; using ::std::qsort; using ::std::quick_exit;
using ::std::rand; using ::std::realloc; using ::std::remainder; using ::std::remainderf;
using ::std::remainderl; using ::std::remove; using ::std::remquo; using ::std::remquof;
using ::std::remquol; using ::std::rename; using ::std::rewind; using ::std::rint;
using ::std::rintf; using ::std::rintl; using ::std::round; using ::std::roundeven;
using ::std::roundevenf; using ::std::roundevenl; using ::std::roundf; using ::std::roundl;
using ::std::scalbln; using ::std::scalblnf; using ::std::scalblnl; using ::std::scalbn;
using ::std::scalbnf; using ::std::scalbnl; using ::std::signbit; using ::std::sin;
using ::std::sinf; using ::std::sinh; using ::std::sinhf; using ::std::sinhl; using ::std::sinl;
using ::std::sinpi; using ::std::sinpif; using ::std::sinpil; using ::std::size_t;
using ::std::snprintf; using ::std::sprintf; using ::std::sqrt; using ::std::sqrtf;
using ::std::sqrtl; using ::std::srand; using ::std::sscanf; using ::std::strtod;
using ::std::strtof; using ::std::strtol; using ::std::strtold; using ::std::strtoll;
using ::std::strtoul; using ::std::strtoull; using ::std::tan; using ::std::tanf; using ::std::tanh;
using ::std::tanhf; using ::std::tanhl; using ::std::tanl; using ::std::tanpi; using ::std::tanpif;
using ::std::tanpil; using ::std::tgamma; using ::std::tgammaf; using ::std::tgammal;
using ::std::trunc; using ::std::truncf; using ::std::truncl; using ::std::ungetc;
using ::std::vasprintf; using ::std::vfprintf; using ::std::vprintf; using ::std::vsnprintf;
using ::std::vsprintf; using ::std::vsscanf;
}
#endif
