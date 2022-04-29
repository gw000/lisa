#include "lips.h"
#include "terp.h"

// functions for pairs and lists
ob pair(en v, ob a, ob b) {
  two w;
  with(a, with(b, w = cells(v, 2)));
  bind(w, w);
  w->a = a, w->b = b;
  return puttwo(w); }

// pairs
OP1(car, A(xp)) OP1(cdr, B(xp))
Vm(cons) {
  Have1();
  hp[0] = xp;
  hp[1] = *sp++;
  xp = puttwo(hp);
  hp += 2;
  Next(1); }

Vm(car_u) {
  Arity(1);
  TypeCheck(*Argv, Two);
  Go(ret, A(*Argv)); }

Vm(cdr_u) {
  Arity(1);
  TypeCheck(*Argv, Two);
  Go(ret, B(*Argv)); }

Vm(cons_u) {
  Arity(2);
  Have(2);
  two w = (two) hp;
  hp += 2;
  w->a = Argv[0], w->b = Argv[1];
  Go(ret, puttwo(w)); }