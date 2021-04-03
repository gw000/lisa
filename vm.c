#include "lips.h"
#include "ev.h"

// "the virtual machine"
// it's a stack machine with one free register (xp)
// that's implemented on top of the C compiler's calling
// convention. this allows us to keep the most important
// state variables in CPU registers at all times while the
// interpreter is running, without any platform-specific code.

// " the interpreter "
// the interpreter is the set of functions of type terp,
// given in the header file and also here:
#define Vm(n,...) Nin obj \
  n(vm v,obj ip,mem fp,mem sp,mem hp,obj xp,##__VA_ARGS__)
// the arguments to a terp function collectively represent the
// runtime state, and the  return value is the result of the
// program. there are six arguments because that's the number
// that the prevalent unix calling convention on AMD64 (System
// V ABI) says should be passed in registers; that's the only
// reason why there aren't more. but it's not too bad; the six
// arguments are:
// - v  : vm instance pointer ; most lips functions take this as the first argument
// - ip : instruction pointer ; the current vm instruction ; function pointer pointer
// - fp : frame pointer       ; current function context
// - sp : stack pointer       ; data/call stack
// - hp : heap pointer        ; the next free heap location
// - xp : return value        ; general-purpose register

// when the interpreter isn't running, the state variables that
// would normally be in registers are stored in slots on the
// vm structure. however while the interpreter is running it
// uses these struct slots to pass and return extra values
// without using the stack. so the interpreter has to be sure
// to restore the current values in the vm struct before it 
// makes any "external" function calls.
#define Pack() (Ip=ip,Sp=sp,Hp=hp,Fp=fp,Xp=xp)
#define Unpack() (fp=Fp,hp=Hp,sp=Sp,ip=Ip,xp=Xp)
#define CallC(...)(Pack(),(__VA_ARGS__),Unpack())

// the return value of a terp function is usually a call
// to another terp function. the C compiler has to optimize
// these tail calls or else every one will grown the call stack.
#define Jump(f,...) R (f)(v,ip,fp,sp,hp,xp,##__VA_ARGS__)
#define Cont(n, x) R ip+=w2b(n), xp=x, G(ip)(v,ip,fp,sp,hp,xp)
#define Ap(f,x) R G(f)(v,f,fp,sp,hp,x)
#define Go(f,x) R f(v,ip,fp,sp,hp,x)
#define Next(n) Ap(ip+w2b(n),xp)

// a special case is when garbage collection is necessary.
// this occurs near the beginning of a function. if enough
// memory is not available the interpret jumps to a specific
// terp function
St Vm(gc) {
  num n = Xp; // this is the required amount of memory, passed in Xp so as not to spill it
  CallC(reqsp(v, n)); Next(0); }
// that stores the state and calls the garbage collector;
// afterwards it jumps back to the instruction that called it.
// therefore anything before the Have() macro will be executed
// twice if garbage collection happens! there should be no side
// effects before Have() or similar.
#define avail (sp-hp)
#define Have(n) if (avail < n) Jump((Xp=n,gc))
#define Have1() if (hp == sp) Jump((Xp=1,gc)) // common case, faster comparison

// the frame structure holds the current function context.
Ty Sr fr { obj clos, retp, subd, argc, argv[]; } *fr;
#define ff(x)((fr)(x))
#define Clos ff(fp)->clos
#define Retp ff(fp)->retp
#define Subd ff(fp)->subd
#define Argc ff(fp)->argc
#define Argv ff(fp)->argv
// the pointer to the local variables array isn't in the frame struct. it
// isn't present for all functions, but if it is it's in the word of memory
// immediately preceding the frame pointer.
#define Locs fp[-1]
// if a function has locals, this will have been initialized by the
// by the time they are referred to. the wrinkle in the representation
// gives a small but significant benefit to general function call
// performance and should be extended to the closure pointer, which is often
// nil.

// these are for when something goes wrong
static Vm(nope);
#define TypeCheck(x,t) if(kind(x)!=t)Jump(nope)
#define Arity(n) if(n>Argc)Jump(nope)
#define ArityCheck(n) Arity(putnum(n))


// " virtual machine instructions "
// more or less in ascending order of interpreterness
//
// load instructions
Vm(immv) { xp = O GF(ip); Next(2); }
Vm(unit) { xp = nil; Next(1); }
Vm(one)  { xp = Pn(1); Next(1); }
Vm(zero) { xp = Pn(0); Next(1); }

// indexed instructions
#define fast_ref(b) (*(num*)((num)(b)+(num)GF(ip)-Num))
// pointer arithmetic works because fixnums are premultiplied by sizeof(obj)
Vm(argn) { xp = fast_ref(Argv); Next(2); }
Vm(locn) { xp = fast_ref(AR(Locs)); Next(2); }
Vm(clon) { xp = fast_ref(AR(Clos)); Next(2); }
Vm(arg0) { xp = Argv[0]; Next(1); }
Vm(arg1) { xp = Argv[1]; Next(1); }
Vm(loc0) { xp = AR(Locs)[0]; Next(1); }
Vm(loc1) { xp = AR(Locs)[1]; Next(1); }
Vm(clo0) { xp = AR(Clos)[0]; Next(1); }
Vm(clo1) { xp = AR(Clos)[1]; Next(1); }

// environment operations
// stack push
Vm(push) { Have1(); *--sp = xp; Next(1); }
// set a global variable
Vm(tbind) { CallC(tblset(v, Dict, (obj) GF(ip), xp)); Next(2); }

// set a local variable
Vm(setl) { fast_ref(AR(Locs)) = xp; Next(2); }

// initialize local variable slots
Vm(prel) {
  num n = Gn(GF(ip));
  Have(n + 2);
  tup t = (tup) hp;
  hp += n + 1;
  t->len = n;
  while (n--) t->xs[n] = nil;
  *--sp = puttup(t);
  Next(2); }

// late bind
Vm(lbind) {
  obj w = (obj) GF(ip),
      d = XY(w), y = X(w);
  w = tblget(v, d, xp = YY(w));
  if (!w) Jump(nope);
  xp = w;
  if (Gn(y) != 8) TypeCheck(xp, Gn(y));
  terp *q = G(FF(ip));
  if ((q == call || q == rec) && homp(xp)) {
    obj aa = (obj)GF(FF(ip));
    if (G(xp) == arity && aa >= (obj)GF(xp))
      xp += W2; }
  G(ip) = immv;
  GF(ip) = (terp*) xp;
  Next(2); }


// control flow
// return to C
Vm(yield) { R Pack(), xp; }

// conditional jumps
Vm(branch) { ip = xp == nil ? O FF(ip) : O GF(ip);      Next(0); }
Vm(barnch) { ip = xp == nil ? O GF(ip) : O FF(ip);      Next(0); }
Vm(brlt)   { ip = *sp++ <  xp ? O GF(ip) : O FF(ip);    Next(0); }
Vm(brgteq) { ip = *sp++ <  xp ? O FF(ip) : O GF(ip);    Next(0); }
Vm(brlteq) { ip = *sp++ <= xp ? O GF(ip) : O FF(ip);    Next(0); }
Vm(brgt)   { ip = *sp++ <= xp ? O FF(ip) : O GF(ip);    Next(0); }
Vm(breq)   { ip = eql(*sp++, xp) ? O GF(ip) : O FF(ip); Next(0); }
Vm(brne)   { ip = eql(*sp++, xp) ? O FF(ip) : O GF(ip); Next(0); }

// unconditional jumps
Vm(jump) { Ap(O GF(ip), xp); }
Vm(clos) { // with closure
  Clos = O GF(ip);
  ip = O G(FF(ip));
  Next(0); }

// return from a function
Vm(ret) {
  ip = Retp;
  sp = (mem) ((num) Argv + Argc - Num);
  fp = (mem) ((num)   sp + Subd - Num);
  Next(0); }

// regular function call
Vm(call) {
  Have(Size(fr));
  obj adic = O GF(ip);
  num off = fp - (mem) ((num) sp + adic - Num);
  fp = sp -= Size(fr);
  Retp = Ph(ip+W2);
  Subd = Pn(off);
  Clos = nil;
  Argc = adic;
  Ap(xp, nil); }

// general tail call
Vm(rec) {
  num adic = Gn(GF(ip));
  if (Argc == (obj)GF(ip)) {
    for (mem p = Argv; adic--; *p++ = *sp++);
    sp = fp;
    Ap(xp, nil); }


  obj off = Subd, rp = Retp; // save return info
  mem src = sp + adic;
  // overwrite current frame with new frame
  sp = Argv + Gn(Argc);
  // important to copy in reverse order since they
  // may overlap
  for (num i = adic; i--; *--sp = *--src);
  fp = sp -= Size(fr);
  Retp = rp;
  Argc = Pn(adic);
  Subd = off;
  Clos = nil;
  Ap(xp, nil); }

// type/arity checking
Vm(arity) { Arity((obj)GF(ip)); Next(2); }
#define tcn(k) {terp*r=kind(xp)==k?G(ip+=W):nope;Jump(r);}
Vm(idnum) { tcn(Num); }
Vm(idtwo) { tcn(Two); }
Vm(idhom) { tcn(Hom); }
Vm(idtbl) { tcn(Tbl); }

// continuations
//
// this is a simple but expensive way of doing continuations.
// it would be more memory efficient to do a copy-on-write
// kind of thing where the stack is only copied if the function
// returns. a spaghetti stack would be another option but it
// would be slower. faster continuations at the cost of slower
// function calls seems like a bad deal given the relative
// frequency of the two.
Vm(ccc_u) {
  ArityCheck(1);
  obj x = Argv[0];
  TypeCheck(x, Hom);
  // we need space for:
  // the entire stack
  // the frame offset
  // the length (to put it all in a tuple)
  // the continuation thread (4 words)
  num ht = Pool + Len - sp;
  Have(ht + 6);
  tup t = (tup) hp;
  hp += ht + 2;
  t->len = ht + 1;
  t->xs[0] = Pn(fp - sp);
  memcpy(t->xs+1, sp, w2b(ht));
  hom c = (hom) hp;
  hp += 4;
  c[0] = cont;
  c[1] = (terp*) puttup(t);
  c[2] = NULL;
  c[3] = (terp*) c;
  Argv[0] = Ph(c);
  Ap(x, nil); }

// call a continuation
Vm(cont) {
  tup t = gettup(GF(ip));
  Have(t->len - 1);
  xp = Gn(Argc) == 0 ? nil : *Argv;
  num off = Gn(t->xs[0]);
  sp = Pool + Len - (t->len - 1);
  fp = sp + off;
  memcpy(sp, t->xs+1, w2b(t->len-1));
  Jump(ret); }

Vm(ap_u) {
  ArityCheck(2);
  obj x = Argv[0];
  TypeCheck(x, Hom);
  obj y = Argv[1];
  num adic = llen(y);
  Have(adic);
  obj off = Subd, rp = Retp;
  sp = Argv + Gn(Argc) - adic;
  for (num j = 0; j < adic; sp[j++] = X(y), y = Y(y));
  fp = sp -= Size(fr);
  Retp = rp;
  Argc = Pn(adic);
  Subd = off;
  Clos = nil;
  Ap(x, nil); }


// instructions used by the compiler
Vm(hom_u) {
  ArityCheck(1);
  TypeCheck(*Argv, Num);
  num len = Gn(*Argv) + 2;
  Have(len);
  hom h = (hom) hp;
  hp += len;
  memset(h, -1, w2b(len));
  h[len-1] = (terp*) h;
  h[len-2] = NULL;
  Go(ret, Ph(h+len-2)); }

Vm(tset) {
  obj x = *sp++, y = *sp++;
  CallC(x = tblset(v, xp, x, y));
  Ap(ip+W, x); }
Vm(emx) {
  hom h = Gh(*sp++) - 1;
  G(h) = (terp*) xp;
  Ap(ip+W, Ph(h)); }
Vm(emi) {
  hom h = Gh(*sp++) - 1;
  G(h) = (terp*) Gn(xp);
  Ap(ip+W, Ph(h)); }
Vm(emx_u) {
  ArityCheck(2);
  TypeCheck(Argv[1], Hom);
  hom h = Gh(Argv[1]) - 1;
  G(h) = (terp*) Argv[0];
  Go(ret, Ph(h)); }
Vm(emi_u) {
  ArityCheck(2);
  TypeCheck(Argv[0], Num);
  TypeCheck(Argv[1], Hom);
  hom h = Gh(Argv[1]) - 1;
  G(h) = (terp*) Gn(Argv[0]);
  Go(ret, Ph(h)); }
Vm(hom_geti_u) {
  ArityCheck(1);
  TypeCheck(Argv[0], Hom);
  Go(ret, Pn(G(Argv[0]))); }
Vm(hom_getx_u) {
  ArityCheck(1);
  TypeCheck(Argv[0], Hom);
  Go(ret, (obj)G(Argv[0])); }
Vm(hom_seek_u) {
  ArityCheck(2);
  TypeCheck(Argv[0], Hom);
  TypeCheck(Argv[1], Num);
  Go(ret, Ph(Gh(Argv[0])+Gn(Argv[1]))); }

// hash tables
Vm(tblg) {
  ArityCheck(2);
  TypeCheck(Argv[0], Tbl);
  xp = tblget(v, Argv[0], Argv[1]);
  Go(ret, xp ? xp : nil); }
Vm(tget) {
  xp = tblget(v, xp, *sp++);
  Ap(ip+W, xp ? xp : nil); }
Vm(tblc) {
  ArityCheck(2);
  TypeCheck(Argv[0], Tbl);
  xp = tblget(v, Argv[0], Argv[1]);
  Go(ret, xp ? Pn(0) : nil); }

static obj tblss(vm v, num i, num l) {
  mem fp = Fp;
  return i > l-2 ? Argv[i-1] :
    (tblset(v, Xp, Argv[i], Argv[i+1]),
     tblss(v, i+2, l)); }

Vm(tbls) {
  obj x = nil;
  ArityCheck(1);
  xp = *Argv;
  TypeCheck(xp, Tbl);
  CallC(x = tblss(v, 1, Gn(Argc)));
  Go(ret, x); }
Vm(tbld) {
  ArityCheck(2);
  TypeCheck(Argv[0], Tbl);
  obj x = nil;
  CallC(x = tbldel(v, Argv[0], Argv[1]));
  Go(ret, x); }
Vm(tblks) {
  ArityCheck(1);
  TypeCheck(Argv[0], Tbl);
  obj x;
  CallC(x = tblkeys(v, Argv[0]));
  Go(ret, x); }
Vm(tbll) {
  ArityCheck(1);
  TypeCheck(Argv[0], Tbl);
  Go(ret, Pn(gettbl(*Argv)->len)); }
Vm(tblmk) {
  CallC(Xp = table(v),
    tblss(v, 0, Gn(Argc)));
  Go(ret, Xp); }

// string instructions
Vm(strl) {
  ArityCheck(1);
  TypeCheck(*Argv, Oct);
  Go(ret, Pn(getoct(*Argv)->len-1)); }
Vm(strg) {
  ArityCheck(2);
  TypeCheck(Argv[0], Oct);
  TypeCheck(Argv[1], Num);
  Go(ret, Gn(Argv[1]) < getoct(Argv[0])->len-1 ?
    Pn(getoct(Argv[0])->text[Gn(Argv[1])]) :
    nil); }

Vm(strc) {
  num l = Gn(Argc), sum = 0, i = 0;
  while (i < l) {
    obj x = Argv[i++];
    TypeCheck(x, Oct);
    sum += getoct(x)->len - 1; }
  num words = b2w(sum+1) + 1;
  Have(words); oct d = (oct) hp; hp += words;
  d->len = sum + 1;
  d->text[sum] = 0;
  while (i) {
    oct x = getoct(Argv[--i]);
    sum -= x->len - 1;
    memcpy(d->text+sum, x->text, x->len - 1); }
  Go(ret, putoct(d)); }

#define min(a,b)(a<b?a:b)
#define max(a,b)(a>b?a:b)
Vm(strs) {
  ArityCheck(3);
  TypeCheck(Argv[0], Oct);
  TypeCheck(Argv[1], Num);
  TypeCheck(Argv[2], Num);
  oct src = getoct(Argv[0]);
  num lb = Gn(Argv[1]), ub = Gn(Argv[2]);
  lb = max(lb, 0), ub = max(min(ub, src->len-1), lb);
  num words = 1 + b2w(ub - lb + 1);
  Have(words);
  oct dst = (oct) hp; hp += words;
  dst->len = ub - lb + 1;
  dst->text[ub - lb] = 0;
  memcpy(dst->text, src->text + lb, ub - lb);
  Go(ret, putoct(dst)); }

Vm(strmk) {
  num i, l = Gn(Argc)+1, size = 1 + b2w(l);
  Have(size);
  oct s = (oct) hp;
  hp += size;
  for (i = 0; i < l-1; i++) {
    obj x = Argv[i];
    TypeCheck(x, Num);
    if (x == Pn(0)) break;
    s->text[i] = Gn(x); }
  s->text[i] = 0;
  s->len = i+1;
  Go(ret, putoct(s)); }

Vm(vararg) {
  num reqd = Gn(GF(ip)),
      vdicity = Gn(Argc) - reqd;
  ArityCheck(reqd);
  // in this case we need to add another argument
  // slot to hold the nil.
  if (!vdicity) {
    Have1();
    sp = --fp;
    for (num i = 0; i < Size(fr) + reqd; i++)
      fp[i] = fp[i+1];
    Argc += W;
    Argv[reqd] = nil; }
  // in this case we just keep the existing slots.
  // the path is knowable at compile time in many cases
  // so maybe vararg should be two or more different
  // functions.
  else {
    Have(2 * vdicity);
    two t = (two) hp;
    hp += 2 * vdicity;
    for (num i = vdicity; i--;)
      t[i].x = Argv[reqd + i],
      t[i].y = puttwo(t+i+1);
    t[vdicity-1].y = nil;
    Argv[reqd] = puttwo(t); }
  Next(2); }

// the next few functions create and store
// lexical environments.
static Vm(encl) {
  num n = Xp;
  obj x = (obj) GF(ip);
  mem block = hp;
  hp += n;
  obj arg = nil; // optional argument array
  if (n > 11) {
    n -= 12;
    tup t = (tup) block;
    block += 1 + n;
    t->len = n;
    while (n--) t->xs[n] = Argv[n];
    arg = puttup(t); }

  tup t = (tup) block; // compiler thread closure array (1 length 5 elements)
  hom at = (hom) (block+6); // compiler thread (1 instruction 2 data 2 tag)

  t->len = 5; // initialize alpha closure
  t->xs[0] = arg;
  t->xs[1] = xp; // Locs or nil
  t->xs[2] = Clos;
  t->xs[3] = Y(x);
  t->xs[4] = Ph(at);

  at[0] = pc0;
  at[1] = (terp*) puttup(t);
  at[2] = (terp*) X(x);
  at[3] = 0;
  at[4] = (terp*) at;

  Ap(ip+W2, Ph(at)); }

Vm(prencl) {
  num n = Gn(Argc);
  n += n ? 12 : 11;
  Have(n);
  Xp = n;
  Jump(encl); }

Vm(encll) { Go(prencl, Locs); }
Vm(encln) { Go(prencl, nil); }

// this function is run the first time a user
// function with a closure is called. its
// purpose is to reconstruct the enclosing
// environment and call the closure constructor
// thread generated by the compiler. afterwards
// it overwrites itself with a special jump
// instruction that sets the closure and enters
// the function.
Vm(pc0) {
  obj ec = (obj) GF(ip),
      arg = AR(ec)[0],
      loc = AR(ec)[1];
  num adic = nilp(arg) ? 0 : AL(arg);
  Have(Size(fr) + adic + 1);
  num off = (mem) fp - sp;
  G(ip) = pc1;
  sp -= adic;
  for (num z = adic; z--; sp[z] = AR(arg)[z]);
  ec = (obj) GF(ip);
  fp = sp -= Size(fr);
  Retp = ip;
  Subd = Pn(off);
  Argc = Pn(adic);
  Clos = AR(ec)[2];
  if (!nilp(loc)) *--sp = loc;
  ip = AR(ec)[3];
  Next(0); }

// finalize function instance closure
Vm(pc1) {
  G(ip) = clos;
  GF(ip) = (terp*) xp;
  Next(0); }

// this is used to create closures.
Vm(take) {
  num n = Gn((obj)GF(ip));
  Have(n + 1);
  tup t = (tup) hp;
  hp += n + 1;
  t->len = n;
  memcpy(t->xs, sp, w2b(n));
  sp += n;
  Go(ret, puttup(t)); }

// print to console
Vm(em_u) {
  num l = Gn(Argc), i;
  if (l) {
    for (i = 0; i < l - 1; i++)
      emsep(v, Argv[i], stdout, ' ');
    emit(v, xp = Argv[i], stdout); }
  fputc('\n', stdout);
  Jump(ret); }

Vm(pc_u) {
  ArityCheck(1);
  xp = *Argv;
  TypeCheck(xp, Num);
  fputc(Gn(xp), stdout);
  Jump(ret); }

Vm(emse) {
  emsep(v, xp, stdout, Gn(GF(ip)));
  Next(2); }

// pairs
Vm(cons) {
  Have1(); hp[0] = xp, hp[1] = *sp++;
  xp = puttwo(hp); hp += 2; Next(1); }
Vm(car) { Ap(ip+W, X(xp)); }
Vm(cdr) { Ap(ip+W, Y(xp)); }

Vm(cons_u) {
  num aa = Gn(Argc);
  if (!aa) Jump(nope);
  Have(2); hp[0] = Argv[0], hp[1] = aa == 1 ? nil : Argv[1];
  xp = puttwo(hp), hp += 2; Jump(ret); }
Vm(car_u) {
  ArityCheck(1); TypeCheck(*Argv, Two);
  Go(ret, X(*Argv)); }
Vm(cdr_u) {
  ArityCheck(1); TypeCheck(*Argv, Two);
  Go(ret, Y(*Argv)); }

// arithmetic
#define ok Pn(0)
Vm(neg) { Ap(ip+W, Pn(-Gn(xp))); }
Vm(add) {
  xp = xp + *sp++ - Num;
  Next(1); }
Vm(sub) {
  xp = *sp++ - xp + Num;
  Next(1); }
Vm(mul) {
  xp = Pn(Gn(xp) * Gn(*sp++));
  Next(1); }
Vm(dqv) {
  if (xp == Pn(0)) Jump(nope);
  xp = Pn(Gn(*sp++) / Gn(xp));
  Next(1); }
Vm(mod) {
  if (xp == Pn(0)) Jump(nope);
  xp = Pn(Gn(*sp++) % Gn(xp));
  Next(1); }

#define mm_u(_c,_v,_z,op){\
  obj x,m=_z,*xs=_v,*l=xs+_c;\
  if (_c) for(;xs<l;m=m op Gn(x)){\
    x = *xs++; TypeCheck(x, Num);}\
  Go(ret, Pn(m));}
#define mm_u0(_c,_v,_z,op){\
  obj x,m=_z,*xs=_v,*l=xs+_c;\
  if (_c) for(;xs<l;m=m op Gn(x)){\
    x = *xs++; TypeCheck(x, Num);\
    if (x == Pn(0)) Jump(nope);}\
  Go(ret, Pn(m));}

Vm(add_u) {
  mm_u(Gn(Argc), Argv, 0, +); }
Vm(mul_u) {
  mm_u(Gn(Argc), Argv, 1, *); }
Vm(sub_u) {
  num i = Gn(Argc);
  if (i == 0) Go(ret, Pn(0));
  TypeCheck(*Argv, Num);
  if (i == 1) Go(ret, Pn(-Gn(*Argv)));
  mm_u(i-1,Argv+1,Gn(Argv[0]),-); }

Vm(div_u) {
  num i = Gn(Argc);
  if (i == 0) Go(ret, Pn(1));
  TypeCheck(*Argv, Num);
  mm_u0(i-1,Argv+1,Gn(*Argv),/); }
Vm(mod_u) {
  num i = Gn(Argc);
  if (i == 0) Go(ret, Pn(1));
  TypeCheck(*Argv, Num);
  mm_u0(i-1,Argv+1,Gn(*Argv),%); }

// type predicates
Vm(numpp) { xp = nump(xp) ? ok : nil; Next(1); }
Vm(hompp) { xp = homp(xp) ? ok : nil; Next(1); }
Vm(twopp) { xp = twop(xp) ? ok : nil; Next(1); }
Vm(sympp) { xp = symp(xp) ? ok : nil; Next(1); }
Vm(strpp) { xp = octp(xp) ? ok : nil; Next(1); }
Vm(tblpp) { xp = tblp(xp) ? ok : nil; Next(1); }
Vm(nilpp) { xp = nilp(xp) ? ok : nil; Next(1); }
Vm(vecpp) { xp = tupp(xp) ? ok : nil; Next(1); }

// comparison
int eql(obj, obj);

// lists are immutable, so we can opportunistically
// deduplicate them.
static int twoeq(obj a, obj b) {
  if (!eql(X(a), X(b))) return 0;
  X(a) = X(b);
  if (!eql(Y(a), Y(b))) return 0;
  Y(a) = Y(b);
  return 1; }

static int streq(obj a, obj b) {
  oct o = getoct(a), m = getoct(b);
  if (o->len != m->len) return 0;
  for (num i = 0; i < o->len; i++)
    if (o->text[i] != m->text[i]) return 0;
  return 1; }

int eql(obj a, obj b) {
  if (a == b) return 1;
  if (kind(a) != kind(b)) return 0;
  switch (kind(a)) {
    case Two: return twoeq(a, b);
    case Oct: return streq(a, b);
    default: return 0; } }

Vm(lt)    { xp = *sp++ <  xp ? xp : nil; Next(1); }
Vm(lteq)  { xp = *sp++ <= xp ? xp : nil; Next(1); }
Vm(gteq)  { xp = *sp++ >= xp ? xp : nil; Next(1); }
Vm(gt)    { xp = *sp++ >  xp ? xp : nil; Next(1); }
// there should be a separate instruction for simple equality.
Vm(eq)   {
  obj y = *sp++;
  xp = eql(xp, y) ? ok : nil;
  Next(1); }

#define ord_w(r){\
  obj n=Gn(Argc),*xs=Argv,m,*l;\
  switch(n){\
    case 0: no: Go(ret, nil);\
    case 1: break;\
    default: for(l=xs+n-1,m=*xs;xs<l;m=*++xs)\
               if(!(m r xs[1])) goto no;}\
  Go(ret, ok);}

#define ord_wv(r){\
  obj n=Gn(Argc),*xs=Argv,m,*l;\
  switch(n){\
    case 0: no: Go(ret, nil);\
    case 1: break;\
    default: for(l=xs+n-1,m=*xs;xs<l;m=*++xs)\
               if(!(r(m,xs[1]))) goto no;}\
  Go(ret, ok);}

#define ord_v(r) Go(ret, ord_u(Gn(Argc), Argv, r))

Vm(lt_u)   { ord_w(<); }
Vm(lteq_u) { ord_w(<=); }
Vm(eq_u)   { ord_wv(eql); }
Vm(gteq_u) { ord_w(>=); }
Vm(gt_u)   { ord_w(>); }

#define typpp(t) {\
  for (obj *xs = Argv, *l=xs+Gn(Argc);xs<l;)\
    if (kind(*xs++)!=t) Go(ret, nil);\
  Go(ret, ok); }
Vm(nump_u) { typpp(Num); }
Vm(homp_u) { typpp(Hom); }
Vm(strp_u) { typpp(Oct); }
Vm(tblp_u) { typpp(Tbl); }
Vm(twop_u) { typpp(Two); }
Vm(symp_u) { typpp(Sym); }
Vm(nilp_u) { typpp(Nil); }
Vm(vecp_u) { typpp(Tup); }

// stack manipulation
Vm(tuck) { Have1(); sp--, sp[0] = sp[1], sp[1] = xp; Next(1); }
Vm(drop) { sp++; Next(1); }
Vm(dupl) { Have1(); --sp; sp[0] = sp[1]; Next(1); }

// errors
Vm(fail) { Jump(nope); }
Vm(zzz) { exit(EXIT_SUCCESS); }
Vm(gsym_u) {
  Have(Size(sym));
  sym y = (sym) hp; hp += Size(sym);
  y->nom = y->l = y->r = nil;
  y->code = v->count++ * mix;
  Go(ret, putsym(y)); }

// this is for runtime errors from the interpreter, it prints
// a backtrace and everything.
St In __ perrarg(vm v, mem fp) {
  num argc = fp == Pool + Len ? 0 : Gn(Argc), i = 0;
  if (argc) for (fputc(' ', stderr);;fputc(' ', stderr)) {
    obj x = Argv[i++];
    emit(v, x, stderr);
    if (i == argc) break; }
  fputc(')', stderr); }

St Vm(nope) {
  fputs("# (", stderr), emit(v, Ph(ip), stderr),
  perrarg(v, fp);
  fputs(" does not exist\n", stderr);
  for (;;) {
    ip = Retp, fp += Size(fr) + Gn(Argc) + Gn(Subd);
    if (button(Gh(ip))[-1] == yield) break;
    fputs("#  in ", stderr), emsep(v, Ph(ip), stderr, '\n'); }
  return Hp = hp, restart(v); }

obj restart(vm v) {
  Fp = Sp = Pool + Len;
  Xp = Ip = nil;
  v->mem_root = NULL;
  //reqsp(v, 0);
  longjmp(v->restart, 1); }

Vm(hom_fin_u) {
  ArityCheck(1);
  TypeCheck(*Argv, Hom);
  obj a = *Argv;
  GF(button(Gh(a))) = (terp*) a;
  Go(ret, a); }

Vm(ev_u) {
  ArityCheck(1);
  obj x;
  CallC(
   x = compile(v, *Argv),
   x = G(x)(v, x, Fp, Sp, Hp, nil));
  Go(ret, x); }
