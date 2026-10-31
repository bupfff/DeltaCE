// delta.cc - DeltaCE's Δ key (delta.h): what the input is and its facts, as a textbook states them.
// The input is read from its shape, as typed: a list of equations is a system, a list of lists a
// matrix, a list of numbers data; a relation in one letter an equation or an inequality, in x and y
// a curve (a line, a circle, an ellipse, a hyperbola, a parabola: completing the squares); no letter
// a number (an integer, a fraction, a decimal, a complex number); one letter a function (linear,
// quadratic, polynomial, rational, exponential, logarithmic, a sinusoid, a root, an absolute value),
// with analyze_of's domain, intercepts, symmetry and asymptotes after its own facts.
// Every value comes from giac; the words around them are fixed (no claim is guessed: a function is
// called a sinusoid only when it is A sin(Bx + C) + D exactly).
// Built for size: the facts are formulas in giac syntax (tables of label, formula), parsed and
// evaluated when shown, in names given values first (let: qa, qb... and qx, qy for the letters);
// compiled, each formula cost ten times its text. The values go in as text before parsing (giac's
// subst of the names took 110 ms a formula on the calculator, the parse 40). The formulas use these commands by name, which
// must stay typable (static_lexer.h): sqrt abs re im conj stddev sum mean det rank
// trace inv lcm gcd denom dot.
#if defined FLOWCE_STEPS || defined HOSTGIAC
#include <giac/giacPCH.h> // first: its "#define std ustl"
#include "delta.h"
using namespace giac;
#define NOINL __attribute__((noinline))

static const context * ctx_;
#define CTX ctx_
static std::vector<an_line> * out_;
static std::vector<dl_move> * mv_;
static std::vector<const char *> * vn_; // the formulas' names...
static std::vector<std::string> * vs_;  // ...and their values, as text

static NOINL bool stop(){ return ctrl_c || interrupted; }
static NOINL gen ev(const gen & g){ return eval(g,1,CTX); }
static NOINL std::string P(const gen & g){ // (a matrix prints a row a line: one line here)
  std::string s=g.print(CTX),r;
  for (unsigned i=0;i<s.size();++i) if (s[i]!='\n') r+=s[i];
  return r;
}
static NOINL void line(int kind,const char * t,const std::string & m,int sep=AN_COMMA){
  an_line l; l.kind=(unsigned char)kind; l.sep=(unsigned char)sep; l.text=t; l.math=m; out_->push_back(l);
}
static NOINL void row(const char * t,const gen & g){ line(AN_ROW,t,P(g)); }
static NOINL void say(const char * t){ line(AN_ROW,t,std::string()); }           // a sentence
static NOINL void sec(const char * t){ line(AN_SECTION,t,std::string()); }
static NOINL void words(const char * t,const char * w){ std::string m("\1"); m+=w; line(AN_ROW,t,m); }
static NOINL void item(std::string & m,const std::string & s){ if (!m.empty()) m+='\n'; m+=s; }
static NOINL void move(const char * label,const std::string & cmd,char how){ dl_move d; d.label=label; d.cmd=cmd; d.how=how; mv_->push_back(d); }
// a move: pre + g (evaluated: giac prints the parsed tree with every group) + post
static NOINL void mv(const char * label,const char * pre,const gen & g,const char * post,char how){
  std::string c(pre); c+=P(ev(g)); c+=post; move(label,c,how);
}
static NOINL std::string eqs(const gen & a,const gen & b){ std::string s=P(a); s+='='; s+=P(b); return s; } // a=b
static NOINL std::string Pi(const gen & g){ return g==plus_inf?std::string("oo"):g==minus_inf?std::string("-oo"):P(g); } // oo: the renderer's infinity
static NOINL std::string pt(const gen & a,const gen & b){ std::string s("("); s+=P(a); s+=','; s+=P(b); s+=')'; return s; }
static NOINL bool num(const gen & g,double & d){
  gen e=evalf_double(ev(g),1,CTX);
  if (e.type!=_DOUBLE_) return false;
  d=e._DOUBLE_val; return true;
}
static NOINL int sgn(const gen & g){ double d; if (!num(g,d)) return 2; return d>1e-12?1:d<-1e-12?-1:0; }
static NOINL bool is_op(const gen & g,const unary_function_ptr * op){ return g.is_symb_of_sommet(op); }
static NOINL bool with(const gen & g,const gen & x){
  vecteur v=lidnt(g);
  for (unsigned i=0;i<v.size();++i) if (v[i]==x) return true;
  return false;
}
// the letters of g (identifiers without a value: not pi, not e)
static NOINL vecteur letters(const gen & g){
  vecteur ids=lidnt(g),r;
  for (unsigned i=0;i<ids.size();++i) if (evalf(ids[i],1,CTX).type!=_DOUBLE_) r.push_back(ids[i]);
  return r;
}
static NOINL gen at(const gen & f,const gen & x,const gen & v){ return ev(subst(f,x,v,false,CTX)); }
static NOINL gen co(const gen & f,const gen & x,int k){ return ev(_coeff(makesequence(f,x,k),CTX)); }
static NOINL gen nrm(const gen & g){ return simplify(ev(g),CTX); } // 2 pi/2: pi

// ---------------------------------------------------------------- the formulas
static NOINL void let(const char * n,const gen & v){
  for (unsigned i=0;i<vn_->size();++i) if (!strcmp((*vn_)[i],n)){ (*vs_)[i]=P(v); return; }
  vn_->push_back(n); vs_->push_back(P(v));
}
static NOINL bool idc(char c){ return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_'; }
// the formula f[0,n) in let's names: its value (~ first: simplified)
static NOINL gen fx(const char * f,int n=-1){
  const bool s=*f=='~';
  if (s){ ++f; if (n>0) --n; }
  const char * e=n<0?f+strlen(f):f+n;
  std::string t;
  for (const char * p=f;p<e;){
    if (!idc(*p) || (*p>='0' && *p<='9')){ t+=*p++; continue; }
    const char * q=p;
    while (q<e && idc(*q)) ++q;
    int k=-1;
    for (unsigned i=0;i<vn_->size();++i) if ((int)strlen((*vn_)[i])==q-p && !strncmp((*vn_)[i],p,q-p)) k=i;
    if (k>=0){ t+='('; t+=(*vs_)[k]; t+=')'; } else t.append(p,q-p);
    p=q;
  }
  gen g(t,CTX);
  g=ev(g);
  return s?simplify(g,CTX):g;
}
// an item's text: a point (pts: x,y), an equation (each side evaluated: y=(-2+x) printed its group),
// else the value
static NOINL std::string fs(const char * f,int n,bool pts){
  if (pts){ std::string s("["); s.append(f,n); s+=']'; gen v=fx(s.c_str()); return v.type==_VECT && v._VECTptr->size()==2?pt(v[0],v[1]):P(v); }
  int d=0;
  for (int i=0;i<n;++i){
    const char c=f[i];
    if (c=='(' || c=='[') ++d; else if (c==')' || c==']') --d;
    else if (c=='=' && !d && i && f[i-1]!='<' && f[i-1]!='>' && f[i-1]!='!' && f[i-1]!='=' && f[i+1]!='=') return eqs(fx(f,i),fx(f+i+1,n-i-1));
  }
  return P(fx(f,n));
}
// facts: label, formula pairs (0 ends); ';' separates a fact's items; '@' first: points x,y; '|'
// first: items joined by "or"
static NOINL void facts(const char * const * t){
  for (;*t && !stop();t+=2){
    const char * f=t[1];
    const bool pts=*f=='@',orr=*f=='|';
    if (pts || orr) ++f;
    std::string m;
    for (;;){
      const char * e=strchr(f,';');
      item(m,fs(f,e?e-f:strlen(f),pts));
      if (!e) break;
      f=e+1;
    }
    line(AN_ROW,t[0],m,orr?AN_OR:AN_COMMA);
  }
}
static NOINL void fact(const char * label,const char * f){ const char * t[3]={label,f,0}; facts(t); }

// ---------------------------------------------------------------- reading the form typed
// f's kernels with x in them, as giac's lvar lists them (x, sqrt(x+4), 2^x, abs(x-2))
static NOINL vecteur kx(const gen & f,const gen & x){
  vecteur l=lvar(f),r;
  for (unsigned i=0;i<l.size();++i) if (with(l[i],x)) r.push_back(l[i]);
  return r;
}
// the denominators with x in g as typed ((x^2-4)/(x-2): giac's evaluation cancels it)
static NOINL void dens(const gen & g,const gen & x,vecteur & out){
  if (g.type!=_SYMB) return;
  if (is_op(g,at_inv) && with(g,x)){ out.push_back(g._SYMBptr->feuille); return; }
  const gen & f=g._SYMBptr->feuille;
  if (is_op(g,at_division) && f.type==_VECT && f._VECTptr->size()==2 && with(f[1],x)){ dens(f[0],x,out); out.push_back(f[1]); return; }
  if (f.type!=_VECT){ dens(f,x,out); return; }
  for (unsigned i=0;i<f._VECTptr->size();++i) dens((*f._VECTptr)[i],x,out);
}
static NOINL bool divides_by(const gen & g,const gen & x){ vecteur v; dens(g,x,v); return !v.empty(); }
// g has no product of a sum and no power of a sum: nothing to expand
static NOINL bool expanded_already(const gen & g){
  if (g.type!=_SYMB) return true;
  const gen & f=g._SYMBptr->feuille;
  if (is_op(g,at_pow) && f.type==_VECT && is_op(f[0],at_plus)) return false;
  if (is_op(g,at_prod) && f.type==_VECT) for (unsigned i=0;i<f._VECTptr->size();++i) if (is_op((*f._VECTptr)[i],at_plus)) return false;
  if (f.type!=_VECT) return expanded_already(f);
  for (unsigned i=0;i<f._VECTptr->size();++i) if (!expanded_already((*f._VECTptr)[i])) return false;
  return true;
}
// the degree of f in x when f is a polynomial in x, else -1 (a quotient of polynomials: -2)
static NOINL int pdeg(const gen & f,const gen & x){
  vecteur k=kx(f,x);
  if (k.size()>1 || (k.size()==1 && k[0]!=x)) return -1;
  if (with(ev(_denom(f,CTX)),x)) return -2;
  gen d=_degree(makesequence(ev(_numer(f,CTX)),x),CTX);
  return d.type==_INT_?d.val:-1;
}
// f's one kernel in x (sin(3x), exp(2x), sqrt(x+1)...) when f has only that one, else 0
static NOINL gen kernel(const gen & f,const gen & x){
  vecteur k=kx(f,x);
  return k.size()==1 && k[0]!=x?k[0]:gen(0);
}
// f linear in its kernel u: f = A u + D (A, D free of x); false if not
static NOINL bool linear_in(const gen & f,const gen & u,const gen & x,gen & A,gen & D){
  D=ev(subst(f,u,0,false,CTX));
  A=ev(subst(f,u,1,false,CTX)-D);
  if (with(A,x) || with(D,x) || is_zero(A)) return false;
  return is_zero(ev(subst(f,u,2,false,CTX)-D-2*A));
}
// the one sin, cos or tan of x in g as typed (giac turns sin(3x - pi) into -sin(3x): the phase
// shift would be lost), else 0
static NOINL gen trig_node(const gen & g,const gen & x){
  if (g.type!=_SYMB) return 0;
  if ((is_op(g,at_sin) || is_op(g,at_cos) || is_op(g,at_tan)) && with(g,x)) return g;
  const gen & f=g._SYMBptr->feuille;
  if (f.type!=_VECT) return trig_node(f,x);
  gen r=0;
  for (unsigned i=0;i<f._VECTptr->size();++i){ gen t=trig_node((*f._VECTptr)[i],x); if (!is_zero(t)){ if (!is_zero(r)) return 0; r=t; } }
  return r;
}
static NOINL const char * op_name(const gen & g){ return g.type==_SYMB?g._SYMBptr->sommet.ptr()->s:""; }

// ---------------------------------------------------------------- numbers
// g as a decimal, rounded to nd places, its trailing zeros left out (the calculator's doubles are
// 32-bit floats: 7.6 rounded and printed was 7.60001): from the whole number of its 10^-nd
static NOINL std::string dec(const gen & g,int nd=4){
  gen sc=1;
  for (int i=0;i<nd;++i) sc=sc*10;
  std::string n=P(_floor(ev(evalf(g,1,CTX)*sc+gen(0.5)),CTX)),s;
  const bool neg=n[0]=='-';
  if (neg) n=n.substr(1,n.size()-1);
  while ((int)n.size()<=nd) n.insert(0,"0");
  s=n.substr(0,n.size()-nd); s+='.'; s+=n.substr(n.size()-nd,nd);
  while (s[s.size()-1]=='0') s.erase(s.size()-1,1); // (uSTL: erase and substr with their counts)
  if (s[s.size()-1]=='.') s.erase(s.size()-1,1);
  if (neg && s!="0") s.insert(0,"-");
  return s;
}
static NOINL std::string approx(const gen & g){ std::string m("\n\1\xe2\x89\x88\n"); m+=dec(g); return m; } // ≈ 7.6
// q as a percent: whole, else its decimal (7/3: 233.3333%)
static NOINL void percent(const gen & q){
  gen p=ev(q*100);
  std::string m=p.type==_INT_ || p.type==_ZINT?P(p):dec(p); m+="%";
  line(AN_ROW,"Percent",m);
}
static NOINL void integer(const gen & n){
  const gen a=abs(n,CTX);
  const bool unit=is_zero(a) || a==1,prime=!unit && is_one(_isprime(a,CTX));
  say(unit?"Neither prime nor composite.":!prime?"A composite number.":sgn(n)>0?"A prime number.":"The opposite of a prime number.");
  words("Parity",is_zero(ev(_irem(makesequence(a,2),CTX)))?"even":"odd");
  if (!unit){
    const gen pf=_ifactor(a,CTX);
    if (!prime) row("Prime factorization",pf);
    // the divisors; their number from the prime factors (p^e: e+1 choices) when giac won't list
    // them (30!: [an error])
    gen d=_idivis(a,CTX),c=1;
    if (d.type==_VECT && !d._VECTptr->empty() && d._VECTptr->front().type==_STRNG) d=0;
    const vecteur fs=is_op(pf,at_prod) && pf._SYMBptr->feuille.type==_VECT?*pf._SYMBptr->feuille._VECTptr:vecteur(1,pf);
    for (unsigned i=0;i<fs.size();++i) c=c*(is_op(fs[i],at_pow)?fs[i]._SYMBptr->feuille[1]+1:gen(2));
    row("Number of divisors",d.type==_VECT?gen(int(d._VECTptr->size())):ev(c));
    if (d.type==_VECT){
      if (d._VECTptr->size()<=24){
        std::string m;
        for (unsigned i=0;i<d._VECTptr->size();++i) item(m,P((*d._VECTptr)[i]));
        line(AN_ROW,"Divisors",m);
      }
    }
  }
  if (sgn(n)>=0){
    gen r=ev(sqrt(n,CTX));
    if (r.type==_INT_ || r.type==_ZINT){ std::string m=P(r); m+="^2="; m+=P(n); line(AN_ROW,"A perfect square",m); }
    else row("Square root",r);
  }
}
static NOINL void rational(const gen & typed,const gen & q){
  const gen p=ev(_numer(q,CTX)),d=ev(_denom(q,CTX));
  say("A fraction.");
  if (P(typed)!=P(q)) row("Simplest form",q);
  gen dd=d; // terminating: only 2s and 5s in the denominator
  for (int k=2;k<=5;k+=3) while (is_zero(ev(_irem(makesequence(dd,k),CTX)))) dd=ev(dd/k);
  line(AN_ROW,dd==1?"Decimal (terminating)":"Decimal (repeating)",dec(q,6));
  percent(q);
  if (is_strictly_greater(abs(p,CTX),d,CTX)){ // 7/3: 2 1/3
    gen w=ev(_iquo(makesequence(abs(p,CTX),d),CTX)),r=ev(_irem(makesequence(abs(p,CTX),d),CTX));
    std::string m("\1"); if (sgn(q)<0) m+="\xe2\x88\x92"; m+=P(w); m+=' '; m+=P(r); m+='/'; m+=P(d);
    line(AN_ROW,"Mixed number",m);
  }
  let("qq",q);
  fact("Reciprocal","1/qq");
}
static NOINL void real_number(const gen & typed,const gen & v){
  if (P(typed).find('.')!=std::string::npos){ // a decimal: its fraction
    gen q=exact(v,CTX);
    say("A decimal.");
    if (q.type==_FRAC || q.type==_INT_ || q.type==_ZINT){ row("As a fraction",q); percent(q); }
    return;
  }
  say("A real number in exact form.");
  if (P(typed)!=P(v)) row("Simplified",v);
  line(AN_ROW,"Decimal",dec(v,6));
}
static const char * const T_cx[]={"Real part","re(qz)","Imaginary part","im(qz)","Modulus","abs(qz)",0};
static NOINL void complex_number(const gen & v){
  let("qz",v);
  say("A complex number.");
  facts(T_cx);
  const gen t=ev(arg(v,CTX));
  { std::string m=P(t); if (m.find("atan")!=std::string::npos) m+=approx(t); line(AN_ROW,"Argument",m,AN_SPACE); }
  row("Polar form",symbolic(at_prod,makesequence(ev(abs(v,CTX)),symbolic(at_exp,symbolic(at_prod,makesequence(cst_i,t))))));
  fact("Conjugate","conj(qz)");
  const int sa=sgn(re(v,CTX)),sb=sgn(im(v,CTX));
  words("Quadrant",sb==0?"on the real axis":sa==0?"on the imaginary axis":sa>0?(sb>0?"I":"IV"):(sb>0?"II":"III"));
}

// ---------------------------------------------------------------- functions of one letter
static NOINL void end_behavior(const gen & lead,int n){
  const int s=sgn(lead),l=n%2?-s:s;
  words("As x \xe2\x86\x92 \xe2\x88\x9e",s>0?"f(x) \xe2\x86\x92 \xe2\x88\x9e":"f(x) \xe2\x86\x92 \xe2\x88\x92\xe2\x88\x9e");
  words("As x \xe2\x86\x92 \xe2\x88\x92\xe2\x88\x9e",l>0?"f(x) \xe2\x86\x92 \xe2\x88\x9e":"f(x) \xe2\x86\x92 \xe2\x88\x92\xe2\x88\x9e");
}
// factored form, when giac factors f over the rationals (a product it was not typed as)
static NOINL void factored(const gen & f){
  gen fa=factor(f,false,CTX);
  if ((is_op(fa,at_prod) || is_op(fa,at_pow) || is_op(fa,at_neg)) && P(fa)!=P(f)) row("Factored form",fa);
}
// y op k as text (a range, a midline: y not evaluated)
static NOINL void yrow(const char * label,const char * op,const char * f){ std::string m("y"); m+=op; m+=P(fx(f)); line(AN_ROW,label,m); }
// ---------------------------------------------------------------- the textbook's sections, fast
// For polynomials, rational functions and sinusoids the domain, intercepts, symmetry and
// asymptotes are found the textbook's way (no limits, no sign charts: analyze_of took 1 to 3.6 s
// on the calculator for them), worded as analyze_of words them.
// the real solutions of p = 0 in x, in order
static NOINL vecteur roots_of(const gen & p,const gen & x){
  gen s=_solve(makesequence(symbolic(at_equal,makesequence(p,0)),x),CTX);
  vecteur r; std::vector<double> d;
  if (s.type==_VECT) for (unsigned i=0;i<s._VECTptr->size();++i){
    const gen & e=(*s._VECTptr)[i];
    double t;
    if (with(e,x) || !num(e,t)) continue;
    unsigned j=0;
    while (j<d.size() && d[j]<t) ++j;
    d.insert(d.begin()+j,t); r.insert(r.begin()+j,e);
  }
  return r;
}
static NOINL void x_ints(const vecteur & r){ std::string m; for (unsigned i=0;i<r.size();++i) item(m,pt(r[i],0)); if (m.empty()) m="\1none"; line(AN_ROW,"x-intercepts",m); }
static const char * const SYM[3]={"Even: symmetric about the y-axis","Odd: symmetric about the origin","Neither even nor odd"};
// even (0), odd (1) or neither (2): f at three points and their opposites
static NOINL int parity(const gen & f,const gen & x){
  static const double t[3]={0.37,1.29,2.13};
  bool e=true,o=true;
  for (int i=0;i<3;++i){
    double a,b;
    if (!num(at(f,x,t[i]),a) || !num(at(f,x,-t[i]),b)) return 2;
    const double m=1e-6*(1+(a<0?-a:a));
    if (a-b>m || b-a>m) e=false;
    if (a+b>m || -a-b>m) o=false;
  }
  return e?0:o?1:2;
}
static NOINL void head_sections(const gen & f,const gen & x,bool all){
  sec("DOMAIN"); if (all) words("","all real numbers");
}
static NOINL void poly_sections(const gen & f,const gen & x,int n){
  head_sections(f,x,true);
  sec("INTERCEPTS");
  line(AN_ROW,"y-intercept",pt(0,co(f,x,0)));
  vecteur r;
  if (n==1) r.push_back(ev(-co(f,x,0)/co(f,x,1)));
  else if (n==2){ // by the formula (solve took longer), the smaller first
    const gen a=co(f,x,2),b=co(f,x,1),c=co(f,x,0),d=ev(b*b-4*a*c);
    const int sd=sgn(d);
    if (sd==0) r.push_back(ev(-b/(2*a)));
    else if (sd>0){ gen u=nrm((-b-sqrt(d,CTX))/(2*a)),v=nrm((-b+sqrt(d,CTX))/(2*a)); if (sgn(a)<0) std::swap(u,v); r.push_back(u); r.push_back(v); }
  }
  else if (n>2) r=roots_of(f,x);
  x_ints(r);
  bool ev=true,od=true; // the powers there
  for (int k=0;k<=n;++k) if (!is_zero(co(f,x,k))){ if (k%2) ev=false; else od=false; }
  sec("SYMMETRY"); say(SYM[ev?0:od?1:2]);
  sec("ASYMPTOTES"); say("None");
}
// a rational function: the denominator as typed (its zeros are out of the domain), the factor it
// shares with the numerator (holes), the rest (vertical asymptotes), the degrees (the horizontal
// or slant asymptote)
static NOINL void rational_sections(const gen & typed,const gen & f,const gen & x){
  vecteur dv; dens(typed,x,dv);
  gen Q=1;
  for (unsigned i=0;i<dv.size();++i) Q=Q*dv[i];
  Q=dv.empty()?ev(_denom(f,CTX)):normal(ev(Q),CTX);
  const gen N=normal(ev(f*Q),CTX),g=ev(_gcd(makesequence(N,Q),CTX)),N2=normal(ev(N/g),CTX),Q2=normal(ev(Q/g),CTX);
  const vecteur out=roots_of(Q,x);
  sec("DOMAIN");
  if (out.empty()) words("","all real numbers");
  else { std::string m; for (unsigned i=0;i<out.size();++i){ std::string t=P(x); t+="!="; t+=P(out[i]); item(m,t); } line(AN_ROW,"",m); }
  sec("INTERCEPTS");
  if (is_zero(at(Q,x,0))) words("y-intercept","none"); else line(AN_ROW,"y-intercept",pt(0,at(f,x,0)));
  x_ints(roots_of(N2,x));
  sec("SYMMETRY"); say(SYM[parity(f,x)]);
  sec("ASYMPTOTES");
  int k=0;
  { const vecteur va=roots_of(Q2,x); std::string m; for (unsigned i=0;i<va.size();++i) item(m,eqs(x,va[i])); if (!m.empty()){ line(AN_ROW,"Vertical",m); ++k; } }
  const gen dn=_degree(makesequence(N2,x),CTX),dq=_degree(makesequence(Q2,x),CTX);
  if (dn.type==_INT_ && dq.type==_INT_ && dq.val>0){ // (all of it cancelled: a polynomial with holes, no asymptote)
    const int a=dn.val,b=dq.val;
    std::string m("y=");
    if (a<b){ m+='0'; line(AN_ROW,"Horizontal",m); ++k; }
    else if (a==b){ m+=P(ev(_lcoeff(makesequence(N2,x),CTX)/_lcoeff(makesequence(Q2,x),CTX))); line(AN_ROW,"Horizontal",m); ++k; }
    else if (a==b+1){ m+=P(ev(_quo(makesequence(N2,Q2,x),CTX))); line(AN_ROW,"Slant",m); ++k; }
  }
  { const vecteur h=roots_of(g,x); const gen s=ev(N2/Q2); std::string m; for (unsigned i=0;i<h.size();++i) item(m,pt(h[i],nrm(at(s,x,h[i])))); if (!m.empty()){ line(AN_ROW,"Hole",m); ++k; } } // (its value: the simplified one's)
  if (!k) say("None");
}
// sin or cos: every real x, its y-intercept, its symmetry, no asymptote (its zeros: a move)
static NOINL void sinus_sections(const gen & f,const gen & x){
  head_sections(f,x,true);
  sec("INTERCEPTS"); line(AN_ROW,"y-intercept",pt(0,at(f,x,0)));
  sec("SYMMETRY"); say(SYM[parity(f,x)]);
  sec("ASYMPTOTES"); say("None");
}
static const char * const T_quad[]={"Vertex","@-qb/(2*qa),qc-qb^2/(4*qa)","Axis of symmetry","qx=-qb/(2*qa)",0};
static const char * const T_quad2[]={"Discriminant","qb^2-4*qa*qc",0};
static const char * const T_quad3[]={"Vertex form","qa*(qx+qb/(2*qa))^2+qk",0};
static NOINL void function_of(const gen & typed,const gen & f0,const gen & x){
  gen f=ev(f0);
  int n=pdeg(f,x);
  if (n>=0 && divides_by(typed,x)) n=-3; // (x^2-4)/(x-2): rational as typed, x+2 once simplified
  gen u=kernel(f,x),A,D;
  bool sinus=false; // sin or cos as A sin(Bx + C) + D
  let("qx",x);
  if (n>=1){ let("qa",co(f,x,n>=2?2:1)); let("qb",co(f,x,1)); let("qc",co(f,x,0)); }
  sec("FACTS");
  if (n==0) say("A constant function: a horizontal line.");
  else if (n==1){
    say(sgn(fx("qb"))>0?"A linear function, increasing.":"A linear function, decreasing.");
    fact("Slope","qb");
  }
  else if (n==2){
    const int up=sgn(fx("qa"));
    say(up>0?"A quadratic function: a parabola opening up.":"A quadratic function: a parabola opening down.");
    facts(T_quad);
    const char * k="qc-qb^2/(4*qa)";
    fact(up>0?"Minimum value":"Maximum value",k);
    yrow("Range",up>0?">=":"<=",k);
    facts(T_quad2);
    const int d=sgn(fx(T_quad2[1]));
    let("qk",fx(k)); // (inside a sum giac leaves 6-25/4 as it is)
    words("Real zeros",d>0?"two":d==0?"one (a double zero)":"none");
    facts(T_quad3);
    factored(f);
  }
  else if (n>2){
    const gen lead=co(f,x,n);
    say("A polynomial function.");
    row("Degree",n);
    row("Leading coefficient",lead);
    end_behavior(lead,n);
    factored(f);
  }
  else if (n==-2 || n==-3){
    say("A rational function.");
    gen s=normal(f,CTX);
    if (n==-3 || P(s)!=P(f)) row("Simplified",s);
  }
  else if (!is_zero(u) && (is_op(u,at_sin) || is_op(u,at_cos) || is_op(u,at_tan)) && !is_zero(u=trig_node(typed,x)) && linear_in(typed,u,x,A,D)){
    const gen arg=u._SYMBptr->feuille,B=ev(derive(arg,x,CTX));
    if (with(B,x)) say("A trigonometric function.");
    else {
      const bool tn=is_op(u,at_tan);
      sinus=!tn;
      let("qa",A); let("qb",B); let("qc",at(arg,x,0)); let("qd",D);
      say(tn?"A tangent function.":"A sinusoid: y = A sin(Bx + C) + D.");
      if (!tn) fact("Amplitude","abs(qa)");
      fact("Period",tn?"~pi/abs(qb)":"~2*pi/abs(qb)");
      const gen ps=fx("~-qc/qb");
      if (!is_zero(ps)){ std::string m=P(ps); m+="\n\1"; m+=sgn(ps)>0?"to the right":"to the left"; line(AN_ROW,"Phase shift",m,AN_SPACE); }
      if (!tn){
        yrow("Midline","=","qd");
        std::string m=P(fx("qd-abs(qa)")); m+="<=y<="; m+=P(fx("qd+abs(qa)")); line(AN_ROW,"Range",m);
      }
      else if (!is_zero(D)) row("Vertical shift",D);
    }
  }
  else if (!is_zero(u) && (is_op(u,at_exp) || (is_op(u,at_pow) && with(u._SYMBptr->feuille[1],x) && !with(u._SYMBptr->feuille[0],x)))){
    // a b^x + k: the factor from one x to the next, around the asymptote, checked at another x
    gen K=limit(f,*x._IDNTptr,minus_inf,0,CTX);
    if (is_inf(K) || is_undef(K)) K=limit(f,*x._IDNTptr,plus_inf,0,CTX);
    gen r=0;
    double r1,r2;
    if (!is_inf(K) && !is_undef(K)){
      r=nrm((at(f,x,1)-K)/(at(f,x,0)-K));
      if (!num(r,r1) || !num((at(f,x,3.5)-K)/(at(f,x,2.5)-K),r2) || r1<=0 || (r1-r2>1e-9*r1 || r2-r1>1e-9*r1)) r=0;
    }
    const int g=is_zero(r)?0:sgn(ev(r-1));
    if (g){
      say(g>0?"An exponential function: growth.":"An exponential function: decay.");
      row("Initial value",at(f,x,0));
      const char * lab=g>0?"Growth factor":"Decay factor";
      if (P(r).find("exp")!=std::string::npos){ // e, e^(-2)
        gen c=nrm(ln(r,CTX));
        std::string m("e"); if (c!=1){ m+="^("; m+=P(c); m+=')'; }
        line(AN_ROW,lab,m);
      }
      else row(lab,r);
      if (r.type==_INT_ || r.type==_FRAC || r.type==_DOUBLE_){ std::string m=dec(ev(abs(r-1,CTX)*100)); m+="%"; line(AN_ROW,g>0?"Growth rate":"Decay rate",m); }
    }
    else say("An exponential function.");
  }
  else if (!is_zero(u) && (is_op(u,at_ln) || is_op(u,at_log10))) say("A logarithmic function.");
  else if (!is_zero(u) && (is_op(u,at_sqrt) || (is_op(u,at_pow) && u._SYMBptr->feuille[1]==plus_one_half))) say("A square root function.");
  else if (!is_zero(u) && is_op(u,at_abs) && linear_in(f,u,x,A,D) && pdeg(u._SYMBptr->feuille,x)==1){
    const gen in=u._SYMBptr->feuille,h=ev(-co(in,x,0)/co(in,x,1));
    say(sgn(A)>0?"An absolute value function: a V opening up.":"An absolute value function: a V opening down.");
    line(AN_ROW,"Vertex",pt(h,at(f,x,h)));
  }
  else say("A function of x.");
  if (stop()) return;
  if (n>=0) poly_sections(f,x,n);
  else if (n<=-2) rational_sections(typed,f,x);
  else if (sinus) sinus_sections(f,x);
  else { analyze_brief=true; analyze_of(f0,*out_,CTX); analyze_brief=false; }
  std::string vx(","); vx+=P(x); vx+=")";
  if (n>=2){ mv("Factor","factor(",f,")",'r'); std::string z("=0"); z+=vx; mv("Zeros with steps","solve(",f,z.c_str(),'s'); }
  if (sinus){ std::string z("=0"); z+=vx; mv("Zeros with steps","solve(",f,z.c_str(),'s'); }
  if (n<0) mv("Derivative with steps","diff(",f,vx.c_str(),'s');
  mv("Graph","",f,"",'g');
  mv("Full analysis","",f,"",'a');
}

// ---------------------------------------------------------------- equations and inequalities
// the kind of f = 0 (f = lhs - rhs): its sentence; rat: divides by x as typed
static NOINL const char * eq_kind(const gen & f,const gen & x,bool ineq,bool rat){
  static const char * const K[]={"A linear","A quadratic","A polynomial","A rational","An absolute value","A radical","A trigonometric","A logarithmic","An exponential"};
  const int n=rat?-2:pdeg(f,x);
  int k=n==1?0:n==2?1:n>2?2:n==-2?3:-1;
  vecteur ks=kx(ev(f),x);
  for (unsigned i=0;i<ks.size() && k<0;++i){
    const gen & u=ks[i];
    k=is_op(u,at_abs)?4:is_op(u,at_sqrt) || (is_op(u,at_pow) && u._SYMBptr->feuille[1]==plus_one_half)?5:is_op(u,at_sin) || is_op(u,at_cos) || is_op(u,at_tan)?6:is_op(u,at_ln) || is_op(u,at_log10)?7:is_op(u,at_exp) || is_op(u,at_pow)?8:-1;
  }
  static char s[40];
  strcpy(s,k<0?(ineq?"An":"An"):K[k]); strcat(s,ineq?" inequality.":" equation.");
  return s;
}
// the moves of a relation in x: solve with steps, both sides graphed
static NOINL void rel_moves(const gen & r,const gen & x){
  std::string c("solve("); c+=P(ev(r)); c+=','; c+=P(x); c+=')';
  move("Solve with steps",c,'s');
  std::string g("["); g+=P(ev(r._SYMBptr->feuille[0])); g+=','; g+=P(ev(r._SYMBptr->feuille[1])); g+=']';
  move("Graph both sides",g,'g');
}
// v as a textbook writes it when it is a rational multiple of pi (5*pi/6, -2*pi/3: normal printed
// 5/6*pi), else simplified
static NOINL std::string pitext(const gen & v){
  const gen c=normal(ev(v/cst_pi),CTX);
  if (c.type!=_INT_ && c.type!=_FRAC) return P(nrm(v));
  if (is_zero(c)) return "0";
  const gen p=c.type==_FRAC?c._FRACptr->num:c,q=c.type==_FRAC?c._FRACptr->den:gen(1);
  std::string t=p==1?"pi":p==-1?"-pi":P(p)+"*pi";
  if (q!=1){ t+='/'; t+=P(q); }
  return t;
}
// a trigonometric equation: its general solution (giac's, every solution: n_0 an integer, called k)
// and the solutions in [0, 2 pi) from it (giac's principal ones can miss some: sin(2x) = 1/2 has
// four there); false if giac has no general solution
static NOINL bool trig_solutions(const gen & eq,const gen & x){
  // its solutions as bases and steps (b + st k): one sin, cos or tan of a x + c, linear in it
  // (2sin(x) = 1, sin(2x) = 1/2): the principal solutions and the period (as the console solves
  // it: giac's every-solution solve took 2.7 s on the calculator); else giac's general solution
  vecteur B,St;
  const gen f=ev(eq._SYMBptr->feuille[0]-eq._SYMBptr->feuille[1]);
  gen u=kernel(f,x),A,D,per=0;
  if (!is_zero(u) && (is_op(u,at_sin) || is_op(u,at_cos) || is_op(u,at_tan)) && linear_in(f,u,x,A,D)){
    const gen b=ev(derive(u._SYMBptr->feuille,x,CTX));
    if (!with(b,x) && !is_zero(b)) per=normal(ev((is_op(u,at_tan)?cst_pi:2*cst_pi)/abs(b,CTX)),CTX);
  }
  const bool ats=all_trig_sol(CTX);
  all_trig_sol(is_zero(per),CTX);
  gen s=_solve(makesequence(eq,x),CTX);
  all_trig_sol(ats,CTX);
  if (stop() || s.type!=_VECT || s._VECTptr->empty()) return false;
  for (unsigned i=0;i<s._VECTptr->size();++i){
    const gen & e=(*s._VECTptr)[i];
    if (!is_zero(per)){ B.push_back(e); St.push_back(per); continue; }
    vecteur l=letters(e);
    gen n=0;
    for (unsigned j=0;j<l.size();++j) if (l[j]!=x) n=l[j];
    const gen b=normal(ev(subst(e,n,0,false,CTX)),CTX); // (rational in pi: normal, not simplify)
    B.push_back(b); St.push_back(is_zero(n)?gen(0):normal(ev(subst(e,n,1,false,CTX)-b),CTX));
  }
  std::string g,m;
  vecteur in; std::vector<double> d;
  for (unsigned i=0;i<B.size() && !stop();++i){
    const gen & b=B[i], & st=St[i];
    std::string t=P(x); t+='='; t+=pitext(b);
    if (!is_zero(st)){ t+='+'; t+=pitext(st); t+="*k"; }
    item(g,t);
    // the k that land in [0, 2 pi), from the base and the step
    double b0,s0;
    if (!num(b,b0)) continue;
    int k0=0,k1=0;
    if (num(st,s0) && (s0>1e-9 || s0<-1e-9)){
      int a=(int)((-b0)/s0),c=(int)((2*M_PI-b0)/s0);
      k0=(a<c?a:c)-1; k1=(a<c?c:a)+1;
      if (k1-k0>24) k1=k0+24;
    }
    for (int k=k0;k<=k1;++k){
      const gen v=k?ev(b+st*k):b;
      double tv;
      if (!num(v,tv) || tv<-1e-9 || tv>=2*M_PI-1e-9) continue;
      bool seen=false;
      for (unsigned j=0;j<d.size();++j) seen=seen || (d[j]-tv<1e-9 && tv-d[j]<1e-9);
      if (!seen){ d.push_back(tv); in.push_back(v); }
    }
  }
  for (unsigned i=0;i<d.size();++i) for (unsigned j=i+1;j<d.size();++j) if (d[j]<d[i]){ std::swap(d[i],d[j]); std::swap(in[i],in[j]); }
  for (unsigned i=0;i<in.size();++i){ std::string t=P(x); t+='='; t+=pitext(in[i]); item(m,t); }
  say("Infinitely many solutions: they repeat.");
  line(AN_ROW,"General solution",g,AN_OR);
  say("k is any integer.");
  line(AN_ROW,"In [0, 2\xcf\x80)",m,AN_OR);
  rel_moves(eq,x);
  return true;
}
static const char * const T_csol[]={"Complex solutions","|qx=(-qb+i*sqrt(4*qa*qc-qb^2))/(2*qa);qx=(-qb-i*sqrt(4*qa*qc-qb^2))/(2*qa)",0};
static NOINL void equation1(const gen & eq,const gen & x){
  const gen L=eq._SYMBptr->feuille[0],R=eq._SYMBptr->feuille[1],f=ev(L-R);
  sec("FACTS");
  const bool rat=divides_by(L,x) || divides_by(R,x);
  say(eq_kind(f,x,false,rat));
  vecteur ks=kx(f,x);
  bool trig=false;
  for (unsigned i=0;i<ks.size();++i) trig=trig || is_op(ks[i],at_sin) || is_op(ks[i],at_cos) || is_op(ks[i],at_tan);
  if (trig && trig_solutions(eq,x)) return;
  gen s=_solve(makesequence(eq,x),CTX);
  if (stop()) return;
  const int n=pdeg(f,x);
  let("qx",x);
  if (n==2){ let("qa",co(f,x,2)); let("qb",co(f,x,1)); let("qc",co(f,x,0)); }
  if (s.type==_VECT){
    const unsigned k=s._VECTptr->size();
    std::string m;
    for (unsigned i=0;i<k;++i) item(m,eqs(x,(*s._VECTptr)[i]));
    if (!k){
      say(n==2?"No real solution: its solutions are complex.":"No real solution.");
      if (n==2) facts(T_csol);
    }
    else {
      say(k==1?"One solution.":k==2?"Two solutions.":"Several solutions.");
      line(AN_ROW,k==1?"Solution":"Solutions",m,AN_OR);
    }
  }
  if (n==2) facts(T_quad2);
  if (n==-2 || rat){ // the values the denominators exclude (as typed: (x^2-4)/(x-2) excludes 2)
    vecteur dv; dens(L,x,dv); dens(R,x,dv);
    gen den=1;
    for (unsigned i=0;i<dv.size();++i) den=den*dv[i];
    gen z=_solve(makesequence(symbolic(at_equal,makesequence(ev(den),0)),x),CTX);
    if (z.type==_VECT && z._VECTptr->size()){
      std::string m;
      for (unsigned i=0;i<z._VECTptr->size();++i){ std::string t=P(x); t+="!="; t+=P((*z._VECTptr)[i]); item(m,t); }
      line(AN_ROW,"Excluded values",m);
    }
  }
  rel_moves(eq,x);
}
// an interval of an inequality's solution: lo < x < hi (lc, hc: closed ends; oo: none)
struct iv { gen lo,hi; bool lc,hc; };
static NOINL bool lt(const gen & a,const gen & b){ // a < b, oo understood
  if (a==b) return false;
  if (a==minus_inf || b==plus_inf) return true;
  if (a==plus_inf || b==minus_inf) return false;
  double p,q; return num(a,p) && num(b,q) && p<q;
}
// a relation of x (x < 3, x >= -1, 3 > x, x = 2) as an interval; false if not one
static NOINL bool to_iv(const gen & r,const gen & x,iv & I){
  if (r.type!=_SYMB || r._SYMBptr->feuille.type!=_VECT || r._SYMBptr->feuille._VECTptr->size()!=2) return false;
  gen a=r._SYMBptr->feuille[0],b=r._SYMBptr->feuille[1];
  const unary_function_ptr & s=r._SYMBptr->sommet;
  if (s==at_equal){ if (a!=x) std::swap(a,b); if (a!=x) return false; I.lo=I.hi=b; I.lc=I.hc=true; return true; }
  bool less=s==at_inferieur_strict || s==at_inferieur_egal;
  const bool strict=s==at_inferieur_strict || s==at_superieur_strict;
  if (!less && s!=at_superieur_strict && s!=at_superieur_egal) return false;
  if (b==x){ std::swap(a,b); less=!less; }
  if (a!=x || with(b,x)) return false;
  if (less){ I.lo=minus_inf; I.lc=false; I.hi=b; I.hc=!strict; }
  else { I.lo=b; I.lc=!strict; I.hi=plus_inf; I.hc=false; }
  return true;
}
// the solution on a number line (focus_steps.cc draws it): its ends in order, then for each of
// the pieces they cut the line into whether it is shaded ('1'), for each end whether it is in
// ('c', filled) or not ('o', hollow), and the ends' labels: "pieces<tab>ends<tab>label<tab>label..."
static NOINL void numline(const std::vector<iv> & V){
  vecteur E; std::vector<double> d;
  for (unsigned i=0;i<V.size();++i) for (int s=0;s<2;++s){
    const gen & e=s?V[i].hi:V[i].lo;
    double t;
    if (is_inf(e) || !num(e,t)) continue;
    unsigned j=0;
    while (j<d.size() && d[j]<t-1e-9) ++j;
    if (j<d.size() && d[j]-t<1e-9) continue;
    d.insert(d.begin()+j,t); E.insert(E.begin()+j,e);
  }
  const unsigned k=d.size();
  if (k>5) return;
  std::string seg,ends,lab;
  for (unsigned j=0;j<=k;++j){ // a piece's middle in an interval: shaded
    const double t=k==0?0:j==0?d[0]-1:j==k?d[k-1]+1:(d[j-1]+d[j])/2;
    bool in=false;
    for (unsigned i=0;i<V.size() && !in;++i){ double a,b; in=(V[i].lo==minus_inf || (num(V[i].lo,a) && a<t)) && (V[i].hi==plus_inf || (num(V[i].hi,b) && t<b)); }
    seg+=in?'1':'0';
  }
  for (unsigned j=0;j<k;++j){ // an end in an interval (closed there, or a point)
    bool in=false;
    for (unsigned i=0;i<V.size() && !in;++i){ double a,b; in=(V[i].lc && num(V[i].lo,a) && a-d[j]<1e-9 && d[j]-a<1e-9) || (V[i].hc && num(V[i].hi,b) && b-d[j]<1e-9 && d[j]-b<1e-9); }
    ends+=in?'c':'o';
    lab+='\t'; lab+=P(E[j]);
  }
  std::string m=seg; m+='\t'; m+=ends; m+=lab;
  line(AN_NUMLINE,0,m);
}
static NOINL void inequality1(const gen & r,const gen & x){
  const gen L=r._SYMBptr->feuille[0],R=r._SYMBptr->feuille[1];
  sec("FACTS");
  say(eq_kind(ev(L-R),x,true,divides_by(L,x) || divides_by(R,x)));
  gen s=_solve(makesequence(r,x),CTX);
  if (stop() || s.type!=_VECT) return;
  std::vector<iv> V;
  bool ok=true;
  for (unsigned i=0;i<s._VECTptr->size() && ok;++i){
    const gen & c=(*s._VECTptr)[i];
    iv I;
    if (is_op(c,at_and) && c._SYMBptr->feuille.type==_VECT && c._SYMBptr->feuille._VECTptr->size()==2){
      iv B;
      ok=to_iv(c._SYMBptr->feuille[0],x,I) && to_iv(c._SYMBptr->feuille[1],x,B);
      if (ok){ // their intersection
        if (lt(I.lo,B.lo) || (I.lo==B.lo && !B.lc)){ I.lo=B.lo; I.lc=B.lc; }
        if (lt(B.hi,I.hi) || (I.hi==B.hi && !B.hc)){ I.hi=B.hi; I.hc=B.hc; }
      }
    }
    else ok=to_iv(c,x,I);
    if (ok) V.push_back(I);
  }
  if (!ok){ row("Solution",s); return; }
  // sorted by their left ends, the ones that meet or touch merged (x <= 0 or x >= 0: all of them)
  for (unsigned i=0;i<V.size();++i) for (unsigned j=i+1;j<V.size();++j) if (lt(V[j].lo,V[i].lo)) std::swap(V[i],V[j]);
  for (unsigned i=0;i+1<V.size();){
    iv & A=V[i]; const iv & B=V[i+1];
    if (lt(B.lo,A.hi) || (B.lo==A.hi && (A.hc || B.lc))){
      if (lt(A.hi,B.hi)){ A.hi=B.hi; A.hc=B.hc; } else if (A.hi==B.hi) A.hc=A.hc || B.hc;
      V.erase(V.begin()+i+1);
    }
    else ++i;
  }
  if (V.empty()) say("No solution: no x makes it true.");
  else if (V.size()==1 && V[0].lo==minus_inf && V[0].hi==plus_inf){ say("Every real number is a solution."); words("Solution","all real numbers"); }
  else {
    std::string m,n;
    for (unsigned i=0;i<V.size();++i){
      const iv & I=V[i];
      std::string t;
      if (I.lo==I.hi) t=eqs(x,I.lo);
      else {
        if (I.lo!=minus_inf){ t=P(I.lo); t+=I.lc?"<=":"<"; }
        t+=P(x);
        if (I.hi!=plus_inf){ t+=I.hc?"<=":"<"; t+=P(I.hi); }
        else if (I.lo!=minus_inf){ t=P(x); t+=I.lc?">=":">"; t+=P(I.lo); }
      }
      item(m,t);
      std::string w(I.lc?"[":"("); w+=Pi(I.lo); w+=','; w+=Pi(I.hi); w+=I.hc?"]":")";
      item(n,w);
    }
    line(AN_ROW,"Solution",m,AN_OR);
    line(AN_ROW,"Interval notation",n,AN_OR);
    numline(V);
  }
  rel_moves(r,x);
}

// ---------------------------------------------------------------- curves in x and y
// a line q_d x + q_e y + q_f = 0; a conic q_a x^2 + q_c y^2 + q_d x + q_e y + q_f = 0, centered at
// (q_h, q_k) with q_u and q_v under (x - q_h)^2 and (y - q_k)^2 once divided by its constant
static const char * const T_line[]={"Slope","-qd/qe","y-intercept","@0,-qf/qe",0};
static const char * const T_line2[]={"Slope-intercept form","qy=-qd/qe*qx-qf/qe","Standard form","qn*qd*qx+qn*qe*qy=-qn*qf",0};
static const char * const T_circle[]={"Center","@qh,qk","Radius","sqrt(qu)","Standard form","(qx-qh)^2+(qy-qk)^2=qu",0};
static const char * const T_ell[]={"Center","@qh,qk","Standard form","(qx-qh)^2/qu+(qy-qk)^2/qv=1",0};
static const char * const T_ellh[]={"Vertices","@qh-sqrt(qu),qk;qh+sqrt(qu),qk","Foci","@qh-sqrt(qu-qv),qk;qh+sqrt(qu-qv),qk","Major axis length","2*sqrt(qu)","Minor axis length","2*sqrt(qv)",0};
static const char * const T_ellv[]={"Vertices","@qh,qk-sqrt(qv);qh,qk+sqrt(qv)","Foci","@qh,qk-sqrt(qv-qu);qh,qk+sqrt(qv-qu)","Major axis length","2*sqrt(qv)","Minor axis length","2*sqrt(qu)",0};
static const char * const T_hyph[]={"Center","@qh,qk","Standard form","(qx-qh)^2/qu-(qy-qk)^2/(-qv)=1","Vertices","@qh-sqrt(qu),qk;qh+sqrt(qu),qk","Foci","@qh-sqrt(qu-qv),qk;qh+sqrt(qu-qv),qk",0};
static const char * const T_hypv[]={"Center","@qh,qk","Standard form","(qy-qk)^2/qv-(qx-qh)^2/(-qu)=1","Vertices","@qh,qk-sqrt(qv);qh,qk+sqrt(qv)","Foci","@qh,qk-sqrt(qv-qu);qh,qk+sqrt(qv-qu)",0};
static const char * const T_asym[]={"Asymptotes","qy=qk+sqrt(-qv/qu)*(qx-qh);qy=qk-sqrt(-qv/qu)*(qx-qh)",0};
static const char * const T_parup[]={"Vertex","@qh,qk","Focus","@qh,qk+qp","Directrix","qy=qk-qp","Axis of symmetry","qx=qh",0};
static const char * const T_parri[]={"Vertex","@qh,qk","Focus","@qh+qp,qk","Directrix","qx=qh-qp","Axis of symmetry","qy=qk",0};
static NOINL void curve(const gen & eq,const gen & x,const gen & y){
  const gen F=ev(expand(ev(eq._SYMBptr->feuille[0]-eq._SYMBptr->feuille[1]),CTX));
  sec("FACTS");
  int deg=-1;
  gen B=0;
  let("qx",x); let("qy",y);
  if (pdeg(F,x)>=0 && pdeg(F,y)>=0){ // a polynomial in x and y: its total degree and coefficients
    const gen T=identificateur("t__");
    gen t=_degree(makesequence(ev(subst(F,makevecteur(x,y),makevecteur(x*T,y*T),false,CTX)),T),CTX);
    deg=t.type==_INT_?t.val:-1;
    if (deg>=1 && deg<=2){
      const gen Fx=co(F,y,0),Fy=co(F,y,1);
      B=co(Fy,x,1);
      let("qa",co(Fx,x,2)); let("qc",co(F,y,2)); let("qd",co(Fx,x,1)); let("qe",co(Fy,x,0)); let("qf",co(Fx,x,0));
    }
  }
  if (deg==1){
    say("A line.");
    if (is_zero(fx("qe"))){ fact("Vertical line","qx=-qf/qd"); words("Slope","undefined"); }
    else {
      facts(T_line);
      if (!is_zero(fx("qd"))) fact("x-intercept","@-qf/qd,0");
      // standard form A x + B y = C: whole numbers without a common factor, A >= 0
      gen l=fx("lcm(lcm(denom(qd),denom(qe)),denom(qf))");
      let("qm",l);
      gen g=fx("gcd(gcd(qd*qm,qe*qm),qf*qm)");
      const int s=sgn(fx("qd"));
      let("qn",ev(l/g*(s<0 || (s==0 && sgn(fx("qe"))<0)?-1:1)));
      facts(T_line2);
    }
    mv("Graph","",eq,"",'g');
    mv("Solve for y","solve(",eq,",y)",'s');
    return;
  }
  if (deg==2 && is_zero(B)){
    const bool A=!is_zero(fx("qa")),C=!is_zero(fx("qc"));
    if (A && C){
      let("qh",fx("-qd/(2*qa)")); let("qk",fx("-qe/(2*qc)"));
      let("qu",fx("(qa*qh^2+qc*qk^2-qf)/qa")); let("qv",fx("(qa*qh^2+qc*qk^2-qf)/qc"));
      const int sa=sgn(fx("qu")),sc=sgn(fx("qv"));
      if (sa==0 && sc==0){ say("A single point (a degenerate conic)."); fact("The point","@qh,qk"); return; }
      if (sa<0 && sc<0){ say("No points: no real x, y satisfy it."); return; }
      if (fx("qa")==fx("qc")){ say("A circle."); facts(T_circle); }
      else if (sa>0 && sc>0){
        const bool hor=sgn(fx("qu-qv"))>0;
        say(hor?"An ellipse, wider than tall.":"An ellipse, taller than wide.");
        facts(T_ell); facts(hor?T_ellh:T_ellv);
      }
      else if (sa==0 || sc==0) say("Two crossing lines (a degenerate conic).");
      else {
        const bool hor=sa>0; // opens left and right
        say(hor?"A hyperbola opening left and right.":"A hyperbola opening up and down.");
        facts(hor?T_hyph:T_hypv); facts(T_asym);
      }
      mv("Graph","",eq,"",'g');
      return;
    }
    const bool up=A && !C && !is_zero(fx("qe")),ri=!A && C && !is_zero(fx("qd"));
    if (up || ri){ // (x - h)^2 = 4p(y - k), or (y - k)^2 = 4p(x - h)
      if (up){ let("qh",fx("-qd/(2*qa)")); let("qk",fx("(qa*qh^2-qf)/qe")); let("qp",fx("-qe/(4*qa)")); }
      else { let("qk",fx("-qe/(2*qc)")); let("qh",fx("(qc*qk^2-qf)/qd")); let("qp",fx("-qd/(4*qc)")); }
      const int p=sgn(fx("qp"));
      say(up?(p>0?"A parabola opening up.":"A parabola opening down."):(p>0?"A parabola opening right.":"A parabola opening left."));
      facts(up?T_parup:T_parri);
      mv("Graph","",eq,"",'g');
      return;
    }
  }
  if (deg==2){ const int d=sgn(fx("qd*0")+ev(B*B)-fx("4*qa*qc")); say(d<0?"An ellipse, rotated.":d>0?"A hyperbola, rotated.":"A parabola, rotated."); }
  else say("A curve in x and y.");
  { // where it crosses the axes
    gen s=_solve(makesequence(symbolic(at_equal,makesequence(at(F,x,0),0)),y),CTX),t=_solve(makesequence(symbolic(at_equal,makesequence(at(F,y,0),0)),x),CTX);
    std::string m,n;
    if (s.type==_VECT) for (unsigned i=0;i<s._VECTptr->size() && i<6;++i) item(m,pt(0,(*s._VECTptr)[i]));
    if (t.type==_VECT) for (unsigned i=0;i<t._VECTptr->size() && i<6;++i) item(n,pt((*t._VECTptr)[i],0));
    if (!m.empty()) line(AN_ROW,"y-intercepts",m);
    if (!n.empty()) line(AN_ROW,"x-intercepts",n);
  }
  mv("Graph","",eq,"",'g');
  mv("dy/dx with steps","implicitdiff(",eq,",x,y)",'s');
}

// ---------------------------------------------------------------- lists: systems, matrices, data
static NOINL void system_of(const gen & g,const vecteur & X){
  const vecteur & v=*g._VECTptr;
  sec("FACTS");
  bool lin=true; // every second derivative 0
  for (unsigned i=0;i<v.size() && lin;++i){
    const gen f=ev(v[i]._SYMBptr->feuille[0]-v[i]._SYMBptr->feuille[1]);
    for (unsigned j=0;j<X.size() && lin;++j) for (unsigned k=0;k<X.size() && lin;++k) lin=is_zero(ev(derive(derive(f,X[j],CTX),X[k],CTX)));
  }
  gen s=_solve(makesequence(g,gen(X)),CTX);
  if (stop()) return;
  const bool two=lin && X.size()==2 && v.size()==2;
  if (s.type==_VECT && s._VECTptr->empty()) say(two?"Parallel lines: no solution.":"No solution.");
  else if (s.type==_VECT && s._VECTptr->size()==1 && (*s._VECTptr)[0].type==_VECT){
    const vecteur & w=*(*s._VECTptr)[0]._VECTptr;
    bool free=false;
    for (unsigned i=0;i<w.size();++i) free=free || !letters(w[i]).empty();
    say(free?(two?"The same line twice: infinitely many solutions.":"Infinitely many solutions."):two?"Two lines crossing at one point.":"One solution.");
    std::string m;
    for (unsigned i=0;i<w.size() && i<X.size();++i) if (w[i]!=X[i]) item(m,eqs(X[i],w[i]));
    line(AN_ROW,"Solution",m);
  }
  else if (s.type==_VECT){
    say(lin?"A linear system.":"A system of equations.");
    std::string m;
    for (unsigned i=0;i<s._VECTptr->size() && i<8;++i) item(m,P((*s._VECTptr)[i]));
    line(AN_ROW,"Solutions",m,AN_OR);
  }
  std::string c("solve("); c+=P(ev(g)); c+=','; c+=P(gen(X)); c+=')';
  move("Solve with steps",c,'s');
  if (X.size()==2) mv("Graph","",g,"",'g');
}
static const char * const T_sq[]={"Trace","trace(qm)",0};
static const char * const T_eig2[]={"Eigenvalues","(trace(qm)-sqrt(trace(qm)^2-4*det(qm)))/2;(trace(qm)+sqrt(trace(qm)^2-4*det(qm)))/2",0};
static NOINL void matrix_of(const gen & M){
  const vecteur & r=*M._VECTptr;
  const unsigned m=r.size(),n=r[0]._VECTptr->size();
  let("qm",M);
  sec("FACTS");
  { std::string t("\1"); t+=P(gen(int(m))); t+=" rows, "; t+=P(gen(int(n))); t+=" columns"; line(AN_ROW,"Size",t); }
  fact("Rank","rank(qm)");
  if (m==n){
    const gen d=fx("det(qm)");
    row("Determinant",d);
    say(is_zero(d)?"Not invertible: its determinant is 0.":"Invertible: its determinant is not 0.");
    facts(T_sq);
    if (m==2) facts(T_eig2); // (giac's egvl took 1.5 s on the calculator)
    if (m<=3){
      gen e=m==3?_egvl(M,CTX):gen(0);
      if (ckmatrix(e)){
        std::string s;
        for (unsigned i=0;i<m;++i) item(s,P((*(*e._VECTptr)[i]._VECTptr)[i]));
        line(AN_ROW,"Eigenvalues",s);
      }
      if (!is_zero(d)) fact("Inverse","inv(qm)");
    }
    if (!is_zero(d)) mv("Inverse","inv(",M,")",'r');
  }
  mv("Row reduce (rref)","rref(",M,")",'r');
}
// TI's quartiles: the medians of the halves below and above the median (the median left out)
static NOINL gen med(const vecteur & v,unsigned a,unsigned b){ // of v[a,b)
  const unsigned n=b-a;
  return n%2?v[a+n/2]:ev((v[a+n/2-1]+v[a+n/2])/2);
}
static NOINL void data_of(const gen & L){
  vecteur v=*ev(L)._VECTptr;
  const unsigned n=v.size();
  for (unsigned i=0;i<n;++i) for (unsigned j=i+1;j<n;++j) if (is_strictly_greater(v[i],v[j],CTX)) std::swap(v[i],v[j]);
  let("ql",gen(v,_LIST__VECT)); let("qn",int(n)); // (a list: a bare vecteur is a sequence)
  sec("FACTS");
  say("A list of data.");
  row("Number of values",int(n));
  fact("Sum","sum(ql)");
  { const gen mean=fx("mean(ql)"); std::string m=P(mean); if (mean.type==_FRAC) m+=approx(mean); line(AN_ROW,"Mean",m,AN_SPACE); }
  row("Median",med(v,0,n));
  { // the values seen most often, if any is seen more than once
    unsigned best=1;
    for (unsigned i=0;i<n;){ unsigned j=i; while (j<n && v[j]==v[i]) ++j; if (j-i>best) best=j-i; i=j; }
    std::string m;
    if (best>1) for (unsigned i=0;i<n;){ unsigned j=i; while (j<n && v[j]==v[i]) ++j; if (j-i==best) item(m,P(v[i])); i=j; }
    if (m.empty()) words("Mode","none (no value repeats)"); else line(AN_ROW,"Mode",m);
  }
  row("Minimum",v[0]);
  const gen q1=n>1?med(v,0,n/2):v[0],q3=n>1?med(v,(n+1)/2,n):v[0],iqr=ev(q3-q1);
  row("Q1",q1);
  row("Q3",q3);
  row("Maximum",v[n-1]);
  row("Range",ev(v[n-1]-v[0]));
  row("IQR",iqr);
  if (n>1){
    line(AN_ROW,"Sample SD, s",dec(fx("stddev(ql)*sqrt(qn/(qn-1))")));
    line(AN_ROW,"Population SD, \xcf\x83",dec(fx("stddev(ql)")));
    std::string o; // outliers: beyond 1.5 IQR from the quartiles
    const gen lo=ev(q1-iqr*3/2),hi=ev(q3+iqr*3/2);
    for (unsigned i=0;i<n;++i) if (is_strictly_greater(lo,v[i],CTX) || is_strictly_greater(v[i],hi,CTX)) item(o,P(v[i]));
    if (o.empty()) words("Outliers","none (1.5 IQR rule)"); else line(AN_ROW,"Outliers",o);
  }
  if (n==2 || n==3){ sec("AS A VECTOR"); fact("Length","sqrt(dot(ql,ql))"); }
  mv("Sorted","sort(",L,")",'r');
}

// ---------------------------------------------------------------- the reading
static const char * const calc_ops[]={"diff","integrate","limit","sum","solve","factor","expand","simplify","partfrac","series","taylor","implicitdiff","analyze",0};
static NOINL const char * read(const gen & g0){
  // a phrase's equation (phrase.cc: ph_fact(left, right), giac's of(ph_fact, [left, right])): the equation
  gen g=g0;
  if (is_op(g0,at_of) && g0._SYMBptr->feuille.type==_VECT && g0._SYMBptr->feuille._VECTptr->size()==2){
    const gen & n=g0._SYMBptr->feuille[0], & a=g0._SYMBptr->feuille[1];
    if (n.type==_IDNT && !strcmp(n._IDNTptr->id_name,"ph_fact") && a.type==_VECT && a._VECTptr->size()==2) g=symbolic(at_equal,makesequence(a[0],a[1]));
  }
  // f(x)=... typed: the input pre-pass made it f(x):=..., a program stored in f: its body
  if (is_op(g,at_sto) && g._SYMBptr->feuille.type==_VECT && is_op(g._SYMBptr->feuille[0],at_program)){
    const gen & pr=g._SYMBptr->feuille[0]._SYMBptr->feuille; // [arguments, defaults, body]
    if (pr.type==_VECT && pr._VECTptr->size()==3){
      const gen a=pr[0].type==_VECT && pr[0]._VECTptr->size()==1?pr[0][0]:pr[0];
      if (a.type==_IDNT && letters(pr[2]).size()<=1){ function_of(pr[2],pr[2],a); return 0; }
    }
  }
  if (g.type==_VECT && !g._VECTptr->empty()){
    const vecteur & v=*g._VECTptr;
    bool eqs_=true,nums=true;
    for (unsigned i=0;i<v.size();++i){
      eqs_=eqs_ && is_op(v[i],at_equal);
      double d; nums=nums && v[i].type!=_VECT && letters(v[i]).empty() && num(v[i],d);
    }
    if (eqs_){ vecteur X=letters(g); if (X.empty()) return "No letters to solve for"; system_of(g,X); return 0; }
    if (ckmatrix(g)){ matrix_of(g); return 0; }
    if (nums){ data_of(g); return 0; }
    return "Not a list Δ reads yet";
  }
  if (g.type==_SYMB){ // a calculation typed (diff, integrate, solve...): its answer and its steps
    const char * o=op_name(g);
    for (int i=0;calc_ops[i];++i) if (!strcmp(o,calc_ops[i])){
      static const char * const S[]={"A derivative.","An integral.","A limit.","A sum."};
      sec("FACTS");
      say(i<4?S[i]:"A calculation.");
      if (i!=12){ // (solve's list[...] shown as the console shows it, [...]; undef in words)
        gen r=ev(add_autosimplify(ev(g),CTX));
        if (r.type==_VECT) r=gen(*r._VECTptr,0);
        if (is_undef(r) || r==unsigned_inf) words("Answer",i==1?"it diverges":i==2?"it does not exist":"undefined");
        else row("Answer",r);
      }
      move("Steps",P(g),'s');
      return 0;
    }
  }
  vecteur X=letters(g);
  const bool rel=is_op(g,at_equal) || is_op(g,at_inferieur_strict) || is_op(g,at_inferieur_egal) || is_op(g,at_superieur_strict) || is_op(g,at_superieur_egal);
  if (rel && g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()==2){
    const gen L=g._SYMBptr->feuille[0],R=g._SYMBptr->feuille[1];
    if (is_op(g,at_equal)){
      // y = f(x), f(x) = ...: the function (x = f(y) is a curve)
      // (f(x) is giac's of(f, x): sin(x) = cos(x) is an equation)
      const bool fx_=is_op(L,at_of) && L._SYMBptr->feuille.type==_VECT && L._SYMBptr->feuille._VECTptr->size()==2 && L._SYMBptr->feuille[1].type==_IDNT;
      if ((L.type==_IDNT && P(L)!="x" && !with(R,L) && letters(R).size()==1) || (fx_ && letters(R).size()==1 && with(R,L._SYMBptr->feuille[1]))){
        function_of(R,R,letters(R)[0]);
        return 0;
      }
      if (X.size()==1){ equation1(g,X[0]); return 0; }
      if (X.size()==2){
        gen x=X[0],y=X[1];
        if (P(x)=="y" || P(y)=="x") std::swap(x,y);
        curve(g,x,y);
        return 0;
      }
      return X.empty()?"No letter: a true or false statement":"Too many letters for Δ";
    }
    if (X.size()==1){ inequality1(g,X[0]); return 0; }
    return "Δ reads inequalities in one letter";
  }
  if (X.empty()){ // a number
    gen v=ev(g);
    sec("FACTS");
    if (v.type==_INT_ || v.type==_ZINT){ if (P(g)!=P(v)) row("Value",v); integer(v); return 0; }
    if (v.type==_FRAC){ rational(g,v); return 0; }
    if (!is_zero(ev(im(v,CTX)))){ complex_number(v); return 0; }
    double d;
    if (num(v,d)){ real_number(g,v); return 0; }
    return "Not a number Δ reads";
  }
  if (X.size()==1){ function_of(g,g,X[0]); return 0; }
  // an expression in several letters: its other forms
  const gen e=ev(g),fa=factor(e,false,CTX),ex=ev(expand(e,CTX)),si=simplify(e,CTX);
  sec("FACTS");
  say("An expression in several letters.");
  if ((is_op(fa,at_prod) || is_op(fa,at_pow) || is_op(fa,at_neg)) && P(fa)!=P(e)) row("Factored",fa);
  if (!expanded_already(e)) row("Expanded",ex);
  if (P(si).size()<P(e).size() && P(si)!=P(fa) && P(si)!=P(ex)) row("Simplified",si);
  mv("Factor","factor(",g,")",'r');
  mv("Expand","expand(",g,")",'r');
  mv("Simplify","simplify(",g,")",'r');
  return 0;
}

// g compared with the last answer a: equal, equal but for a constant (two antiderivatives), or
// an x where they differ; expressions and numbers only
static NOINL void compare_last(const gen & g,const gen & a){
  if (g.type==_VECT || a.type==_VECT || is_op(g,at_equal) || is_op(a,at_equal) || is_op(g,at_of) || is_op(a,at_of)) return;
  const gen d=ev(g-a);
  vecteur L=letters(d);
  if (L.size()>1) return;
  sec("COMPARED WITH THE LAST ANSWER");
  const gen s=nrm(d);
  if (is_zero(s)){ say("Equal: the same, in another form."); return; }
  if (letters(s).empty() && L.size()==1){ say("Equal but for a constant (two antiderivatives?)."); row("The difference",s); return; }
  if (L.empty()){ say("Not equal."); row("The difference",s); return; }
  static const double T[4]={0.5,1.3,2.7,-1.7};
  for (int i=0;i<4;++i){ // an x where both are defined and differ
    double p,q;
    if (!num(at(g,L[0],T[i]),p) || !num(at(a,L[0],T[i]),q)) continue;
    if (p-q<1e-6*(1+(p<0?-p:p)) && q-p<1e-6*(1+(p<0?-p:p))) continue;
    say("Not equal.");
    std::string m=P(L[0]); m+='='; m+=dec(gen(T[i])); line(AN_ROW,"They differ at",m);
    line(AN_ROW,"This one there",dec(gen(p))); line(AN_ROW,"The last answer there",dec(gen(q)));
    return;
  }
  say("Equal at every x tried (not simplified to the same).");
}
const char * delta_of(const gen & g,const char * typed,const char * last,std::vector<an_line> & out,std::vector<dl_move> & moves,GIAC_CONTEXT){
  ctx_=contextptr; out_=&out; mv_=&moves;
  std::vector<const char *> vn;
  std::vector<std::string> vs;
  vn_=&vn; vs_=&vs;
  stdostream * log=logptr(CTX);
  logptr(0,CTX); // (solve's notes went to the history)
  line(AN_HEAD,0,typed?std::string(typed):P(g));
  const char * why=first_error_line(CTX)?"Can't read this: check what you typed":read(g);
  if (!why && last && *last && (!typed || strcmp(typed,last)) && !stop()){
    gen a(std::string(last),CTX);
    if (!is_undef(a)) compare_last(g,ev(a));
  }
  logptr(log,CTX);
  if (stop()) return "Stopped";
  return why;
}
#endif
