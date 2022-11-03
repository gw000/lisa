#include "la.h"

// function functions
//
// functions are laid out in memory like this
//
// *|*|*|*|*|*|?|0|^
// * = function pointer or inline value
// ? = function name / metadata (optional)
// 0 = null
// ^ = pointer to head of function
//
// this way we can support internal pointers for branch
// destinations, return addresses, etc, while letting
// the garbage collector always find the head.
//
// two easy potential optimizations are:
// - add a tail pointer to the start of the function,
//   so GC can find the head quickly (since often we
//   won't have an internal pointer)
// - tag the tail/head pointers instead of using a null
//   sentinel (but then the C compiler would need to
//   align functions)

// allocate a thread
mo mkmo(la v, size_t n) {
  mo k = cells(v, n + Width(tag));
  return k ? ini_mo(k, n) : k; }

// get the tag at the end of a function
tag button(mo k) {
  while (G(k)) k = F(k);
  return (tag) k; }

// instructions for the internal compiler
//
// initialize a function
Vm(hom_u) {
  ArityCheck(1);
  Check(nump(fp->argv[0]));
  size_t len = getnum(fp->argv[0]);
  Have(len);
  mo k = setw(ini_mo(hp, len), nil, len);
  hp += len + Width(tag);
  return ApC(ret, (ob) (k + len)); }

// trim a function after writing out code
Vm(hfin_u) {
  ArityCheck(1);
  xp = fp->argv[0];
  Check(homp(xp) && G(xp) != disp);
  button((mo) xp)->self = (mo) xp;
  return ApC(ret, xp); }

// emit code
Vm(emi_u) {
  ArityCheck(2);
  Check(nump(fp->argv[0]));
  Check(homp(fp->argv[1]));
  mo k = (mo) fp->argv[1] - 1;
  G(k) = (vm*) getnum(fp->argv[0]);
  return ApC(ret, (ob) k); }

// frameless version
Vm(emi) {
  mo k = (mo) *sp++ - 1;
  G(k) = (vm*) getnum(xp);
  return ApN(1, (ob) k); }

// emit data
Vm(emx_u) {
  ArityCheck(2);
  Check(homp(fp->argv[1]));
  mo k = (mo) fp->argv[1] - 1;
  G(k) = (vm*) fp->argv[0];
  return ApC(ret, (ob) k); }

// frameless
Vm(emx) {
  mo k = (mo) *sp++ - 1;
  G(k) = (vm*) xp;
  return ApN(1, (ob) k); }

// read an instruction from a thread (as a fixnum)
Vm(peeki_u) {
  ArityCheck(1);
  xp = fp->argv[0];
  Check(homp(xp));
  return ApC(ret, putnum(G(xp))); }

// read data from a thread (be sure it's really data!)
Vm(peekx_u) {
  ArityCheck(1);
  xp = fp->argv[0];
  Check(homp(xp));
  return ApC(ret, (ob) G(xp)); }

// thread pointer arithmetic -- not bounds checked!
Vm(seek_u) {
  ArityCheck(2);
  Check(homp(fp->argv[0]));
  Check(nump(fp->argv[1]));
  ip = (mo) fp->argv[0];
  xp = getnum(fp->argv[1]);
  return ApC(ret, (ob) (ip + xp)); }

// dispatch a data thread
// TODO maybe we could do this with closures instead?
Vm(disp) { return ApC(((mtbl) GF(ip))->does, xp); }

// closure functions
//
// pop some things off the stack into an array.
Vm(take) {
  ob n = getnum((ob) GF(ip));
  Have(n + Width(tag));
  mo k = ini_mo(cpyw(hp, sp, n), n);
  hp += n + Width(tag);
  return ApC(ret, (ob) k); }

// this function is run the first time a user
// function with a closure is called. its
// purpose is to reconstruct the enclosing
// environment and call the closure constructor
// thread generated by the compiler. afterwards
// it overwrites itself with a special jump
// instruction that sets the closure and enters
// the function.
Vm(genclo0) {
  ob *ec = (ob*) GF(ip), arg = ec[0];
  size_t adic = nilp(arg) ? 0 : getnum(G(arg));
  Have(Width(sf) + adic + 1);
  ob loc = ec[1];
  sf subd = fp;
  G(ip) = genclo1;
  sp = cpyw(sp - adic, (ob*) arg + 1, adic);
  ec = (ob*) GF(ip); // XXX p sure this is not needed?
  fp = (sf) sp - 1;
  sp = (ob*) fp;
  fp->retp = ip;
  fp->subd = subd;
  fp->argc = adic;
  fp->clos = (ob*) ec[2];
  if (!nilp(loc)) *--sp = loc;
  return ApY(ec[3], xp); }

// finalize function instance closure
Vm(genclo1) { return
  G(ip) = setclo,
  GF(ip) = (vm*) xp,
  ApY(ip, xp); }

// set the closure for this frame
Vm(setclo) { return
  fp->clos = (ob*) GF(ip),
  ApY(G(FF(ip)), xp); }

// the next few functions create and store
// lexical environments.
static Vm(enclose) {
  size_t
    adic = fp->argc,
    arg_len = adic ? adic + 1 + Width(tag) : 0,
    env_len = 4 + Width(tag),
    thd_len = 3 + Width(tag),
    n = arg_len + env_len + thd_len;
  Have(n);
  ob codeXcons = (ob) GF(ip), // pair of the compiled thread & closure constructor
     arg = nil,
     *block = hp;
  hp += n;

  if (adic)
    ini_mo(block, adic + 1),
    block[0] = putnum(adic),
    cpyw(block + 1, fp->argv, adic),
    arg = (ob) block,
    block += arg_len;

  ob *env = (ob*) ini_mo(block, 4); // holds the closure environment & constructor
  block += env_len;
  ob *thd = (ob*) ini_mo(block, 3), // the thread that actually gets returned
     // TODO get closure out of stack frame; configure via xp
     loc = nilp(xp) ? xp : ((ob*)fp)[-1],
     clo = (ob) fp->clos;

  env[0] = arg;
  env[1] = loc;
  env[2] = clo;
  env[3] = B(codeXcons);

  thd[0] = (ob) genclo0;
  thd[1] = (ob) env;
  thd[2] = A(codeXcons);

  return ApN(2, (ob) thd); }

// these pass the locals array to encl in xp
// TODO do the same thing with the closure ptr
Vm(encl1) { return ApC(enclose, putnum(1)); }
Vm(encl0) { return ApC(enclose, nil); }

// FIXME this is really weird
// print a function name
static long tx_mo_n(la v, FILE *o, ob x) {
  if (symp(x)) {
    if (fputc('\\', o) == EOF) return -1;
    long r = la_tx(v, o, x);
    return r < 0 ? r : r + 1; }
  if (!twop(x))
    return fputc('\\', o) == EOF ? -1 : 1;
  long r = 0, a;
  // FIXME this is weird
  if (symp(A(x)) || twop(A(x))) {
    a = tx_mo_n(v, o, A(x));
    if (a < 0) return a;
    r += a; }
  if (symp(B(x)) || twop(B(x))) {
    a = tx_mo_n(v, o, B(x));
    if (a < 0) return a;
    r += a; }
  return r; }

// try to get the name of a function
static ob hnom(la v, mo x) {
  if (!livep(v, (ob) x)) return nil;
  vm *k = G(x);
  if (k == setclo || k == genclo0 || k == genclo1)
    return hnom(v, (mo) G(FF(x)));
  ob n = ((ob*) button(x))[-1];
  return livep(v, n) ? n : nil; }

long tx_mo(la v, FILE *o, mo x) {
  if (primp((mo) x)) return
    fprintf(o, "\\%s", ((struct prim*)x)->nom);
  return tx_mo_n(v, o, hnom(v, (mo) x)); }

intptr_t hx_mo(la v, mo x) {
  if (!livep(v, (ob) x)) return mix ^ ((ob) x * mix);
  return mix ^ hash(v, hnom(v, x)); }
