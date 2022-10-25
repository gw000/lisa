#include "lisa.h"
#include "vm.h"

// pairs and lists
ob pair(la v, ob a, ob b) {
  two w;
  with(a, with(b, w = cells(v, Width(two))));
  if (!w) return 0;
  w->a = a;
  w->b = b;
  return puttwo(w); }

// length of list
size_t llen(ob l) {
  size_t i = 0;
  while (twop(l)) l = B(l), i++;
  return i; }

Vm(car) { return ApN(1, A(xp)); }
Vm(cdr) { return ApN(1, B(xp)); }

Vm(cons) {
  Have(Width(two) - 1);
  hp[0] = xp;
  hp[1] = *sp++;
  xp = puttwo(hp);
  hp += Width(two);
  return ApN(1, xp); }

Vm(car_u) {
  ArityCheck(1);
  xp = Argv[0];
  Check(twop(xp));
  return ApC(ret, A(xp)); }

Vm(cdr_u) {
  ArityCheck(1);
  xp = Argv[0];
  Check(twop(xp));
  return ApC(ret, B(xp)); }

Vm(cons_u) {
  ArityCheck(2);
  Have(Width(two));
  hp[0] = Argv[0];
  hp[1] = Argv[1];
  xp = puttwo(hp);
  hp += Width(two);
  return ApC(ret, xp); }

Vm(do_two) {
  xp = Argc == putnum(0) ? A(ip) : B(ip);
  return ApC(ret, xp); }

Gc(cp_two) {
  two src = gettwo(x), dst;
  dst = bump(v, Width(two));
  ob src_a = src->a;
  G(src) = (vm*) puttwo(dst);
  dst->b = cp(v, src->b, len0, pool0);
  dst->a = cp(v, src_a, len0, pool0);
  return puttwo(dst); }

void em_two(la v, FILE *o, ob x) {
  for (fputc('(', o);; fputc(' ', o)) {
    tx(v, o, A(x));
    if (!twop(x = B(x))) break; }
  fputc(')', o); }

size_t hash_two(la v, ob x) {
  return ror(hash(v, A(x)) & hash(v, B(x)), 32); }

bool twop(ob _) { return TypeOf(_) == Two; }
