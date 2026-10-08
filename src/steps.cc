// steps.cc - step-by-step solutions (steps.h).
// Built for size: a step stores a rule number and its parts. Each rule's math line is a template
// (giac syntax; A, B, C the parts, V the variable, dV and dA their differentials) filled only when
// the step is shown. The methods share helpers: a rewritten integrand (via), a substitution (sub).
#if defined FLOWCE_STEPS || defined HOSTGIAC
#include "steps.h"
#include <string.h>
using namespace giac;
#define NOINL __attribute__((noinline))
#ifdef TICE
#define TOL 1e-4 // the calculator's doubles are 32-bit floats
#else
#define TOL 1e-8
#endif

// ---------------------------------------------------------------- rules
// title | formula (giac syntax, drawn small; 0: none) | math line
enum {
  R_EQD, R_SUMD, R_CONSTD, R_PROD, R_QUOT, R_POWD, R_CHAIN, R_BASICD, R_EXPD, R_LOGD,
  R_EQI, R_SUMI, R_CONSTI, R_POWI, R_CONSTANT, R_LNI, R_TABLE, R_ATAN, R_ASIN, R_LNSQ, R_SUB, R_SUBEQ, R_BACK,
  R_PARTS, R_PARTSV, R_PARTS2, R_CYCLE, R_PFRAC, R_TRIGID, R_PYTH, R_EXPAND, R_SQUARE, R_SIMP, R_TSUB,
  R_TSUBEQ, R_BACKX, R_FTC, R_EQV, R_NRULES
};
static const char * const RULES[]={
  "",0,"diff(A,V)=B",
  "Sum rule","diff(u+v,x)=diff(u,x)+diff(v,x)","diff(A,V)=B",
  "Constant multiple","diff(c*u,x)=c*diff(u,x)","diff(A,V)=B*diff(C,V)",
  "Product rule","diff(u*v,x)=diff(u,x)*v+u*diff(v,x)","diff(A,V)=diff(B,V)*C+B*diff(C,V)",
  "Quotient rule","diff(u/v,x)=(diff(u,x)*v-u*diff(v,x))/v^2","diff(A,V)=(diff(B,V)*C-B*diff(C,V))/C^2",
  "Power rule","diff(x^n,x)=n*x^(n-1)","diff(A,V)=B",
  "Chain rule","diff(f(u),x)=diff(f,u)*diff(u,x)","diff(A,V)=B*diff(C,V)",
  "Known derivative",0,"diff(A,V)=B",
  "Exponential rule","diff(a^x,x)=a^x*ln(a)","diff(A,V)=B",
  "Write the power with e",0,"A=B",
  "",0,"integrate(A,V)=B",
  "Sum rule","integrate(u+v,x)=integrate(u,x)+integrate(v,x)","integrate(A,V)=B",
  "Constant multiple","integrate(c*u,x)=c*integrate(u,x)","integrate(A,V)=B*integrate(C,V)",
  "Power rule","integrate(x^n,x)=x^(n+1)/(n+1)","integrate(A,V)=B",
  "Integral of a constant","integrate(c,x)=c*x","integrate(A,V)=B",
  "Integral of 1/x","integrate(1/x,x)=ln(abs(x))","integrate(A,V)=B",
  "Known integral",0,"integrate(A,V)=B",
  "Arctangent formula","integrate(1/(a^2+x^2),x)=atan(x/a)/a","integrate(A,V)=B",
  "Arcsine formula","integrate(1/sqrt(a^2-x^2),x)=asin(x/a)","integrate(A,V)=B",
  "Logarithm formula","integrate(1/sqrt(x^2+c),x)=ln(abs(x+sqrt(x^2+c)))","integrate(A,V)=B",
  "Substitution",0,"A=B,dA=C*dV",
  "",0,"integrate(A,V)=integrate(B,C)",
  "Substitute back",0,"integrate(A,V)=B",
  "Integration by parts","integrate(u,v)=u*v-integrate(v,u)","U=A,dW=B*dV",
  "",0,"dU=A*dV,W=B",
  "",0,"integrate(A,V)=B",
  "The integral comes back: solve for it",0,"integrate(A,V)=B",
  "Partial fractions",0,"A=B",
  "Trig identity",0,"A=B",
  "Pythagorean identity","tan(x)^2=sec(x)^2-1","A=B",
  "Expand",0,"A=B",
  "Complete the square",0,"A=B",
  "Simplify",0,"A=B",
  "Trig substitution",0,"V=B,dV=C*dA",
  "",0,"integrate(A,V)=integrate(B,C)",
  "Back to x",0,"integrate(A,V)=B",
  "Evaluate from a to b","integrate(f,x,a,b)=F(b)-F(a)","A=B",
  "",0,"A=B",
};

const char * steps_title(const step_t & s){ return RULES[3*s.rule]; }
const char * steps_formula(const step_t & s){ return RULES[3*s.rule+1]; }

// d followed by a name (du, dx): identifiers keep their char pointer, the string one copies it
static gen dname(const gen & v){ return gen(identificateur(std::string("d")+v.print(context0))); }

gen steps_math(const step_t & s,GIAC_CONTEXT){
  gen t(RULES[3*s.rule+2],contextptr);
  // by parts: u and v, unless the variable is one of them (after a substitution u): p and q
  bool uv=s.v.type==_IDNT && (!strcmp(s.v._IDNTptr->id_name,"u") || !strcmp(s.v._IDNTptr->id_name,"v"));
  gen U(identificateur(uv?"p":"u")),W(identificateur(uv?"q":"v"));
  vecteur from=makevecteur(gen(identificateur("A")),gen(identificateur("B")),gen(identificateur("C")),gen(identificateur("V")));
  vecteur to=makevecteur(s.a,s.b,s.c,s.v);
  from.push_back(gen(identificateur("dV"))); to.push_back(dname(s.v));
  from.push_back(gen(identificateur("dA"))); to.push_back(dname(s.a));
  from.push_back(gen(identificateur("U"))); to.push_back(U);
  from.push_back(gen(identificateur("dU"))); to.push_back(dname(U));
  from.push_back(gen(identificateur("dW"))); to.push_back(dname(W));
  return subst(t,from,to,true,contextptr);
}

// ---------------------------------------------------------------- state and helpers
static std::vector<step_t> * out_;
static unsigned char depth_;
static int budget_;
static const context * ctx_;
static gen * var_; // the variable (no global gen: the app has no static constructors)
#define CTX ctx_
#define X (*var_)

static NOINL void S(int r,const gen & a,const gen & b,const gen & c){
  step_t s; s.rule=r; s.depth=depth_; s.a=a; s.b=b; s.c=c; s.v=X;
  out_->push_back(s);
}
static NOINL void S(int r,const gen & a,const gen & b){ S(r,a,b,zero); }
static NOINL size_t mark(){ return out_->size(); }
static NOINL void cut(size_t m){ out_->resize(m); }
static NOINL gen ev(const gen & g){ return eval(g,1,CTX); }
static NOINL gen rn(const gen & g){ return ratnormal(g,CTX); }
static NOINL gen D(const gen & g,const gen & x){ return ev(derive(g,x,CTX)); }
static NOINL bool dep(const gen & g,const gen & x){
  return g.type==_IDNT ? g==x : (g.type==_SYMB || g.type==_VECT) && depend(g,*x._IDNTptr);
}
static bool dep(const gen & g){ return dep(g,X); }
static NOINL gen sub(const gen & g,const gen & a,const gen & b){ return subst(g,a,b,false,CTX); }
static NOINL gen SYM(const unary_function_ptr * u,const gen & a){ return symbolic(*u,a); }
static NOINL gen SYM2(const unary_function_ptr * u,const gen & a,const gen & b){ return symbolic(*u,makesequence(a,b)); }
static gen Dx(const gen & f){ return SYM2(at_derive,f,X); } // d/dx f, not computed
static gen Ix(const gen & f){ return SYM2(at_integrate,f,X); }
static bool is(const gen & g,const unary_function_ptr * u){ return g.is_symb_of_sommet(u); }
static const gen & arg(const gen & g){ return g._SYMBptr->feuille; }
static gen arg(const gen & g,int i){ return g._SYMBptr->feuille[i]; }

// the factors of a product or the terms of a sum, flattened
static NOINL void flat(const unary_function_ptr * u,const gen & g,vecteur & v){
  if (is(g,u) && arg(g).type==_VECT){
    const vecteur & w=*arg(g)._VECTptr;
    for (unsigned i=0;i<w.size();++i) flat(u,w[i],v);
  }
  else v.push_back(g);
}
// constant factors in c, the others in v
static NOINL void factors(const gen & f,vecteur & c,vecteur & v){
  vecteur w; flat(at_prod,f,w);
  for (unsigned i=0;i<w.size();++i)
    (dep(w[i])?v:c).push_back(w[i]);
}
static gen prodof(const vecteur & v){ return v.empty()?gen(1):v.size()==1?v[0]:symb_prod(v); }

// The parsed text as sums and products: a-b is a+(-b), a/b a*inv(b), the inverse of a product the
// product of the inverses, nested sums and products flattened, numbers evaluated (1*1/3 is 1/3).
static bool numeric(const gen & g){ // no identifier (lidnt skips the bound ones)
  if (g.type==_IDNT) return false;
  if (g.type==_SYMB) return numeric(arg(g));
  if (g.type==_VECT){ for (unsigned i=0;i<g._VECTptr->size();++i) if (!numeric((*g._VECTptr)[i])) return false; }
  return true;
}
static gen inv_of(const gen & g){
  if (g.type<_IDNT) return ev(symb_inv(g));
  if (g.is_prod() && arg(g).type==_VECT){
    vecteur v=*arg(g)._VECTptr;
    for (unsigned i=0;i<v.size();++i) v[i]=inv_of(v[i]);
    return symb_prod(v);
  }
  if (g.is_inv()) return arg(g);
  return symb_inv(g);
}
static NOINL gen norm(const gen & g){
  if (g.type!=_SYMB) return g;
  if (numeric(g)) return ev(g);
  const unary_function_ptr & u=g._SYMBptr->sommet;
  gen a=arg(g);
  if (a.type==_VECT){
    vecteur v=*a._VECTptr;
    for (unsigned i=0;i<v.size();++i) v[i]=norm(v[i]);
    a=gen(v,a.subtype);
  }
  else a=norm(a);
  vecteur v;
  if (u==at_binary_minus && a.type==_VECT && a._VECTptr->size()==2){
    flat(at_plus,a[0],v); flat(at_plus,a[1].type<_IDNT?-a[1]:symb_neg(a[1]),v); return symb_plus(v);
  }
  if (u==at_division && a.type==_VECT && a._VECTptr->size()==2){
    flat(at_prod,a[0],v); flat(at_prod,inv_of(a[1]),v); return symb_prod(v);
  }
  if (u==at_inv) return inv_of(a);
  if ((u==at_plus || u==at_prod) && a.type==_VECT){
    flat(&u,SYM(&u,a),v); return SYM(&u,gen(v,_SEQ__VECT));
  }
  if (u==at_neg && a.type<_IDNT) return -a;
  if (u==at_sqrt && a.is_pow() && arg(a,0).type==_IDNT) // sqrt(x^7) is x^(7/2)
    return symb_pow(arg(a,0),ev(arg(a,1)/2));
  return SYM(&u,a);
}

// a = f numerically at a few points, the other variables given values (giac's simplify is slow
// on the calculator)
static NOINL bool same_num(const gen & a0,const gen & f0,const gen & x){
  gen d=a0-f0,f=f0;
  vecteur ids=lidnt(makevecteur(d,f));
  for (unsigned i=0;i<ids.size();++i)
    if (ids[i]!=x){ d=sub(d,ids[i],0.71+0.17*i); f=sub(f,ids[i],0.71+0.17*i); }
  static const double pts[]={0.37,1.29,2.13,-0.61,3.7};
  int good=0;
  for (int i=0;i<5 && good<3;++i){
    gen y=evalf(sub(d,x,pts[i]),1,CTX),z=evalf(sub(f,x,pts[i]),1,CTX);
    if ((y.type!=_DOUBLE_ && y.type!=_CPLX) || (z.type!=_DOUBLE_ && z.type!=_CPLX)) continue;
    double ay=evalf_double(abs(y,CTX),1,CTX)._DOUBLE_val,az=evalf_double(abs(z,CTX),1,CTX)._DOUBLE_val;
    if (!(ay<=TOL*(1+az))) return false;
    ++good;
  }
  return good>=2;
}

// ---------------------------------------------------------------- derivatives
static gen dsteps(const gen & f);

// f = outer(inner): the chain rule (inner x: a known derivative)
static NOINL gen chain(const gen & f,const gen & outer,const gen & inner){
  if (inner==X){ S(R_BASICD,f,outer); return outer; }
  S(R_CHAIN,f,outer,inner);
  ++depth_; gen d=dsteps(inner); --depth_;
  gen r=ev(outer*d);
  S(R_EQD,f,r);
  return r;
}
// the product rule, two factors at a time
static gen dprod(const vecteur & v){
  if (v.size()==1) return dsteps(v[0]);
  gen u=v[0],w=v.size()==2?v[1]:symb_prod(vecteur(v.begin()+1,v.end())),f=symb_prod(u,w);
  S(R_PROD,f,u,w);
  ++depth_;
  gen du=dsteps(u),dw=v.size()==2?dsteps(w):dprod(vecteur(v.begin()+1,v.end()));
  --depth_;
  gen r=ev(du*w+u*dw);
  S(R_EQD,f,r);
  return r;
}
// textbook derivatives of sec, csc, cot, tan (giac's are in sin and cos), in t
static gen trig_der(const unary_function_ptr & u,const gen & t){
  if (u==at_sec) return symb_prod(SYM(at_sec,t),symb_tan(t));
  if (u==at_csc) return symb_neg(symb_prod(SYM(at_csc,t),SYM(at_cot,t)));
  if (u==at_cot) return symb_neg(symb_pow(SYM(at_csc,t),2));
  if (u==at_tan) return symb_pow(SYM(at_sec,t),2);
  return 0;
}
static gen dsteps(const gen & f){
  if (!dep(f)) return 0;
  if (f==X) return 1;
  if (f.type!=_SYMB) return D(f,X);
  const unary_function_ptr & u=f._SYMBptr->sommet;
  const gen & a=arg(f);
  if (u==at_plus && a.type==_VECT){
    const vecteur & v=*a._VECTptr;
    vecteur dv,rv;
    for (unsigned i=0;i<v.size();++i)
      if (dep(v[i])) dv.push_back(v[i].is_neg()?symb_neg(Dx(arg(v[i]))):Dx(v[i]));
    bool shown=dv.size()>1;
    if (shown){ S(R_SUMD,f,symb_plus(dv)); ++depth_; }
    for (unsigned i=0;i<v.size();++i)
      if (dep(v[i])) rv.push_back(dsteps(v[i]));
    gen r=ev(symb_plus(rv));
    if (shown){ --depth_; S(R_EQD,f,r); }
    return r;
  }
  if (u==at_neg) return ev(-dsteps(a));
  if (u==at_prod && a.type==_VECT){
    vecteur c,num,den;
    const vecteur & v=*a._VECTptr;
    for (unsigned i=0;i<v.size();++i){
      const gen & w=v[i];
      if (!dep(w)) c.push_back(w);
      else if (w.is_inv() && arg(w)!=X && !(arg(w).is_pow() && arg(arg(w),0)==X)) den.push_back(arg(w));
      else num.push_back(w);
    }
    if (!c.empty()){ // c u
      gen k=ev(prodof(c)),rest=prodof(num);
      for (unsigned i=0;i<den.size();++i) rest=num.empty() && i==0?symb_inv(den[i]):symb_prod(rest,symb_inv(den[i]));
      S(R_CONSTD,f,k,rest);
      ++depth_; gen r=dsteps(rest); --depth_;
      r=ev(k*r);
      S(R_EQD,f,r);
      return r;
    }
    if (!den.empty()){ // u/v
      gen n=prodof(num),d=prodof(den);
      S(R_QUOT,f,n,d);
      ++depth_; gen dn=dsteps(n),dd=dsteps(d); --depth_;
      gen r=(dn*d-n*dd)/symb_pow(d,2);
      S(R_EQD,f,r);
      gen r2=rn(r);
      if (taille(r2,200)<taille(r,200)){ S(R_EQD,f,r2); r=r2; }
      return r;
    }
    return dprod(num);
  }
  if (u==at_inv){ // 1/g
    if (a==X || (a.is_pow() && arg(a,0)==X)){
      gen n=a==X?gen(-1):gen(ev(-arg(a,1))),r=ev(n*pow(X,n-1,CTX));
      S(R_POWD,f,r); return r;
    }
    return chain(f,-symb_inv(symb_pow(a,2)),a);
  }
  if (u==at_pow && a.type==_VECT){
    gen b=a[0],e=a[1];
    if (!dep(e)){
      gen o=ev(e*pow(b,e-1,CTX));
      if (b==X){ S(R_POWD,f,o); return o; }
      return chain(f,o,b);
    }
    if (!dep(b)){
      if (e==X){ gen r=ev(f*ln(b,CTX)); S(R_EXPD,f,r); return r; }
      return chain(f,f*ln(b,CTX),e);
    }
    gen w=symb_exp(e*symb_ln(b));
    S(R_LOGD,f,w);
    gen r=ev(sub(dsteps(w),w,f));
    S(R_EQD,f,r);
    return r;
  }
  if (a.type!=_VECT){ // f(g): a known derivative, or the chain rule
    gen t(identificateur(" t")),o=trig_der(u,t);
    if (is_zero(o)) o=D(SYM(&u,t),t);
    return chain(f,subst(o,t,a,true,CTX),a);
  }
  gen r=D(f,X);
  S(R_BASICD,f,r);
  return r;
}

// ---------------------------------------------------------------- integrals
static gen isteps(const gen & f,int lev);
static const char * const UNAMES[]={"u","w","z","s"};
static int nsub_;

// f = g by rule r, then the integral of g; undef (and no step) if it fails
static NOINL gen via(int r,const gen & f,const gen & g,int lev){
  size_t m=mark();
  S(r,f,g);
  gen F=isteps(g,lev+1);
  if (is_undef(F)) cut(m);
  return F;
}
// the integral of f by the substitution u = ux (du = dux dx): g in the new variable
static NOINL gen subs(const gen & f,const gen & uu,const gen & ux,const gen & dux,const gen & g,int lev,int r,int req,int rback){
  size_t m=mark();
  S(r,uu,ux,dux);
  S(req,f,g,uu);
  gen * old=var_; var_=(gen *)&uu; ++depth_; ++nsub_;
  gen G=isteps(g,lev+1);
  --nsub_; --depth_; var_=old;
  if (is_undef(G)){ cut(m); return G; }
  gen F=ev(sub(G,uu,ux));
  S(rback,f,F);
  return F;
}

// c x^r: the power rule (or 1/x)
static NOINL bool power(const gen & g,gen & F,int & r){
  vecteur l=lvarx(g,X);
  for (unsigned i=0;i<l.size();++i)
    if (l[i]!=X && !(l[i].is_pow() && arg(l[i],0)==X)) return false;
  gen n=normal(X*D(g,X)/g,CTX); // (ratnormal leaves x/(3x) with roots)
  if (dep(n) || n.type==_VECT) return false;
  gen c=normal(g/pow(X,n,CTX),CTX);
  if (dep(c)) return false;
  r=is_minus_one(n)?R_LNI:R_POWI;
  F=r==R_LNI?c*symb_ln(symb_abs(X)):ev(c*pow(X,n+1,CTX)/(n+1));
  return true;
}

// The table: f is recognized by its values at two points (the derivatives of these antiderivatives,
// computed once).
static const char TABLE[]="[exp(x),sin(x),-cos(x),tan(x),-cot(x),sec(x),-csc(x),ln(abs(sec(x))),ln(abs(sin(x))),ln(abs(sec(x)+tan(x))),-ln(abs(csc(x)+cot(x))),atan(x),asin(x),sinh(x),cosh(x),tanh(x)]";
static vecteur * tabF_, * tabV_;
static NOINL gen at2(const gen & g,const gen & x,double p){ return evalf(sub(g,x,p),1,CTX); }
static NOINL bool table(const gen & f,gen & F){
  gen x0(identificateur("x"));
  if (!tabF_){
    tabF_=new vecteur(*gen(TABLE,CTX)._VECTptr);
    tabV_=new vecteur;
    for (unsigned i=0;i<tabF_->size();++i){
      gen d=D((*tabF_)[i],x0);
      tabV_->push_back(makevecteur(at2(d,x0,0.37),at2(d,x0,0.61)));
    }
  }
  gen y1=at2(f,X,0.37),y2=at2(f,X,0.61);
  if (y1.type!=_DOUBLE_ || y2.type!=_DOUBLE_) return false;
  for (unsigned i=0;i<tabF_->size();++i){
    const gen & w=(*tabV_)[i];
    if (w[0].type==_DOUBLE_ && w[1].type==_DOUBLE_ && fabs(w[0]._DOUBLE_val-y1._DOUBLE_val)<TOL*(1+fabs(y1._DOUBLE_val))
        && fabs(w[1]._DOUBLE_val-y2._DOUBLE_val)<TOL*(1+fabs(y2._DOUBLE_val))){
      F=sub((*tabF_)[i],x0,X);
      return true;
    }
  }
  return false;
}

// c (p x^2+q)^e, e = -1 or -1/2: the arctangent, arcsine or logarithm formula
static NOINL bool quadform(const gen & f,gen & F,int & r){
  vecteur c,v; factors(f,c,v);
  if (v.size()!=1) return false;
  gen q=v[0],e=1;
  if (q.is_inv()){ q=arg(q); e=-1; }
  if (q.is_pow()){ e=ev(e*arg(q,1)); q=arg(q,0); }
  gen A,B,C,k=ev(prodof(c));
  if (!is_quadratic_wrt(q,X,A,B,C,CTX) || !is_zero(B) || is_zero(A) || dep(A) || dep(C)) return false;
  A=ev(A); C=ev(C);
  if (is_minus_one(e) && is_positive(A,CTX) && is_positive(C,CTX)){
    r=R_ATAN; F=ev(k/sqrt(A*C,CTX)*SYM(at_atan,X*sqrt(A/C,CTX))); return true;
  }
  if (e!=gen(-1)/2) return false;
  if (is_positive(-A,CTX) && is_positive(C,CTX)){
    r=R_ASIN; F=ev(k/sqrt(-A,CTX)*SYM(at_asin,X*sqrt(-A/C,CTX))); return true;
  }
  if (is_positive(A,CTX)){
    r=R_LNSQ; F=ev(k/sqrt(A,CTX)*symb_ln(symb_abs(X*sqrt(A,CTX)+sqrt(q,CTX)))); return true;
  }
  return false;
}

// a basic integral: power, 1/x, a^x, the table, the quadratic formulas
static bool basic(const gen & f,gen & F,int & r){
  if (f==X){ F=symb_pow(X,2)/2; r=R_POWI; return true; }
  if (f.type!=_SYMB) return false;
  if (power(f,F,r)) return true;
  if (f.is_pow() && !dep(arg(f,0)) && arg(f,1)==X){ F=f/ln(arg(f,0),CTX); r=R_TABLE; return true; }
  if (quadform(f,F,r)) return true;
  r=R_TABLE;
  return taille(f,40)<=32 && table(f,F);
}

static gen sc(const gen & g){ return rn(tan2sincos(g,CTX)); } // tan, sec... in sin and cos

// u-substitution candidates: the arguments of functions, the bases of powers, the functions
// themselves; sin proposes cos (odd powers), cos proposes tan and sec
static void addc(vecteur & v,const gen & g){ if (dep(g) && g!=X && !equalposcomp(v,g)) v.push_back(g); }
static NOINL void cands(const gen & g,vecteur & v){
  if (g.type!=_SYMB) return;
  const unary_function_ptr & u=g._SYMBptr->sommet;
  const gen & a=arg(g);
  if (a.type==_VECT){
    if (u==at_pow && dep(a[0])){
      if (a[0]!=X) addc(v,a[0]);
      else if (a[1].type!=_INT_) addc(v,g); // sqrt(x)
    }
    for (unsigned i=0;i<a._VECTptr->size();++i) cands((*a._VECTptr)[i],v);
    return;
  }
  addc(v,a);
  if (u==at_inv || u==at_neg){ cands(a,v); return; }
  addc(v,g);
  if (u==at_sin) addc(v,symb_cos(a));
  if (u==at_cos){ addc(v,symb_sin(a)); addc(v,symb_tan(a)); addc(v,symb_inv(symb_cos(a))); }
  if (u==at_tan) addc(v,symb_inv(symb_cos(a)));
  cands(a,v);
}
// f dx as g(t) dt with t = u (du = u' dx); undef if it can't
static NOINL gen rewrite(const gen & fe,const gen & du,const gen & u,const gen & t){
  gen a,b,g;
  if (is_linear_wrt(u,X,a,b,CTX)) // (giac's is_rewritable_as_f_of says no for a linear u)
    g=ev(sub(fe/du,X,(t-b)/a));
  else {
    g=rn(complex_subst(fe,u,t,CTX)/du); // u's occurrences first: nothing expanded
    if (dep(g)) g=rn(complex_subst(ev(fe/du),u,t,CTX));
    if (dep(g)) g=rn(complex_subst(sc(fe/du),sc(u),t,CTX));
    if (dep(g)){
      gen fx;
      if (!is_rewritable_as_f_of(rn(fe/du),u,fx,X,CTX) || is_undef(fx)) return undef;
      g=sub(fx,X,t);
    }
  }
  if (dep(g) || is_undef(g)) return undef;
  return recursive_normal(g,CTX);
}
// u under a root or in a denominator (x sqrt(x+2), x/(x+1)^2): a linear u is the textbook way
static NOINL bool rooted(const gen & g,const gen & u){
  if (g.type!=_SYMB) return false;
  const gen & a=arg(g);
  if (g.is_pow() && a[0]==u && !(a[1].type==_INT_ && a[1].val>0)) return true;
  if (g.is_inv() && (a==u || (a.is_pow() && arg(a,0)==u))) return true;
  if (a.type==_VECT){ for (unsigned i=0;i<a._VECTptr->size();++i) if (rooted((*a._VECTptr)[i],u)) return true; return false; }
  return rooted(a,u);
}
static NOINL gen usub(const gen & f,int lev){
  if (nsub_>3) return undef;
  vecteur cv; cands(f,cv);
  gen uu=gen(identificateur(UNAMES[nsub_])),fe=ev(f);
  // every candidate first: a linear u giving a known integral, then the simplest new integral
  vecteur ok,gs; std::vector<int> sz;
  for (unsigned i=0;i<cv.size() && budget_>0;++i){
    gen u=cv[i],du=D(u,X),F,a,b; int r;
    budget_-=2;
    if (is_zero(du)) continue;
    gen g=rewrite(fe,du,u,uu);
    if (is_undef(g) || is_inf(g) || is_zero(g)) continue;
    int score=taille(g,1000);
    if (is_linear_wrt(u,X,a,b,CTX)){ // cos(2x): u = 2x; a linear u otherwise only under roots
      vecteur c2,v2; gen * old=var_; var_=&uu; factors(g,c2,v2);
      bool known=v2.size()==1 && basic(ev(v2[0]),F,r);
      var_=old;
      if (known) score=-1;
      else if (!rooted(fe,u)) continue;
    }
    ok.push_back(u); gs.push_back(g); sz.push_back(score);
  }
  while (!ok.empty()){
    unsigned b=0;
    for (unsigned i=1;i<ok.size();++i) if (sz[i]<sz[b]) b=i;
    gen u=ok[b],g=gs[b]; int s=sz[b];
    ok.erase(ok.begin()+b); gs.erase(gs.begin()+b); sz.erase(sz.begin()+b);
    if (s>2*int(taille(fe,1000))+8) continue; // no simpler (taille is unsigned)
    gen F=subs(f,uu,u,D(u,X),g,lev,R_SUB,R_SUBEQ,R_BACK);
    if (!is_undef(F)) return F;
  }
  return undef;
}

// LIATE rank of a factor (lower: the better u)
static NOINL bool linarg(const gen & g){ gen a,b; return g.type==_SYMB && is_linear_wrt(arg(g),X,a,b,CTX); }
static NOINL int liate(const gen & g0){
  gen g=g0;
  if (g.is_pow() && arg(g,1).type==_INT_ && arg(g,1).val>0) g=arg(g,0); // ln(x)^2
  if (is(g,at_ln)) return 0;
  if (is(g,at_atan) || is(g,at_asin) || is(g,at_acos)) return 1;
  vecteur l=lvarx(g0,X);
  if (l.size()==1 && l[0]==X && !dep(_denom(g0,CTX))) return 2; // a polynomial
  if ((is(g0,at_sin) || is(g0,at_cos) || is(g0,at_sinh) || is(g0,at_cosh)) && linarg(g0)) return 3;
  if (is(g0,at_exp) && linarg(g0)) return 4;
  if (g0.is_pow() && !dep(arg(g0,0)) && !dep(D(arg(g0,1),X))) return 4;
  return 9;
}
// by parts: u by LIATE, v the integral of dv (found quietly), rest = v du, k the constant
static NOINL bool parts1(const gen & f,int lev,gen & u,gen & dv,gen & du,gen & V,gen & k,gen & rest){
  gen fe=ev(f);
  vecteur l=lvarx(fe,X);
  if (l.size()==1 && l[0]==X) return false; // a rational fraction: partial fractions
  vecteur c,v; factors(fe,c,v);
  if (v.empty() || v.size()>2) return false;
  int best=-1,br=9;
  for (unsigned i=0;i<v.size();++i){ int r=liate(v[i]); if (r<br){ br=r; best=i; } }
  if (best<0 || (v.size()==1 && br>1)) return false;
  if (v.size()==2){
    int o=liate(v[1-best]);
    if ((br==3 && o==3) || o<br || (o>4 && br>2)) return false; // trig times trig: identities
  }
  u=v[best]; dv=v.size()==2?v[1-best]:gen(1);
  du=D(u,X);
  size_t m=mark(); unsigned char d=depth_; int b=budget_;
  V=isteps(dv,lev+3);
  cut(m); depth_=d; budget_=b-4;
  if (is_undef(V)) return false;
  V=ev(V); k=ev(prodof(c)); rest=rn(V*du);
  return true;
}
static NOINL gen parts(const gen & f,int lev){
  gen u,dv,du,V,k,rest;
  if (!parts1(f,lev,u,dv,du,V,k,rest)) return undef;
  size_t m=mark();
  S(R_PARTS,u,dv); S(R_PARTSV,du,V);
  S(R_PARTS2,f,k*(u*V-Ix(rest)));
  // e^x sin x: a second round brings the integral back, ratio times
  gen u2,dv2,du2,V2,k2,rest2;
  if (parts1(rest,lev+1,u2,dv2,du2,V2,k2,rest2)){
    gen ratio=rn(sc(k2*rest2)/sc(ev(f)));
    if (!dep(ratio) && !is_zero(ratio) && !is_zero(ev(1-k*ratio))){
      ++depth_;
      S(R_PARTS,u2,dv2); S(R_PARTSV,du2,V2);
      S(R_PARTS2,rest,k2*(u2*V2-Ix(rest2)));
      --depth_;
      gen I=ev(k*(u*V-k2*u2*V2)/(1-k*ratio));
      S(R_CYCLE,f,I);
      return I;
    }
  }
  ++depth_; gen R=isteps(rest,lev+1); --depth_;
  if (is_undef(R)){ cut(m); return R; }
  gen F=ev(k*(u*V-R));
  S(R_EQI,f,F);
  return F;
}

// a rational fraction: partial fractions (and the polynomial part)
static NOINL gen ratfrac(const gen & fe,int lev){
  vecteur l=lvarx(fe,X);
  if (l.size()!=1 || l[0]!=X || !dep(_denom(fe,CTX))) return undef;
  gen p=_partfrac(makesequence(fe,X),CTX);
  if (is_undef(p) || p==fe || !p.is_plus()) return undef;
  return via(R_PFRAC,fe,p,lev);
}
// sin^m cos^n with m and n even: the identities first (odd powers: a substitution)
static NOINL bool even_trig(const gen & fe){
  vecteur c,v; factors(fe,c,v);
  if (v.empty()) return false;
  for (unsigned i=0;i<v.size();++i){
    gen b=v[i]; int n=1;
    if (b.is_pow() && arg(b,1).type==_INT_){ n=arg(b,1).val; b=arg(b,0); }
    if ((!is(b,at_sin) && !is(b,at_cos)) || n%2) return false;
  }
  return true;
}
// powers and products of sin and cos: power reduction, product to sum
static NOINL gen trigid(const gen & f,int lev){
  gen fe=sc(ev(f));
  vecteur l=lvarx(fe,X);
  if (l.empty()) return undef;
  for (unsigned i=0;i<l.size();++i)
    if (!is(l[i],at_sin) && !is(l[i],at_cos)) return undef;
  if (dep(_denom(fe,CTX))) return undef;
  gen t=_tlin(fe,CTX);
  if (t==fe || is_undef(t)) return undef;
  return via(R_TRIGID,fe,t,lev);
}
// tan(a)^n, |n| >= 2: tan^2 = sec^2 - 1 (cot: csc^2 - 1)
static NOINL gen pyth(const gen & fe,int lev){
  vecteur c,v; factors(fe,c,v);
  for (unsigned i=0;i<v.size();++i){
    const gen & w=v[i];
    if (!w.is_pow() || !is(arg(w,0),at_tan) || arg(w,1).type!=_INT_) continue;
    int n=arg(w,1).val;
    if (n<2 && n>-2) continue;
    gen a=arg(arg(w,0)),id=n>0?symb_pow(symb_inv(symb_cos(a)),2)-1:symb_pow(symb_inv(symb_sin(a)),2)-1;
    gen g=expand(ev(fe/w*pow(symb_tan(a),n>0?n-2:n+2,CTX)*id),CTX);
    return via(R_PYTH,fe,g,lev);
  }
  return undef;
}
// a (x+h)^2 + m under a power: complete the square
static NOINL gen square(const gen & fe,int lev){
  vecteur c,v; factors(fe,c,v);
  if (v.size()!=1) return undef;
  gen q=v[0],e=1;
  if (q.is_inv()){ q=arg(q); e=-1; }
  if (q.is_pow()){ e=ev(e*arg(q,1)); q=arg(q,0); }
  gen A,B,C;
  if (!is_quadratic_wrt(q,X,A,B,C,CTX) || is_zero(A) || is_zero(B) || dep(A) || dep(B) || dep(C)) return undef;
  gen sq=A*symb_pow(X+ev(B/(2*A)),2)+ev(C-B*B/(4*A));
  size_t m=mark();
  S(R_SQUARE,q,sq);
  gen F=isteps(prodof(c)*symb_pow(sq,e),lev+1);
  if (is_undef(F)) cut(m);
  return F;
}
// every kernel a power of x (and no rational fraction): expanding gives powers
static NOINL bool powers_only(const gen & fe){
  if (fe.is_plus()) return false;
  vecteur l=lvarx(fe,X);
  for (unsigned i=0;i<l.size();++i)
    if (l[i]!=X && !(l[i].is_pow() && arg(l[i],0)==X)) return false;
  gen d=_denom(fe,CTX);
  vecteur ld=lvarx(d,X);
  return ld.empty() || (ld.size()==1 && is_zero(rn(X*D(d,X)/d-_degree(makesequence(d,X),CTX))));
}
static NOINL gen expandit(const gen & fe,int lev){
  if (fe.is_plus() || taille(fe,64)>48) return undef;
  gen t=expand(fe,CTX);
  if (!t.is_plus()) return undef;
  return via(R_EXPAND,fe,t,lev);
}
// a shorter integrand: tan(x)(cot(x)-cos(x)) is 1-sin(x)
static NOINL gen simp(const gen & fe,int lev){
  if (taille(fe,64)>48) return undef;
  gen t=_simplify(fe,CTX);
  if (t==fe || is_undef(t) || taille(t,64)>=taille(fe,64)) return undef;
  return via(R_SIMP,fe,t,lev);
}
// sqrt(a^2-x^2), sqrt(a^2+x^2), sqrt(x^2-a^2): x = a sin t, a tan t, a sec t
static NOINL gen trigsub(const gen & f,int lev){
  if (nsub_>3) return undef;
  gen fe=ev(f),q;
  vecteur l=lvarx(fe,X);
  for (unsigned i=0;i<l.size();++i)
    if (l[i].is_pow() && arg(l[i],1)==gen(1)/2){ q=arg(l[i],0); break; }
  gen A,B,C;
  if (is_zero(q) || !is_quadratic_wrt(q,X,A,B,C,CTX) || !is_zero(B) || is_zero(A) || dep(A) || dep(C)) return undef;
  gen t(identificateur("t")),xt,root;
  if (is_positive(-A,CTX) && is_positive(C,CTX)){ xt=sqrt(ev(-C/A),CTX)*symb_sin(t); root=sqrt(C,CTX)*symb_cos(t); }
  else if (is_positive(A,CTX) && is_positive(C,CTX)){ xt=sqrt(ev(C/A),CTX)*symb_tan(t); root=sqrt(C,CTX)/symb_cos(t); }
  else if (is_positive(A,CTX) && is_positive(-C,CTX)){ xt=sqrt(ev(-C/A),CTX)/symb_cos(t); root=sqrt(ev(-C),CTX)*symb_tan(t); }
  else return undef;
  gen dx=D(xt,t),g=sc(ev(sub(complex_subst(fe,symb_pow(q,gen(1)/2),root,CTX),X,xt)*dx));
  if (dep(g)) return undef;
  size_t m=mark();
  S(R_TSUB,t,xt,dx);
  S(R_TSUBEQ,f,g,t);
  gen * old=var_; var_=&t; ++depth_; ++nsub_;
  gen G=isteps(g,lev+1);
  --nsub_; --depth_; var_=old;
  gen inv=is_undef(G)?G:_solve(makesequence(symb_equal(X,xt),t),CTX);
  if (inv.type==_VECT && !inv._VECTptr->empty()) inv=inv._VECTptr->front();
  if (is_undef(G) || inv.type==_VECT || is_undef(inv)){ cut(m); return undef; }
  gen F=_simplify(sub(G,t,inv),CTX);
  S(R_BACKX,f,F);
  return F;
}

static gen isteps(const gen & f0,int lev){
  if (lev>7 || --budget_<0 || ctrl_c || interrupted) return undef;
  gen f=norm(f0),F;
  if (!dep(f)){ F=f*X; S(R_CONSTANT,f,F); return F; }
  if (f.is_plus() && arg(f).type==_VECT){ // term by term
    const vecteur & v=*arg(f)._VECTptr;
    vecteur iv,rv;
    for (unsigned i=0;i<v.size();++i) iv.push_back(Ix(v[i]));
    size_t m=mark();
    S(R_SUMI,f,symb_plus(iv));
    ++depth_;
    for (unsigned i=0;i<v.size();++i){
      gen G=isteps(v[i],lev+1);
      if (is_undef(G)){ --depth_; cut(m); return G; }
      rv.push_back(G);
    }
    --depth_;
    F=ev(symb_plus(rv));
    S(R_EQI,f,F);
    return F;
  }
  if (f.is_neg()){
    F=isteps(arg(f),lev);
    return is_undef(F)?F:ev(-F);
  }
  vecteur c,v; factors(f,c,v);
  gen k=ev(prodof(c));
  if (!c.empty() && !v.empty()){
    gen g=prodof(v);
    if (!is_one(k)){ // c times the integral of g
      size_t m=mark();
      int r; gen G;
      if (basic(ev(g),G,r)){ F=ev(k*G); S(r,f,F); return F; } // 3x^2: one step
      S(R_CONSTI,f,k,g);
      G=isteps(g,lev);
      if (is_undef(G)){ cut(m); return G; }
      F=ev(k*G);
      S(R_EQI,f,F);
      return F;
    }
    f=g;
  }
  gen fe=ev(f);
  int r;
  if (basic(fe,F,r)){ S(r,f,F); return F; }
  if (even_trig(fe) && !is_undef(F=trigid(f,lev))) return F;
  if (powers_only(fe) && !is_undef(F=expandit(fe,lev))) return F;
  if (!is_undef(F=usub(f,lev))) return F;
  if (!is_undef(F=ratfrac(fe,lev))) return F;
  if (!is_undef(F=pyth(fe,lev))) return F;
  if (!is_undef(F=parts(f,lev))) return F;
  if (!is_undef(F=trigid(f,lev))) return F;
  if (!is_undef(F=square(fe,lev))) return F;
  if (!is_undef(F=expandit(fe,lev))) return F;
  if (!is_undef(F=trigsub(f,lev))) return F;
  return simp(fe,lev);
}

// ---------------------------------------------------------------- entry
bool steps_of(const gen & input0,std::vector<step_t> & out,GIAC_CONTEXT){
  ctx_=contextptr; out_=&out; depth_=0; nsub_=0;
  gen input=norm(input0);
  if (input.type!=_SYMB || arg(input).type!=_VECT) return false;
  const vecteur & v=*arg(input)._VECTptr;
  if (v.size()<2 || v[1].type!=_IDNT) return false;
  gen x=v[1]; var_=&x;
  if (is(input,at_derive) && v.size()==2){
    gen r=dsteps(v[0]);
    return same_num(ev(r),D(v[0],x),x);
  }
  if (is(input,at_integrate) && (v.size()==2 || v.size()==4)){
    budget_=150;
    gen F=isteps(v[0],0);
    if (is_undef(F) || !same_num(D(F,x),ev(v[0]),x)) return false;
    if (v.size()==4){ // the fundamental theorem: F(b) - F(a)
      gen a=ev(v[2]),b=ev(v[3]);
      gen Fb=sub(F,x,b),Fa=sub(F,x,a);
      S(R_FTC,input,SYM2(at_binary_minus,Fb,Fa));
      S(R_EQV,input,_simplify(ev(Fb)-ev(Fa),CTX));
    }
    return true;
  }
  return false;
}
#endif
